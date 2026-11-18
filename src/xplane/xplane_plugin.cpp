#include "zoal_briefing/master/state_machine/state_machine.hpp"
#include "zoal_briefing/master/cache_builder/apt_dat_parser.hpp"
#include "zoal_briefing/master/intent_parser/intent_parser.hpp"
#include "zoal_briefing/master/llm_bridge/llm_bridge.hpp"
#include "zoal_briefing/master/audio_pipeline/wake_word_engine.hpp"
#include "zoal_briefing/master/audio_pipeline/stt_transcriber.hpp"
#include "zoal_briefing/master/audio_pipeline/tts_speaker.hpp"
#include "zoal_briefing/master/xplane/plugin_state.hpp"

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

using zoal_briefing::sm::StateMachine;
using zoal_briefing::sm::State;
using zoal_briefing::intent::IntentParser;
using zoal_briefing::llm::LlmBridge;
using zoal_briefing::llm::BridgeConfig;
using zoal_briefing::audio::WakeWordEngine;
using zoal_briefing::audio::SttTranscriber;
using zoal_briefing::audio::TtsSpeaker;
using zoal_briefing::xplane::get_plugin_state;

constexpr const char *kCommandName = "zoal_briefing/trigger";
constexpr const char *kLogPrefix = "[zoal-briefing] ";

// Plugin globals.
XPLMCommandRef g_trigger_command = nullptr;
StateMachine g_state_machine;
std::unique_ptr<IntentParser> g_intent_parser;
std::unique_ptr<LlmBridge> g_llm_bridge;
std::unique_ptr<WakeWordEngine> g_wake_word;
std::unique_ptr<SttTranscriber> g_stt;
std::unique_ptr<TtsSpeaker> g_tts;

// Audio buffer for wake word / STT pipeline.
constexpr int kAudioBufferSize = 32000; // ~1 second at 16kHz
std::vector<std::int16_t> g_audio_buffer(kAudioBufferSize);
bool g_recording_active = false;
XPLMDataRef g_com1_dataref = nullptr;

// Audio channel for TTS playback (X-Plane com system).
static XPLMAudio2ID g_playback_channel = nullptr;

void log_line(const std::string& msg) {
    XPLMDebugString((kLogPrefix + msg + "\n").c_str());
}

// Play briefing audio through X-Plane's com system.
void play_audio_chunk(const std::vector<std::int16_t>& pcm, int sample_rate_hz) {
    // TODO: Convert PCM to X-Plane-compatible sound format.
    // Use XPLMPlaySoundFile2 or equivalent to play through the selected com radio.
    (void)pcm;
    (void)sample_rate_hz;
}

// Main flight loop callback — processes audio frames and drives state transitions.
static float g_last_audio_time = 0.0f;

bool flight_loop_callback(double current, double delta, double sinceLast,
                          int feedback, void *param) {
    (void)current;
    (void)delta;
    (void)sinceLast;
    (void)feedback;
    (void)param;

    State state = g_state_machine.current_state();

    switch (state) {
        case State::IDLE:
            // Check wake word from audio buffer.
            if (g_wake_word && g_wake_word->detect(g_audio_buffer)) {
                g_state_machine.transition_to(State::WAKE_DETECTED);
                log_line("wake word detected");
            }
            break;

        case State::WAKE_DETECTED:
            // Transition to listening.
            g_state_machine.transition_to(State::LISTENING);
            g_recording_active = true;
            log_line("listening...");
            break;

        case State::LISTENING:
            // In production, this would pull audio from the mic recorder.
            // For now, just check for a timeout / end-of-speech trigger.
            if (current - g_last_audio_time > 5.0) {
                g_recording_active = false;
                g_state_machine.transition_to(State::STT_COMPLETE);
                log_line("audio capture complete");
            }
            break;

        case State::STT_COMPLETE:
            // Transcribe audio buffer.
            if (g_stt && g_stt->is_speaking()) {
                auto transcript = g_stt->transcribe(g_audio_buffer);
                if (transcript.has_value() && !transcript->empty()) {
                    log_line("transcribed: " + transcript.value());
                    
                    // Parse intent.
                    auto intent = g_intent_parser->parse(transcript.value());
                    if (intent.confirmed) {
                        log_line("intent: briefing for " 
                                 + (intent.airport ? *intent.airport : "unknown"));
                        g_state_machine.transition_to(State::PARSING);
                    }
                } else {
                    log_line("no speech detected, returning to idle");
                    g_state_machine.transition_to(State::IDLE);
                }
            }
            break;

        case State::PARSING:
            // Parse intent → route to data readiness check.
            g_state_machine.transition_to(State::DATA_READY);
            log_line("data ready, generating briefing...");
            break;

        case State::DATA_READY:
            // LLM bridge generates the structured briefing text.
            if (g_llm_bridge) {
                std::string system_prompt = 
                    "You are an aviation briefing officer. Provide concise, "
                    "structured flight briefings for X-Plane pilots.";
                
                std::string user_input = "Generate a departure briefing";
                
                g_state_machine.transition_to(State::LLM_ROUTING);
                log_line("LLM routing...");
            }
            break;

        case State::LLM_ROUTING:
        case State::BRIEFING_GENERATING:
            // In production, this is async via the LLM bridge callback.
            // Once the LLM returns text, transition to PLAYBACK.
            g_state_machine.transition_to(State::PLAYBACK);
            break;

        case State::PLAYBACK:
            // Speak the briefing through Piper TTS → X-Plane com mixer.
            if (g_tts) {
                // TODO: Queue the briefing text for Piper generation.
                log_line("briefing speaking...");
            }
            g_state_machine.transition_to(State::IDLE);
            break;

        default:
            break;
    }

    return 1.0f; // Continue calling this flight loop.
}

