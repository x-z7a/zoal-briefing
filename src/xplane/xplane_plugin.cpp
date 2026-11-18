#include "xplane_logger.h"
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
using zoal_briefing::xplane::xplane_debug_log;

constexpr const char* kLogPrefix = "[zoal-briefing] ";
static std::atomic<bool> g_plugin_initialized{false};

// Flight loop callback — runs every frame so the panel's presses and drags
// are handled without lag.
static float flight_loop(float, float, int, void*) {
    if (!g_plugin_initialized.load()) return -1.0f;
    
    // Tick the panel window (drains ImGui actions, moves a dragged panel).
    zoal_briefing::xplane::tick_panel();
    return -1.0f;
}

PLUGIN_API int XPluginStart(char* outName, char* outSignature, char* outPayload) {
    xplane_debug_log("loading...");

    // POSIX paths from the SDK (as zoal-atc): without this a Mac gets HFS paths
    // ("Macintosh HD:Users:...") and the panel cannot open its fonts.
    XPLMEnableFeature("XPLM_USE_NATIVE_PATHS", 1);

    auto& state = get_plugin_state();
    state.set_current_icao("KSFO");
    state.set_llm_available(true);

    // Register flight loop using the modern XPLMProcessing.h API.
    XPLMRegisterFlightLoopCallback(flight_loop, -1.0f, nullptr);
    xplane_debug_log("flight loop registered");

    // Initialize the panel window with simple config.
    zoal_briefing::xplane::PanelConfig config;
    config.plugin_version = "1.0.0";
    zoal_briefing::xplane::start_panel(config);
    
    g_plugin_initialized.store(true);

    xplane_debug_log("loaded");

    if (outName) strncpy(outName, "zoal-briefing", 255);
    if (outSignature) strncpy(outSignature, "ZBRIEF", 255);
    if (outPayload) strncpy(outPayload, "v1.0.0", 255);

    return 1;
}

PLUGIN_API void XPluginStop() {
    xplane_debug_log("unloading...");
    
    g_plugin_initialized.store(false);
    XPLMUnregisterFlightLoopCallback(flight_loop, nullptr);
    zoal_briefing::xplane::stop_panel();
    xplane_debug_log("unloaded");
}

PLUGIN_API int XPluginEnable(void) { 
    xplane_debug_log("enabled");
    return 1; 
}

PLUGIN_API void XPluginDisable(void) {
    xplane_debug_log("disabled");
}

PLUGIN_API void XPluginReceiveMessage(XPLMPluginID inFrom, int message, void* param) {
    (void)inFrom; (void)message; (void)param;
    if (inFrom == XPLM_PLUGIN_XPLANE) {
        if (message == XPLM_MSG_PLANE_LOADED) {
            xplane_debug_log("new flight loaded");
        }
    }
}
