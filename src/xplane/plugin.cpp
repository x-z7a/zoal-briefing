#include "gui/panel_window.hpp"
#include "zoal_briefing/gui/panel_session.hpp"
#include "zoal_briefing/llm_bridge/briefing_generator.hpp"
#include "zoal_briefing/simbrief/plan_watcher.hpp"
#include "zoal_briefing/util/http_client.hpp"
#include "zoal_briefing/xplane/briefing_responder.hpp"
#include "zoal_briefing/log.hpp"
#include "zoal_briefing/main_thread_queue.hpp"
#include "zoal_briefing/state_machine/state_machine.hpp"
#include "zoal_briefing/xplane/plugin_state.hpp"

#ifdef ZOAL_BRIEFING_HAS_AUDIO
#include "audio/output/xplane_bus.hpp"
#include "zoal_briefing/audio/config.hpp"
#include "zoal_briefing/audio/voice_pipeline.hpp"
#endif

#include <nlohmann/json.hpp>

#include <XPLMDataAccess.h>
#include <XPLMDisplay.h>
#include <XPLMPlugin.h>
#include <XPLMProcessing.h>
#include <XPLMUtilities.h>

#include <algorithm>
#include <atomic>
#include <cstdint>
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
XPLMCommandRef g_refresh_command = nullptr;

// The SimBrief plan, the briefings generated for it, and the voice loop's
// answer from both. The watcher hands each new plan to the generator.
std::unique_ptr<zb::util::HttpClient> g_http;
std::unique_ptr<zb::simbrief::PlanWatcher> g_plans;
std::unique_ptr<zb::llm_bridge::BriefingGenerator> g_briefings;
std::unique_ptr<zb::xplane::BriefingResponder> g_responder;
// What the panel last showed of them.
std::uint64_t g_plans_version = 0;
std::uint64_t g_briefings_version = 0;
std::string g_applied_pilot;

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

nlohmann::json read_settings() {
  std::ifstream in(zb::gui::PanelSession::settings_path());
  if (!in.is_open()) return nlohmann::json::object();
  nlohmann::json settings = nlohmann::json::parse(in, nullptr, false);
  return settings.is_object() ? settings : nlohmann::json::object();
}

void start_briefings() {
  if (g_plans) return;
  const nlohmann::json settings = read_settings();
  const nlohmann::json simbrief = settings.value("simbrief", nlohmann::json::object());
  const std::filesystem::path prefs(prefs_dir());

  zb::simbrief::WatcherConfig watch;
  try {
    if (simbrief.is_object()) {
      watch.pilot = simbrief.value("pilot", std::string());
      // Never faster than SimBrief should be polled.
      watch.poll_seconds = std::max(60.0, simbrief.value("poll_seconds", 120.0));
    }
  } catch (const nlohmann::json::exception &e) {
    zb::log::warn("plugin", std::string("bad \"simbrief\" settings: ") + e.what());
  }
  watch.cache_path = (prefs / "zoal-briefing-ofp.json").string();

  zb::llm_bridge::GeneratorConfig generate;
  generate.cache_path = (prefs / "zoal-briefing-briefings.json").string();

  g_http = zb::util::make_http_client();
  g_briefings = std::make_unique<zb::llm_bridge::BriefingGenerator>(*g_http, generate);
  zb::llm_bridge::BriefingGenerator *briefings = g_briefings.get();
  g_plans = std::make_unique<zb::simbrief::PlanWatcher>(
      *g_http, watch, [briefings](const zb::fp::FlightPlan &plan) { briefings->set_plan(plan); });
  g_responder = std::make_unique<zb::xplane::BriefingResponder>(*g_plans, *g_briefings);
  g_applied_pilot = watch.pilot;
  g_plans_version = g_briefings_version = 0;
  g_briefings->start();
  g_plans->start();
}

void stop_briefings() {
  if (g_plans) g_plans->stop(); // first: it feeds the generator
  if (g_briefings) g_briefings->stop();
  g_responder.reset();
  g_plans.reset();
  g_briefings.reset();
  g_http.reset();
}

// Check SimBrief now, and retry briefings that failed.
void refresh_plan(const std::string &why) {
  if (g_plans) g_plans->refresh(why);
  if (g_briefings) g_briefings->retry_failed();
}

