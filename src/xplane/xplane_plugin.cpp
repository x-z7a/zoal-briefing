#include "panel_window.hpp"
#include "zoal_briefing/xplane/plugin_state.hpp"
#include "zoal_briefing/gui/panel_session.hpp"

#include <XPLMDataAccess.h>
#include <XPLMDisplay.h>
#include <XPLMPlugin.h>
#include <XPLMProcessing.h>
#include <XPLMSound.h>
#include <XPLMUtilities.h>

#include <atomic>
#include <chrono>
#include <iostream>
#include <string>

using zoal_briefing::xplane::get_plugin_state;

constexpr const char* kLogPrefix = "[zoal-briefing] ";
static std::atomic<bool> g_plugin_initialized{false};

// Flight loop callback — runs every ~1s while the sim is active.
static float flight_loop(float, float, int, void*) {
    if (!g_plugin_initialized.load()) return 1.0f;
    
    // Tick the panel window (drains ImGui actions).
    zoal_briefing::xplane::tick_panel();
    return 1.0f;
}

PLUGIN_API int XPluginStart(char* outName, char* outSignature, char* outPayload) {
    XPLMDebugString("[zoal-briefing] loading...\n");

    auto& state = get_plugin_state();
    state.set_current_icao("KSFO");
    state.set_llm_available(true);

    // Register flight loop using the modern XPLMProcessing.h API.
    XPLMRegisterFlightLoopCallback(flight_loop, 1.0f, nullptr);
    XPLMDebugString("[zoal-briefing] flight loop registered\n");

    // Initialize the panel window with simple config.
    zoal_briefing::xplane::PanelConfig config;
    config.plugin_version = "1.0.0";
    zoal_briefing::xplane::start_panel(config);
    
    g_plugin_initialized.store(true);

    XPLMDebugString("[zoal-briefing] loaded\n");

    if (outName) strncpy(outName, "zoal-briefing", 255);
    if (outSignature) strncpy(outSignature, "ZBRIEF", 255);
    if (outPayload) strncpy(outPayload, "v1.0.0", 255);

    return 1;
}

PLUGIN_API void XPluginStop() {
    XPLMDebugString("[zoal-briefing] unloading...\n");
    
    g_plugin_initialized.store(false);
    zoal_briefing::xplane::stop_panel();
    XPLMDebugString("[zoal-briefing] unloaded\n");
}

PLUGIN_API int XPluginEnable(void) { 
    XPLMDebugString("[zoal-briefing] enabled\n");
    return 1; 
}

PLUGIN_API void XPluginDisable(void) {
    XPLMDebugString("[zoal-briefing] disabled\n");
}

PLUGIN_API void XPluginReceiveMessage(XPLMPluginID inFrom, int message, void* param) {
    (void)inFrom; (void)message; (void)param;
    if (inFrom == XPLM_PLUGIN_XPLANE) {
        if (message == XPLM_MSG_PLANE_LOADED) {
            XPLMDebugString("[zoal-briefing] new flight loaded\n");
        }
    }
}
