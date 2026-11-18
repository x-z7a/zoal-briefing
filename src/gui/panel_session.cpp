#include "zoal_briefing/gui/panel_session.hpp"

#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace zoal_briefing::gui {

static std::string settings_path() {
  // Write to X-Plane's config directory: ~/Library/Application Support/X-Plane/Resources/Plugins/zoal-briefing.json
  // In production this would use XPLMGetSystemPath to find the right location.
  // For now, fall back to a local file for testing.
  return "zoal-briefing.json";
}

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

void PanelSession::load_settings() {
  // Read settings from zoal-briefing.json if it exists.
  auto path = settings_path();
  std::ifstream file(path);
  if (file.is_open()) {
    try {
      json j;
      file >> j;
      
      if (j.contains("wake_word")) wake_word_ = j["wake_word"].get<std::string>();
      if (j.contains("llm_endpoint")) llm_endpoint_ = j["llm_endpoint"].get<std::string>();
      if (j.contains("model_name")) model_name_ = j["model_name"].get<std::string>();
      if (j.contains("theme")) theme_name_ = j["theme"].get<std::string>();
      
      std::cerr << "[panel] settings loaded from " << path << std::endl;
    } catch (const std::exception &e) {
      std::cerr << "[panel] failed to load settings: " << e.what() << std::endl;
    }
  }
}

void PanelSession::save_settings() {
  // Write settings to zoal-briefing.json using nlohmann/json.
  auto path = settings_path();
  
  json j;
  j["wake_word"] = wake_word_;
  j["llm_endpoint"] = llm_endpoint_;
  j["model_name"] = model_name_;
  j["theme"] = theme_name_;
  
  std::ofstream file(path);
  if (file.is_open()) {
    file << j.dump(2); // Pretty print with 2-space indent
    file.close();
    std::cerr << "[panel] settings saved to " << path << std::endl;
  } else {
    std::cerr << "[panel] WARNING: could not write settings to " << path << std::endl;
  }
}

void PanelSession::on_action(const std::string &action, const std::string &payload) {
  if (action == "save_settings") {
    // Parse the payload as JSON and update settings.
    try {
      json j = json::parse(payload);
      if (j.contains("wake_word")) wake_word_ = j["wake_word"].get<std::string>();
      if (j.contains("llm_endpoint")) llm_endpoint_ = j["llm_endpoint"].get<std::string>();
      if (j.contains("model_name")) model_name_ = j["model_name"].get<std::string>();
      if (j.contains("theme")) theme_name_ = j["theme"].get<std::string>();
      save_settings();
    } catch (...) {
      // Silent ignore for invalid JSON.
    }
  } else if (action == "save_theme") {
    theme_name_ = payload;
    save_settings();
  }
}

} // namespace zoal_briefing::gui