// Command handler for manual trigger (fallback to wake word).
static bool ptt_command_handler(XPLMCommandRef, XPLMCommandCallbackType, 
                                 void *, float *) {
    State state = g_state_machine.current_state();
    if (state == State::IDLE) {
        g_state_machine.transition_to(State::LISTENING);
        log_line("manual trigger: listening...");
    }
    return 1;
}

// Plugin entry point — X-Plane will call this.
PLUGIN_API int XPluginStart() {
    log_line("BriefingMaster loading...");

    // Initialize components.
    g_intent_parser = std::make_unique<IntentParser>();
    
    // LLM bridge config — would be read from briefingmaster.json in production.
    BridgeConfig config;
    config.mode = BridgeConfig::Mode::AUTO;
    config.api.provider = "ollama";
    config.api.endpoint = "http://localhost:11434/v1/chat/completions";
    config.api.model = "qwen3.6:35b-a3b";
    g_llm_bridge = std::make_unique<LlmBridge>(config);

    // Audio pipeline (placeholders — real implementations need model files).
    g_wake_word = std::make_unique<WakeWordEngine>("models/cookie_monster.wav");
    g_stt = std::make_unique<SttTranscriber>("models/ggml-small.en.bin");
    g_tts = std::make_unique<TtsSpeaker>("models/piper-en_US-lessac-medium");

    // Register command for manual trigger (Shift+B fallback).
    g_trigger_command = XPLMCreateCommand(kCommandName, 
                                          "zoal-briefing: Trigger briefing");
    XPLMRegisterCommandHandler(g_trigger_command, ptt_command_handler, 1, nullptr);

    // Setup flight loop.
    XPLMFlightLoopID flight_loop;
    XPLMCreateFlightLoop_t params{};
    params.structSize = sizeof(params);
    params.phase = xplm_FlightLoop_Phase_AfterFlightModel;
    params.callbackFunc = flight_loop_callback;
    flight_loop = XPLMCreateFlightLoop(&params);
    XPLMScheduleFlightLoop(flight_loop, -1.0f, true);

    // Note: State machine is configured via callbacks in the flight loop.
    log_line("BriefingMaster loaded; bind command zoal_briefing/trigger");
    return 1;
}

PLUGIN_API void XPluginStop() {
    if (g_trigger_command) {
        XPLMUnregisterCommandHandler(g_trigger_command, ptt_command_handler, 1, nullptr);
        g_trigger_command = nullptr;
    }
    log_line("BriefingMaster unloaded");
}

PLUGIN_API int XPluginEnable() { return 1; }
PLUGIN_API void XPluginDisable() {}

PLUGIN_API void XPluginReceiveMessage(XPLMPluginID in_from, int message, 
                                       void *param) {
    if (in_from != XPLM_PLUGIN_XPLANE) return;
    // Handle X-Plane messages.
    (void)message;
    (void)param;
}
