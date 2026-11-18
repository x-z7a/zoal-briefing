#include "zoal_briefing/state_machine/state_machine.hpp"
#include "zoal_briefing/cache_builder/apt_dat_parser.hpp"
#include "zoal_briefing/cache_builder/cache_builder.hpp"
#include "zoal_briefing/cache_builder/weather_summarizer.hpp"
#include "zoal_briefing/intent_parser/intent_parser.hpp"
#include "zoal_briefing/llm_bridge/llm_bridge.hpp"
#include "zoal_briefing/llm_bridge/fallback_chain.hpp"
#include "zoal_briefing/simbrief_client/client.hpp"
#include "zoal_briefing/audio_pipeline/wake_word_engine.hpp"
#include "zoal_briefing/audio_pipeline/stt_transcriber.hpp"
#include "zoal_briefing/audio_pipeline/tts_speaker.hpp"
#include "zoal_briefing/xplane/plugin_state.hpp"

#include <XPLMDataAccess.h>
#include <XPLMPlugin.h>
#include <XPLMProcessing.h>
#include <XPLMSound.h>
#include <XPLMUtilities.h>

#include <atomic>
#include <memory>
#include <thread>
#include <vector>
#include <cstring>
#include <iostream>

using zoal_briefing::sm::StateMachine;
using zoal_briefing::sm::State;
using zoal_briefing::intent::IntentParser;
using zoal_briefing::llm::LlmBridge;
using zoal_briefing::llm::BridgeConfig;
using zoal_briefing::llm::FallbackChain;
using zoal_briefing::simbrief::Client as SimBriefClient;
using zoal_briefing::audio::WakeWordEngine;
using zoal_briefing::audio::SttTranscriber;
using zoal_briefing::audio::TtsSpeaker;
using zoal_briefing::xplane::get_plugin_state;

constexpr const char *kCommandName = "zoal_briefing/trigger";
constexpr const char *kLogPrefix = "[zoal-briefing] ";

// ── Plugin Global State ─────────────────────────────────────────────────────
StateMachine g_state_machine;
std::unique_ptr<IntentParser> g_intent_parser;
std::unique_ptr<LlmBridge> g_llm_bridge;
SimBriefClient g_simbrief_client("your_api_key", "https://api.simbrief.com");
std::unique_ptr<WakeWordEngine> g_wake_word;
std::unique_ptr<SttTranscriber> g_stt;
std::unique_ptr<TtsSpeaker> g_tts;

// SimBrief flight plan monitoring.
uint64_t last_flight_plan_hash_ = 0;
static std::atomic<bool> g_briefing_requested{false};
static std::string g_current_icao = "KSFO";

// Datarefs for SimBrief state monitoring.
static XPLMDataRef g_simbrief_state_ref = nullptr;
static XPLMDataRef g_simbrief_plan_hash_ref = nullptr;

// ── Logging Helper ─────────────────────────────────────────────────────────
void log_line(const std::string& msg) {
    XPLMDebugString((kLogPrefix + msg + "\n").c_str());
}

// ── Audio Playback: Play PCM through X-Plane's com system ───────────────────
void play_audio_chunk(const std::vector<std::int16_t>& pcm, int sample_rate_hz) {
    (void)pcm;
    (void)sample_rate_hz;
    // TODO: Use XPLMPlaySoundFile2 or a custom buffer callback.
}

// ── Helper: Build the weather briefing prompt ───────────────────────────────
std::string build_weather_prompt(const std::string& icao) {
    return "Report weather conditions for " + icao + 
           ". Include: Flight Category (VFR/MVFR/IFR/LIFR), Thunderstorm alerts, "
           "SIGMETs/AIRMETs, Precipitation intensity (rain/snow/hail). "
           "If hazardous, recommend diversion to an alternate.";
}

// ── Helper: Build the departure briefing prompt ─────────────────────────────
std::string build_departure_prompt(const std::string& icao) {
    return "Generate a departure briefing for " + icao + 
           ". Prioritize weather first (FLIGHT CATEGORY, THUNDERSTORMS, SIGMETs/AIRMETs).\n"
           "Include: Route summary, METAR, SID/STAR procedures, Fuel notes, Safety notes.";
}

// ── SimBrief Monitoring: Check for flight plan changes ───────────────────────
void monitor_simbrief_changes() {
    // Get the current hash from X-Plane dataref.
    if (g_simbrief_plan_hash_ref && 
        g_simbrief_state_ref &&
        XPLMGetI32(g_simbrief_state_ref) == 1) { // PLAN_LOADED
        auto current_hash = XPLMGetI32(g_simbrief_plan_hash_ref);
        if (current_hash != last_flight_plan_hash_) {
            last_flight_plan_hash_ = current_hash;
            log_line("SimBrief flight plan hash changed: " 
                     + std::to_string(current_hash));
            
            // Fetch the new flight plan and trigger weather refresh.
            auto plan = g_simbrief_client.fetch_flight_plan();
            if (plan.has_value()) {
                log_line("New SimBrief plan: " + plan->origin + " → " 
                         + plan->destination);
                get_plugin_state().set_current_icao(plan->destination);
                
                // Trigger weather refresh for the destination airport.
                g_simbrief_client.trigger_weather_refresh(
                    plan->destination, 37.0, -122.0);
            }
        }
    }
}

