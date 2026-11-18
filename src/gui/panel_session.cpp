#include "zoal_briefing/gui/panel_session.hpp"

namespace zoal_briefing::gui {

void PanelSession::set_status(PluginStatus status) {
  status_ = status;
}

PluginStatus PanelSession::status() const {
  return status_;
}

void PanelSession::set_connected(bool connected) {
  connected_ = connected;
}

bool PanelSession::connected() const {
  return connected_;
}

void PanelSession::set_listening(bool listening) {
  listening_ = listening;
}

bool PanelSession::listening() const {
  return listening_;
}

void PanelSession::set_airport(const std::string &airport) {
  airport_ = airport;
}

const std::string &PanelSession::airport() const {
  return airport_;
}

void PanelSession::set_briefing_text(const std::string &text) {
  briefing_text_ = text;
}

const std::string &PanelSession::briefing_text() const {
  return briefing_text_;
}

void PanelSession::set_theme(std::string theme_name) {
  theme_name_ = std::move(theme_name);
}

std::string PanelSession::theme() const {
  return theme_name_;
}

void PanelSession::set_wake_word(const std::string &word) {
  wake_word_ = word;
  // Persist to config.
  save_settings();
}

std::string PanelSession::wake_word() const {
  return wake_word_;
}

void PanelSession::set_llm_endpoint(const std::string &endpoint) {
  llm_endpoint_ = endpoint;
  save_settings();
}

std::string PanelSession::llm_endpoint() const {
  return llm_endpoint_;
}

void PanelSession::set_model_name(const std::string &model) {
  model_name_ = model;
  save_settings();
}

std::string PanelSession::model_name() const {
  return model_name_;
}

void PanelSession::save_settings() {
  // Serialize session state to zoal-briefing.json (or new config location).
  // In production this would write to disk for persistence across restarts.
  (void)0; // Stub: actual JSON writing goes here.
}

} // namespace zoal_briefing::gui
