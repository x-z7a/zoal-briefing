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
#include <filesystem>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

using zoal_briefing::xplane::get_plugin_state;
using json = nlohmann::json;

constexpr const char* kLogPrefix = "[zoal-briefing] ";
static std::atomic<bool> g_plugin_initialized{false};

// Settings file path — stores in X-Plane's user data folder.
static std::string settings_path() {
    char system_path[1024] = {};
    XPLMGetSystemPath(system_path);
    return std::string(system_path) + "/Preferences/zoal-briefing.cfg";
}

// Logging helper.
static void log_msg(const std::string& msg) {
    XPLMDebugString((kLogPrefix + msg + "\n").c_str());
}

// Flight loop callback — runs every frame while the sim is active.
// Signature matches XPLMFlightLoop_f: (float elapsed, float elapsed_sim, int counter, void* refcon)
static float flight_loop(float, float, int, void*) {
    if (!g_plugin_initialized.load()) return 1.0f;
    
    // Tick the panel window (drains ImGui actions).
    zoal_briefing::xplane::tick_panel();
    
    return 1.0f;
}

PLUGIN_API int XPluginStart(char* outName, char* outSignature, char* outPayload) {
    log_msg("zoal-briefing loading...");

    auto& state = get_plugin_state();
    state.set_current_icao("KSFO");
    
    // Load settings from disk.
    state.load_settings();
    log_msg("settings loaded");

    // Register flight loop using the modern XPLMProcessing.h API.
    float interval = 0.0f;
    int registered = XPLMRegisterFlightLoopCallback(flight_loop, 0.0f, nullptr);
    
    if (registered) {
        log_msg("flight loop registered");
    } else {
        log_msg("WARNING: flight loop registration failed");
    }

    // Initialize the panel window.
    zoal_briefing::xplane::PanelConfig config;
    config.plugin_version = "1.0.0";
    zoal_briefing::xplane::start_panel(config);
    
    g_plugin_initialized.store(true);
    state.set_llm_available(true);

    log_msg("zoal-briefing loaded");

    if (outName) strncpy(outName, "zoal-briefing", 255);
    if (outSignature) strncpy(outSignature, "ZBRIEF", 255);
    if (outPayload) strncpy(outPayload, "v1.0.0", 255);

    return 1;
}

PLUGIN_API void XPluginStop() {
    log_msg("zoal-briefing unloading...");
    
    auto& state = get_plugin_state();
    state.save_settings();
    log_msg("settings saved");

    g_plugin_initialized.store(false);
    
    // Stop the panel window.
    zoal_briefing::xplane::stop_panel();

    log_msg("zoal-briefing unloaded");
}

PLUGIN_API int XPluginEnable(void) { 
    log_msg("zoal-briefing enabled");
    return 1; 
}

PLUGIN_API void XPluginDisable(void) {
    log_msg("zoal-briefing disabled");
}

PLUGIN_API void XPluginReceiveMessage(XPLMPluginID inFrom, int message, void* param) {
    (void)inFrom; (void)message; (void)param;
    if (inFrom == XPLM_PLUGIN_XPLANE) {
        if (message == XPLM_MSG_PLANE_LOADED) {
            log_msg("new flight loaded");
        }
    }
}
