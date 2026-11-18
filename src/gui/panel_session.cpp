#include "zoal_briefing/gui/panel_session.hpp"

#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <cstdio>
#include <sys/stat.h>

using json = nlohmann::json;

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
  save_settings();
}

std::string PanelSession::theme() const {
  return theme_name_;
}

void PanelSession::set_wake_word(const std::string &word) {
  wake_word_ = word;
}

std::string PanelSession::wake_word() const {
  return wake_word_;
}

void PanelSession::set_llm_endpoint(const std::string &endpoint) {
  llm_endpoint_ = endpoint;
}

std::string PanelSession::llm_endpoint() const {
  return llm_endpoint_;
}

void PanelSession::set_model_name(const std::string &model) {
  model_name_ = model;
}

std::string PanelSession::model_name() const {
  return model_name_;
}

void PanelSession::set_reachable(bool reachable) {
  connected_ = reachable;
}

static std::string get_settings_dir() {
#ifdef APL
  // macOS: ~/Library/Application Support/X-Plane/Preferences/
  const char *home = getenv("HOME");
  if (home) return std::string(home) + "/Library/Application Support/X-Plane/Preferences";
#endif
#ifdef IBM  
  // Windows: %APPDATA%\X-Plane\Settings
  const char *appdata = getenv("APPDATA");
  if (appdata) return std::string(appdata) + "\\X-Plane\\Settings";
#endif
  // Default fallback: current directory
  return ".";
}

void PanelSession::load_settings() {
  auto dir = get_settings_dir();
  std::string path = dir + "/zoal-briefing.json";
  
  std::ifstream file(path);
  if (file.is_open()) {
    try {
      json j;
      file >> j;
      
      if (j.contains("wake_word")) wake_word_ = j["wake_word"].get<std::string>();
      if (j.contains("llm_endpoint")) llm_endpoint_ = j["llm_endpoint"].get<std::string>();
      if (j.contains("model_name")) model_name_ = j["model_name"].get<std::string>();
      if (j.contains("theme")) theme_name_ = j["theme"].get<std::string>();
      
      settings_loaded_ = true;
    } catch (const std::exception &e) {
      std::cerr << "[panel] failed to load settings: " << e.what() << std::endl;
    }
  } else {
    // No settings file — use defaults.
    settings_loaded_ = true;
  }
}

void PanelSession::save_settings() {
  auto dir = get_settings_dir();
  std::string path = dir + "/zoal-briefing.json";
  
  mkdir(dir.c_str(), 0755);
  
  json j;
  j["wake_word"] = wake_word_;
  j["llm_endpoint"] = llm_endpoint_;
  j["model_name"] = model_name_;
  j["theme"] = theme_name_;
  
  std::ofstream file(path);
  if (file.is_open()) {
    file << j.dump(2);
    file.close();
  } else {
    std::cerr << "[panel] WARNING: could not write settings to " << path << std::endl;
  }
}

} // namespace zoal_briefing::gui
