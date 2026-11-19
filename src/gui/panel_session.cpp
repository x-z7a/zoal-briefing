#include "zoal_briefing/gui/panel_session.hpp"

#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <cstdio>
#include <filesystem>

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

static std::string &settings_dir_override() {
  static std::string dir;
  return dir;
}

void PanelSession::set_settings_dir(const std::string &dir) { settings_dir_override() = dir; }

// The plugin sets X-Plane's own Output/preferences folder at start; these are
// fallbacks for the SDK-free builds.
static std::string get_settings_dir() {
  if (!settings_dir_override().empty()) return settings_dir_override();
#if defined(__APPLE__) || defined(__linux__)
  if (const char *home = getenv("HOME")) return std::string(home) + "/.config/zoal-briefing";
#elif defined(_WIN32)
  if (const char *appdata = getenv("APPDATA")) return std::string(appdata) + "\\zoal-briefing";
#endif
  return ".";
}

std::string PanelSession::settings_path() { return get_settings_dir() + "/zoal-briefing.json"; }

void PanelSession::load_settings() {
  std::string path = settings_path();

  std::ifstream file(path);
  if (file.is_open()) {
    try {
      json j;
      file >> j;
      
      if (j.contains("wake_word") && j["wake_word"].is_string() &&
          !j["wake_word"].get<std::string>().empty()) {
        wake_word_ = j["wake_word"].get<std::string>();
      }
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
  std::string path = settings_path();

  std::error_code ec;
  std::filesystem::create_directories(dir, ec);

  // Merge into what is there: the file also holds settings the panel does not
  // edit (the "audio" block), which a fresh object would erase.
  json j = json::object();
  {
    std::ifstream in(path);
    if (in.is_open()) {
      json existing = json::parse(in, nullptr, false);
      if (existing.is_object()) j = std::move(existing);
    }
  }
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
