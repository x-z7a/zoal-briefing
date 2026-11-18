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

// What the console says about the flight.
struct PanelTelemetry {
  double frequency_mhz = 0.0;
  double altitude_ft_msl = 0.0;
  double groundspeed_kts = 0.0;
  bool on_ground = false;
  std::string airport;
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
  void set_wake_word(const std::string &word);
  std::string wake_word() const;
  
  void set_llm_endpoint(const std::string &endpoint);
  std::string llm_endpoint() const;
  
  void set_model_name(const std::string &model);
  std::string model_name() const;
  
  // Connection / bridge state.
  bool connection_loaded() const { return connected_; }
  bool settings_loaded() const { return true; }
  const PanelTelemetry *telemetry() const { return nullptr; }
  
private:
  PluginStatus status_ = PluginStatus::IDLE;
  bool connected_ = false;
  bool listening_ = false;
  std::string airport_;
  std::string briefing_text_;
  std::string theme_name_ = "graphite";
  std::string wake_word_;
  std::string llm_endpoint_;
  std::string model_name_;
};

} // namespace zoal_briefing::gui

#endif // ZOAL_BRIEFING_GUI_PANEL_SESSION_HPP
