#include "zoal_briefing/state_machine/state_machine.hpp"
#include "zoal_briefing/llm_bridge/llm_bridge.hpp"
#include "zoal_briefing/data_types/weather.hpp"
#include <iostream>

// Placeholder for X-Plane SDK headers (XPLMPlugging.h, XPLMDataAccess.h, etc.)
// In a real build, these are included via the X-Plane SDK path.

using namespace zoal_briefing::master::state;
using namespace zoal_briefing::bridge;

static StateMachine sm;
static LLMRouter router;

void on_state_change(State from, State to) {
    std::cout << "[State] " << static_cast<int>(from) << " -> " << static_cast<int>(to) << std::endl;
    
    // Transition logic hook
    if (to == State::DATA_READY) {
        // Trigger weather summary aggregation & LLM routing
        // In production: fetch METAR/TAF from NOAA/AvW, run summarizer, call router.generate_briefing()
        std::string sys_prompt = "You are an expert IBCT weather forecaster.";
        std::string ctx = "METAR: KORD 171951Z 27015G25KT 10SM FEW045 SCT250 23/13 A3002";
        router.generate_briefing(sys_prompt, ctx, [](bool ok, const std::string& text) {
            if (ok) {
                std::cout << "[BRIEFING OUTPUT] " << text << std::endl;
                // Trigger Piper TTS: speak(text);
            }
        });
    } else if (to == State::PLAYBACK) {
        std::cout << "[Audio] Playing briefing via X-Plane COM system..." << std::endl;
    } else if (to == State::IDLE) {
        std::cout << "[System] Back to IDLE, listening for wake word 'Cookie Monster'." << std::endl;
    }
}

void xPluginStart() {
    std::cout << ">>> zoal_briefing plugin loaded. Waiting for 'Cookie Monster'..." << std::endl;
    
    // Load config
    router.load_config("briefingmaster.json");
    
    // Register state machine callbacks
    sm.on_transition(on_state_change);
    
    // Initial transition
    sm.transition_to(State::IDLE);
}

void xPluginStop() {
    std::cout << ">>> zoal_briefing plugin unloaded." << std::endl;
}

void xPluginBehaviorLoop(double elapsed_sec, double elapsed_msec) {
    // State machine heartbeat / audio loop / SimBrief trigger checks happen here
    // e.g., if (wake_word_detected && sm.current() == IDLE) sm.transition_to(State::LISTENING);
}

int xPluginWeight(const char* pluginId) {
    return 100;
}

extern "C" XPLMPLUGIN_EXPORT int xPluginDescribe(char* outName, char* outSignature, char* outPayload) {
    strcpy(outName, "zoal_briefing");
    strcpy(outSignature, "zoal-briefing-v1");
    strcpy(outPayload, "Voice-driven weather/briefing plugin");
    return 0;
}

extern "C" XPLMPLUGIN_EXPORT int xPluginStart(char* outName, char* outSignature, char* outPayload) {
    return xPluginDescribe(outName, outSignature, outPayload);
}