// ── Flight Loop Callback: Drives the state machine and audio pipeline ────────
static float g_last_audio_time = 0.0f;

bool flight_loop_callback(double current, double delta, double sinceLast,
                          int feedback, void *param) {
    (void)current;
    (void)delta;
    (void)sinceLast;
    (void)feedback;
    (void)param;

    State state = g_state_machine.current_state();

    // Check SimBrief flight plan changes in IDLE state.
    if (state == State::IDLE && last_flight_plan_hash_ != 0) {
        monitor_simbrief_changes();
    }

    switch (state) {
        case State::IDLE:
            // Check for manual trigger command.
            if (g_briefing_requested.exchange(false)) {
                g_state_machine.transition_to(State::LISTENING);
                log_line("manual trigger: entering LISTENING state");
                
                // Start STT capture in the background thread.
                std::thread([]() {
                    if (g_stt) {
                        std::vector<std::int16_t> buffer(32000);
                        g_stt->transcribe(buffer);
                    }
                }).detach();
            }
            break;

        case State::LISTENING:
            // SimBrief triggers also work here if the pilot says nothing.
            if (g_simbrief_client.has_new_plan()) {
                log_line("SimBrief plan detected during listening");
                g_state_machine.transition_to(State::DATA_READY);
            } else {
                // Check for wake word detection (would set state to LISTENING).
                // For now, timeout after 5 seconds and return to IDLE.
                static float listen_time = 0;
                listen_time += delta;
                if (listen_time > 5.0f) {
                    log_line("Listening timed out, returning to IDLE");
                    g_state_machine.transition_to(State::IDLE);
                    listen_time = 0;
                }
            }
            break;

        case State::STT_COMPLETE:
            // Transcription is done. Parse intent and route.
            log_line("STT complete, parsing intent...");
            g_state_machine.transition_to(State::PARSING);
            break;

        case State::PARSING:
            // Parse intent → determine briefing type and parameters.
            log_line("Parsing intent for " + get_plugin_state().current_icao());
            g_state_machine.transition_to(State::DATA_READY);
            break;

        case State::DATA_READY:
            // LLM bridge generates the structured briefing text.
            {
                auto icao = get_plugin_state().current_icao();
                if (icao.empty()) icao = "KSFO";
                
                std::string system_prompt = build_departure_prompt(icao);
                std::string user_input = 
                    "I need a departure briefing for " + icao;
                
                log_line("DATA_READY: generating briefing for " + icao);
                g_state_machine.transition_to(State::LLM_ROUTING);
            }
            break;

        case State::LLM_ROUTING:
            // Determine API vs CLI mode based on availability.
            if (g_llm_bridge) {
                auto available = g_llm_bridge->is_available();
                log_line("LLM routing: " + std::string(available ? 
                          "AVAILABLE" : "UNAVAILABLE"));
                
                // Trigger the actual LLM generation.
                g_state_machine.transition_to(State::BRIEFING_GENERATING);
            } else {
                log_line("LLM bridge not initialized, using fallback");
                g_state_machine.transition_to(State::BRIEFING_GENERATING);
            }
            break;

        case State::BRIEFING_GENERATING:
            // LLM is generating the briefing. In production, this would be
            // async via the LlmBridge callback. For now, just transition to playback.
            log_line("BRIEFING_GENERATING: waiting for LLM completion...");
            g_state_machine.transition_to(State::PLAYBACK);
            break;

        case State::PLAYBACK:
            // Speak the briefing text through Piper TTS into X-Plane com system.
            {
                auto briefing = get_plugin_state().active_briefing();
                log_line("PLAYBACK: Speaking briefing (" + 
                         std::to_string(briefing.size()) + " chars)");
                
                if (g_tts) {
                    g_tts->speak(briefing);
                }
            }
            // After speaking, return to IDLE and reset.
            get_plugin_state().set_active_briefing("");
            log_line("Briefing complete, returning to IDLE");
            g_state_machine.transition_to(State::IDLE);
            break;

        default:
            break;
    }

    return 1.0f; // Continue calling this flight loop.
}

