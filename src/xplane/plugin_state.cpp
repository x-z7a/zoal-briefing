#include "zoal_briefing/xplane/plugin_state.hpp"
#include <mutex>

namespace zoal_briefing::xplane {

PluginState& get_plugin_state() {
  static PluginState instance;
  return instance;
}

std::string PluginState::current_icao() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return current_icao_;
}

void PluginState::set_current_icao(const std::string& icao) {
  std::lock_guard<std::mutex> lock(mutex_);
  current_icao_ = icao;
}

std::string PluginState::active_briefing() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return active_briefing_;
}

void PluginState::set_active_briefing(const std::string& briefing) {
  std::lock_guard<std::mutex> lock(mutex_);
  active_briefing_ = briefing;
}

bool PluginState::llm_available() const {
  return llm_available_.load();
}

void PluginState::set_llm_available(bool available) {
  llm_available_.store(available);
}

std::string PluginState::origin_icao() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return origin_icao_;
}

void PluginState::set_origin_icao(const std::string& icao) {
  std::lock_guard<std::mutex> lock(mutex_);
  origin_icao_ = icao;
}

std::string PluginState::dest_icao() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return dest_icao_;
}

void PluginState::set_dest_icao(const std::string& icao) {
  std::lock_guard<std::mutex> lock(mutex_);
  dest_icao_ = icao;
}

bool PluginState::is_ready() const {
  return is_ready_.load();
}

void PluginState::set_ready(bool ready) {
  is_ready_.store(ready);
}

} // namespace zoal_briefing::xplane
