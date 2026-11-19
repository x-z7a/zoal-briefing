#ifndef ZOAL_BRIEFING_GUI_PANEL_SESSION_HPP
#define ZOAL_BRIEFING_GUI_PANEL_SESSION_HPP

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace zoal_briefing::gui {

// Plugin status mirrors the state machine's perspective.
enum class PluginStatus {
  IDLE = 0,
  WAKE_DETECTED,
  LISTENING,
  STT_COMPLETE,
  PARSING,
  LLM_ROUTING,
  BRIEFING_GENERATING,
  PLAYBACK,
  COUNT
};

// The current briefing context.
struct PanelBriefing {
  std::string icao;
  std::string route_summary;
  std::string weather_summary;
};

class PanelSession {
public:
  void set_status(PluginStatus status);
  PluginStatus status() const;

  void set_connected(bool connected);
  bool connected() const;

  void set_listening(bool listening);
  bool listening() const;

  void set_airport(const std::string &airport);
  const std::string &airport() const;

  void set_briefing_text(const std::string &text);
  const std::string &briefing_text() const;

  void set_theme(std::string theme_name);
  std::string theme() const;

  // Settings persistence.
  void load_settings();
  void save_settings();

  void set_wake_word(const std::string &word);
  std::string wake_word() const;

  // One line on the audio pipeline ("listening for \"hey jarvis\"").
  void set_audio_status(const std::string &status) { audio_status_ = status; }
  const std::string &audio_status() const { return audio_status_; }

  // zoal-briefing.json in X-Plane's preferences; shared with the audio config.
  static std::string settings_path();
  // Where that file lives; the plugin passes X-Plane's preferences folder.
  static void set_settings_dir(const std::string &dir);

  void set_llm_endpoint(const std::string &endpoint);
  std::string llm_endpoint() const;

  void set_model_name(const std::string &model);
  std::string model_name() const;

  // SimBrief username or pilot ID ("simbrief": {"pilot": ...} in the file).
  // The field edits simbrief_pilot(); the plugin acts on saved_simbrief_pilot(),
  // the value as of the last load or save.
  void set_simbrief_pilot(const std::string &pilot) { simbrief_pilot_ = pilot; }
  const std::string &simbrief_pilot() const { return simbrief_pilot_; }
  const std::string &saved_simbrief_pilot() const { return saved_simbrief_pilot_; }

  // The Refresh button: the plugin takes the request once.
  void request_simbrief_refresh() { simbrief_refresh_requested_ = true; }
  bool take_simbrief_refresh() {
    const bool requested = simbrief_refresh_requested_;
    simbrief_refresh_requested_ = false;
    return requested;
  }

  // Lines the plugin fills from the briefing service.
  void set_flight_plan_status(const std::string &s) { flight_plan_status_ = s; }
  const std::string &flight_plan_status() const { return flight_plan_status_; }
  void set_briefings_status(const std::string &s) { briefings_status_ = s; }
  const std::string &briefings_status() const { return briefings_status_; }
  void set_last_heard(const std::string &s) { last_heard_ = s; }
  const std::string &last_heard() const { return last_heard_; }

  // While a reply is spoken: what was said so far and the sentence playing
  // now. The briefing box follows along; empty when nothing is being said.
  void set_speaking(const std::string &spoken, const std::string &current) {
    speaking_spoken_ = spoken;
    speaking_current_ = current;
  }
  const std::string &speaking_spoken() const { return speaking_spoken_; }
  const std::string &speaking_current() const { return speaking_current_; }

  // Connection / bridge state — needed by panel_view.
  bool reachable() const { return connected_; }

  bool connection_loaded() const { return settings_loaded_; }
  bool settings_loaded() const { return settings_loaded_; }

  void set_reachable(bool reachable);

private:
  PluginStatus status_ = PluginStatus::IDLE;
  bool connected_ = false;
  bool listening_ = false;
  bool settings_loaded_ = false;
  std::string airport_;
  std::string briefing_text_;
  std::string theme_name_ = "graphite";
  std::string wake_word_ = "hey jarvis";
  std::string audio_status_;
  std::string llm_endpoint_;
  std::string model_name_;
  std::string simbrief_pilot_;
  std::string saved_simbrief_pilot_;
  bool simbrief_refresh_requested_ = false;
  std::string flight_plan_status_;
  std::string briefings_status_;
  std::string last_heard_;
  std::string speaking_spoken_;
  std::string speaking_current_;
};

} // namespace zoal_briefing::gui

#endif // ZOAL_BRIEFING_GUI_PANEL_SESSION_HPP
