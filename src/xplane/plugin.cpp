#include "gui/panel_window.hpp"
#include "zoal_briefing/gui/panel_session.hpp"
#include "zoal_briefing/log.hpp"
#include "zoal_briefing/main_thread_queue.hpp"
#include "zoal_briefing/state_machine/state_machine.hpp"
#include "zoal_briefing/xplane/plugin_state.hpp"

#ifdef ZOAL_BRIEFING_HAS_AUDIO
#include "audio/output/xplane_bus.hpp"
#include "zoal_briefing/audio/config.hpp"
#include "zoal_briefing/audio/voice_pipeline.hpp"
#include <nlohmann/json.hpp>
#endif

#include <XPLMDataAccess.h>
#include <XPLMDisplay.h>
#include <XPLMPlugin.h>
#include <XPLMProcessing.h>
#include <XPLMUtilities.h>

#include <atomic>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>

namespace zb = zoal_briefing;
using zb::state::State;
using zoal_briefing::xplane::get_plugin_state;

namespace {

std::atomic<bool> g_plugin_initialized{false};

// Everything worker threads need done on the main thread: log lines, and
// starting/stopping sounds.
zb::MainThreadQueue g_main_queue;
zb::state::StateMachine g_machine;

XPLMCommandRef g_listen_command = nullptr;
XPLMCommandRef g_cancel_command = nullptr;

#ifdef ZOAL_BRIEFING_HAS_AUDIO
std::unique_ptr<zb::audio::XPlaneBus> g_audio_output;
std::unique_ptr<zb::audio::VoicePipeline> g_voice;
#endif

// The plugin's folder: <plugins>/zoal-briefing/<platform>/zoal-briefing.xpl.
std::filesystem::path plugin_dir() {
  char binary[1024] = {};
  XPLMGetPluginInfo(XPLMGetMyID(), nullptr, binary, nullptr, nullptr);
  return std::filesystem::path(binary).parent_path().parent_path();
}

// X-Plane's Output/preferences folder.
std::string prefs_dir() {
  char prefs[1024] = {};
  XPLMGetPrefsPath(prefs);
  return std::filesystem::path(prefs).parent_path().string();
}

zb::gui::PluginStatus panel_status(State s) {
  using zb::gui::PluginStatus;
  switch (s) {
  case State::IDLE: return PluginStatus::IDLE;
  case State::WAKE_DETECTED: return PluginStatus::WAKE_DETECTED;
  case State::LISTENING: return PluginStatus::LISTENING;
  case State::STT_COMPLETE: return PluginStatus::STT_COMPLETE;
  case State::PARSING:
  case State::DATA_READY: return PluginStatus::PARSING;
  case State::LLM_ROUTING: return PluginStatus::LLM_ROUTING;
  case State::BRIEFING_GENERATING: return PluginStatus::BRIEFING_GENERATING;
  case State::PLAYBACK: return PluginStatus::PLAYBACK;
  case State::STATE_COUNT: break;
  }
  return PluginStatus::IDLE;
}

void start_voice() {
#ifdef ZOAL_BRIEFING_HAS_AUDIO
  if (g_voice) return;
  nlohmann::json settings = nlohmann::json::object();
  {
    std::ifstream in(zb::gui::PanelSession::settings_path());
    if (in.is_open()) settings = nlohmann::json::parse(in, nullptr, false);
  }
  zb::audio::AudioConfig config = zb::audio::audio_config_from_json(settings);
  const std::string models_dir =
      config.models_dir.empty() ? (plugin_dir() / "models").string() : config.models_dir;

  g_audio_output = std::make_unique<zb::audio::XPlaneBus>(
      g_main_queue, zb::audio::XPlaneBus::parse_bus(config.output_bus));
  g_voice = std::make_unique<zb::audio::VoicePipeline>(*g_audio_output, g_machine);
  zb::log::info("plugin", "wake word \"" + config.wake_word + "\", speech on the " +
                              config.output_bus + " bus");
  g_voice->start(config, models_dir);
#else
  zb::log::warn("plugin", "built without the audio pipeline (ZoalBriefing_WITH_AUDIO=OFF)");
#endif
}

void stop_voice() {
#ifdef ZOAL_BRIEFING_HAS_AUDIO
  if (g_voice) g_voice->stop();
  g_main_queue.drain(); // run the stop it posted
  if (g_audio_output) g_audio_output->shutdown();
  g_voice.reset();
  g_audio_output.reset();
#endif
}

// Shows the pipeline's state on the panel, once a frame.
void update_panel() {
  zb::gui::PanelSession *session = zb::gui::panel_session();
  if (!session) return;
  const State s = g_machine.current();
  session->set_status(panel_status(s));
  session->set_listening(s == State::LISTENING);
#ifdef ZOAL_BRIEFING_HAS_AUDIO
  if (g_voice) {
    session->set_audio_status(g_voice->status());
    const std::string heard = g_voice->last_transcript();
    if (!heard.empty() && session->briefing_text() != "Heard: " + heard) {
      session->set_briefing_text("Heard: " + heard);
    }
  } else {
    session->set_audio_status("off");
  }
#else
  session->set_audio_status("not built");
#endif
}

int listen_handler(XPLMCommandRef, XPLMCommandPhase phase, void *) {
#ifdef ZOAL_BRIEFING_HAS_AUDIO
  if (phase == xplm_CommandBegin && g_voice) g_voice->request_listen();
#else
  (void)phase;
#endif
  return 1;
}

int cancel_handler(XPLMCommandRef, XPLMCommandPhase phase, void *) {
#ifdef ZOAL_BRIEFING_HAS_AUDIO
  if (phase == xplm_CommandBegin && g_voice) g_voice->request_cancel();
#else
  (void)phase;
#endif
  return 1;
}

// Every frame: run main-thread work, then the panel.
float flight_loop(float, float, int, void *) {
  if (!g_plugin_initialized.load()) return -1.0f;
  g_main_queue.drain();
  update_panel();
  zb::gui::tick_panel();
  return -1.0f;
}

} // namespace