// ── Command Handler: Manual trigger for briefing (fallback to wake word) ────
static bool ptt_command_handler(XPLMCommandRef cmd, 
                                 XPLMCommandCallbackType cb_type, 
                                 void *caller_data, float *call_floats) {
    (void)cmd;
    (void)cb_type;
    (void)caller_data;
    (void)call_floats;

    State state = g_state_machine.current_state();
    if (state == State::IDLE) {
        g_state_machine.transition_to(State::LISTENING);
        log_line("manual trigger: entering LISTENING state");
        
        // Also trigger the full briefing pipeline.
        std::thread([]() {
            auto icao = get_plugin_state().current_icao();
            if (icao.empty()) icao = "KSFO";
            
            // Get the LLM bridge configured.
            BridgeConfig config;
            config.mode = BridgeConfig::Mode::AUTO;
            config.api.provider = "ollama";
            config.api.endpoint = "http://localhost:11434/v1/chat/completions";
            config.api.model = "qwen3.6:35b-a3b";
            
            LlmBridge bridge(config);
            
            // Track briefing state in plugin state.
            get_plugin_state().set_active_briefing("Generating briefing for " 
                                                   + icao + "...");
            
            // Generate the briefing.
            bridge.generate(
                build_departure_prompt(icao),
                "I need a complete departure/arrival briefing for " + icao,
                [&get_plugin_state](const std::string& result, 
                                    const std::string& error) {
                    if (result.empty()) {
                        log_line("LLM returned empty: " + error);
                        return;
                    }
                    
                    get_plugin_state().set_active_briefing(result);
                    log_line("Generated briefing for " + icao + 
                             " (" + std::to_string(result.size()) + " bytes)");
                }
            );
        }).detach();
    }
    return 1;
}

// ── Plugin Entry Point — X-Plane will call this ─────────────────────────────
PLUGIN_API int XPluginStart(char* outName, char* outSignature, char* outPayload) {
    log_line("zoal-briefing loading...");

    // Initialize components.
    g_intent_parser = std::make_unique<IntentParser>();
    
    // LLM bridge config (would be read from zoal-briefing.json in production).
    BridgeConfig config;
    config.mode = BridgeConfig::Mode::AUTO;
    config.api.provider = "ollama";
    config.api.endpoint = "http://localhost:11434/v1/chat/completions";
    config.api.model = "qwen3.6:35b-a3b";
    g_llm_bridge = std::make_unique<LlmBridge>(config);

    // SimBrief client (monitors flight plan changes).
    g_simbrief_client = SimBriefClient("your_api_key", 
                                        "https://api.simbrief.com");
    
    // Audio pipeline (placeholders — real implementations need model files).
    g_wake_word = std::make_unique<WakeWordEngine>("models/cookie_monster.wav");
    g_stt = std::make_unique<SttTranscriber>("models/ggml-small-en.bin");
    g_tts = std::make_unique<TtsSpeaker>("models/piper-en_US-lessac-medium");

    // Register command for manual trigger (Shift+B fallback).
    XPLMCommandRef cmd = XPLMCreateCommand(
        kCommandName, "zoal-briefing: Trigger briefing");
    XPLMRegisterCommandHandler(cmd, ptt_command_handler, 1, nullptr);

    // Setup flight loop.
    XPLMFlightLoopID flight_loop;
    XPLMCreateFlightLoop_t params{};
    params.structSize = sizeof(params);
    params.phase = xplm_FlightLoop_Phase_AfterFlightModel;
    params.callbackFunc = flight_loop_callback;
    flight_loop = XPLMCreateFlightLoop(&params);
    XPLMScheduleFlightLoop(flight_loop, -1.0f, true);

    // Setup SimBrief dataref monitoring (for flight plan state changes).
    g_simbrief_state_ref = 
        XPLMFindDataRef("sim/flightplan/load_flight_plan_state");
    g_simbrief_plan_hash_ref = 
        XPLMFindDataRef("sim/flightplan/last_flight_plan_hash");

    // Set initial state.
    get_plugin_state().set_current_icao("KSFO");
    
    log_line("zoal-briefing loaded; bind command zoal_briefing/trigger");
    return 1;
}

PLUGIN_API void XPluginStop() {
    log_line("zoal-briefing unloaded");
    
    // Cleanup: stop monitoring, release resources.
    g_simbrief_client.stop_monitoring();
}

PLUGIN_API int XPluginEnable(void) { 
    log_line("zoal-briefing enabled");
    return 1; 
}
PLUGIN_API void XPluginDisable(void) {}

PLUGIN_API void XPluginReceiveMessage(XPLMPluginID in_from, int message, 
                                       void *param) {
    if (in_from != XPLM_PLUGIN_XPLANE) return;
    
    // Handle X-Plane messages.
    switch (message) {
        case XPLM_MSG_PLANE_LOADED:
            log_line("plane loaded — resetting SimBrief monitor");
            last_flight_plan_hash_ = 0;
            break;
        default:
            break;
    }
    (void)param;
}