void start_voice() {
#ifdef ZOAL_BRIEFING_HAS_AUDIO
  if (g_voice) return;
  const nlohmann::json settings = read_settings();
  zb::audio::AudioConfig config = zb::audio::audio_config_from_json(settings);
  const std::string models_dir =
      config.models_dir.empty() ? (plugin_dir() / "models").string() : config.models_dir;

  g_audio_output = std::make_unique<zb::audio::XPlaneBus>(
      g_main_queue, zb::audio::XPlaneBus::parse_bus(config.output_bus));
  // Any request is answered with the departure briefing (no intent parser yet).
  g_voice = std::make_unique<zb::audio::VoicePipeline>(*g_audio_output, g_machine,
                                                       g_responder.get());
  zb::log::info("plugin", "wake word \"" + config.wake_word + "\", speech on the " +
                              config.output_bus + " bus");
  if (g_plans) {
    const auto plan = g_plans->status().plan;
    if (plan) g_voice->set_flight(plan->request_id);
  }
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

std::string briefing_label(const zb::llm_bridge::Briefing &b) {
  std::string s = std::string(zb::llm_bridge::briefing_title(b.kind)) + ": " +
                  zb::llm_bridge::to_string(b.status);
  if (b.status == zb::llm_bridge::BriefingStatus::FAILED && !b.error.empty()) {
    s += " (" + b.error + ")";
  }
  return s;
}

// The panel's view of the plan and its briefings, rebuilt only when they changed.
void update_briefings_panel(zb::gui::PanelSession &session) {
  if (!g_plans || !g_briefings) {
    session.set_flight_plan_status("off");
    return;
  }
  // The pilot acts on Save, not on every keystroke.
  if (session.settings_loaded() && session.saved_simbrief_pilot() != g_applied_pilot) {
    g_applied_pilot = session.saved_simbrief_pilot();
    g_plans->set_pilot(g_applied_pilot);
  }
  if (session.take_simbrief_refresh()) refresh_plan("panel Refresh button");

  const std::uint64_t plans_version = g_plans->version();
  const std::uint64_t briefings_version = g_briefings->version();
  if (plans_version == g_plans_version && briefings_version == g_briefings_version) return;
  g_plans_version = plans_version;
  g_briefings_version = briefings_version;

  const zb::simbrief::WatchStatus plan = g_plans->status();
#ifdef ZOAL_BRIEFING_HAS_AUDIO
  // One copilot per flight: a new plan is a new flight, and a new voice.
  if (g_voice && plan.plan) g_voice->set_flight(plan.plan->request_id);
#endif
  session.set_flight_plan_status(plan.fetching ? plan.text + " (checking...)" : plan.text);
  if (!plan.plan) {
    session.set_briefings_status("waiting for a flight plan");
    session.set_briefing_text("");
    return;
  }
  std::string status;
  std::string text;
  for (const auto &b : g_briefings->briefings()) {
    if (!status.empty()) status += '\n';
    status += briefing_label(b);
    if (b.status == zb::llm_bridge::BriefingStatus::READY) {
      if (!text.empty()) text += "\n\n";
      text += std::string(zb::llm_bridge::briefing_title(b.kind)) + "\n" + b.text;
    }
  }
  session.set_briefings_status(status);
  session.set_briefing_text(text);
}

// Shows the pipeline's state on the panel, once a frame.
void update_panel() {
  zb::gui::PanelSession *session = zb::gui::panel_session();
  if (!session) return;
  const State s = g_machine.current();
  session->set_status(panel_status(s));
  session->set_listening(s == State::LISTENING);
  update_briefings_panel(*session);
#ifdef ZOAL_BRIEFING_HAS_AUDIO
  if (g_voice) {
    session->set_audio_status(g_voice->status());
    session->set_last_heard(g_voice->last_transcript());
    if (s == State::PLAYBACK) {
      const zb::audio::SpeechProgress speech = g_voice->speech_progress();
      session->set_speaking(speech.spoken, speech.current);
    } else {
      session->set_speaking("", "");
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

int refresh_handler(XPLMCommandRef, XPLMCommandPhase phase, void *) {
  if (phase == xplm_CommandBegin) refresh_plan("refresh command");
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
  g_refresh_command = XPLMCreateCommand("zoal_briefing/refresh_simbrief",
                                        "zoal-briefing: Check SimBrief for a new flight plan");
  XPLMRegisterCommandHandler(g_refresh_command, refresh_handler, 1, nullptr);

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
  stop_briefings();
  XPLMUnregisterFlightLoopCallback(flight_loop, nullptr);
  XPLMUnregisterCommandHandler(g_listen_command, listen_handler, 1, nullptr);
  XPLMUnregisterCommandHandler(g_cancel_command, cancel_handler, 1, nullptr);
  XPLMUnregisterCommandHandler(g_refresh_command, refresh_handler, 1, nullptr);
  zb::gui::stop_panel();
  zb::log::info("plugin", "unloaded");
  g_main_queue.drain();
  zb::log::reset_sink();
}

// The microphone is open only while the plugin is enabled.
PLUGIN_API int XPluginEnable(void) {
  zb::log::info("plugin", "enabled");
  start_briefings(); // before the voice loop, which answers through g_responder
  start_voice();
  return 1;
}

PLUGIN_API void XPluginDisable(void) {
  stop_voice(); // first: it holds g_responder
  stop_briefings();
  zb::log::info("plugin", "disabled");
}

PLUGIN_API void XPluginReceiveMessage(XPLMPluginID inFrom, int message, void *) {
  if (inFrom == XPLM_PLUGIN_XPLANE && message == XPLM_MSG_PLANE_LOADED) {
    zb::log::info("plugin", "new flight loaded");
    refresh_plan("new flight loaded");
  }
}