PLUGIN_API int XPluginStart(char *outName, char *outSignature, char *outPayload) {
  // Worker threads must not call XPLMDebugString: every log line goes through
  // the main-thread queue to Log.txt.
  zb::log::set_sink([](const std::string &line) {
    g_main_queue.post([line] {
      XPLMDebugString(line.c_str());
      XPLMDebugString("\n");
    });
  });
  zb::log::info("plugin", "loading...");

  // POSIX paths from the SDK (as zoal-atc): without this a Mac gets HFS paths
  // ("Macintosh HD:Users:...") and the panel cannot open its fonts.
  XPLMEnableFeature("XPLM_USE_NATIVE_PATHS", 1);

  zb::gui::PanelSession::set_settings_dir(prefs_dir());

  auto &state = get_plugin_state();
  state.set_current_icao("KSFO");
  state.set_llm_available(true);

  XPLMRegisterFlightLoopCallback(flight_loop, -1.0f, nullptr);

  g_listen_command = XPLMCreateCommand("zoal_briefing/listen",
                                       "zoal-briefing: Listen for a request (skip the wake word)");
  g_cancel_command = XPLMCreateCommand("zoal_briefing/cancel",
                                       "zoal-briefing: Stop listening or speaking");
  XPLMRegisterCommandHandler(g_listen_command, listen_handler, 1, nullptr);
  XPLMRegisterCommandHandler(g_cancel_command, cancel_handler, 1, nullptr);

  zb::gui::PanelConfig config;
  config.plugin_version = "1.0.0";
  zb::gui::start_panel(config);

  g_plugin_initialized.store(true);
  zb::log::info("plugin", "loaded; settings in " + zb::gui::PanelSession::settings_path());

  if (outName) std::strncpy(outName, "zoal-briefing", 255);
  if (outSignature) std::strncpy(outSignature, "ZBRIEF", 255);
  if (outPayload) std::strncpy(outPayload, "v1.0.0", 255);
  return 1;
}

PLUGIN_API void XPluginStop() {
  zb::log::info("plugin", "unloading...");
  g_plugin_initialized.store(false);
  stop_voice();
  XPLMUnregisterFlightLoopCallback(flight_loop, nullptr);
  XPLMUnregisterCommandHandler(g_listen_command, listen_handler, 1, nullptr);
  XPLMUnregisterCommandHandler(g_cancel_command, cancel_handler, 1, nullptr);
  zb::gui::stop_panel();
  zb::log::info("plugin", "unloaded");
  g_main_queue.drain();
  zb::log::reset_sink();
}

// The microphone is open only while the plugin is enabled.
PLUGIN_API int XPluginEnable(void) {
  zb::log::info("plugin", "enabled");
  start_voice();
  return 1;
}

PLUGIN_API void XPluginDisable(void) {
  stop_voice();
  zb::log::info("plugin", "disabled");
}

PLUGIN_API void XPluginReceiveMessage(XPLMPluginID inFrom, int message, void *) {
  if (inFrom == XPLM_PLUGIN_XPLANE && message == XPLM_MSG_PLANE_LOADED) {
    zb::log::info("plugin", "new flight loaded");
  }
}
