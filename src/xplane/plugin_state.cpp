#include "xplane_logger.h"
#include "zoal_briefing/xplane/plugin_state.hpp"
#include <mutex>

namespace zoal_briefing::xplane {

PluginState& get_plugin_state() {
  static PluginState instance;
  xplane_debug_log("get_plugin_state() — singleton retrieved");
  return instance;
}

std::string PluginState::current_icao() const {
  std::lock_guard<std::mutex> lock(mutex_);
  std::string val = current_icao_;
#ifdef ZOAL_BRIEFING_USE_XPLANE_SDK
  if (!val.empty()) {
    xplane_debug_log(("current_icao = " + val).c_str());
  }
#endif
  return val;
}

void PluginState::set_current_icao(const std::string& icao) {
  std::lock_guard<std::mutex> lock(mutex_);
  current_icao_ = icao;
#ifdef ZOAL_BRIEFING_USE_XPLANE_SDK
  xplane_debug_log(("set_current_icao = " + icao).c_str());
#endif
}

std::string PluginState::active_briefing() const {
  std::lock_guard<std::mutex> lock(mutex_);
  std::string val = active_briefing_;
#ifdef ZOAL_BRIEFING_USE_XPLANE_SDK
  if (!val.empty()) {
    xplane_debug_log(("active_briefing (len=" + std::to_string(val.size()) + ")").c_str());
  }
#endif
  return val;
}

void PluginState::set_active_briefing(const std::string& briefing) {
  std::lock_guard<std::mutex> lock(mutex_);
  active_briefing_ = briefing;
#ifdef ZOAL_BRIEFING_USE_XPLANE_SDK
  xplane_debug_log(("set_active_briefing (len=" + std::to_string(briefing.size()) + ")").c_str());
#endif
}

bool PluginState::llm_available() const {
  bool val = llm_available_.load();
#ifdef ZOAL_BRIEFING_USE_XPLANE_SDK
  xplane_debug_log(("llm_available = " + std::string(val ? "true" : "false")).c_str());
#endif
  return val;
}

void PluginState::set_llm_available(bool available) {
  llm_available_.store(available);
#ifdef ZOAL_BRIEFING_USE_XPLANE_SDK
  xplane_debug_log(("set_llm_available = " + std::string(available ? "true" : "false")).c_str());
#endif
}

std::string PluginState::origin_icao() const {
  std::lock_guard<std::mutex> lock(mutex_);
  std::string val = origin_icao_;
#ifdef ZOAL_BRIEFING_USE_XPLANE_SDK
  if (!val.empty()) {
    xplane_debug_log(("origin_icao = " + val).c_str());
  }
#endif
  return val;
}

void PluginState::set_origin_icao(const std::string& icao) {
  std::lock_guard<std::mutex> lock(mutex_);
  origin_icao_ = icao;
#ifdef ZOAL_BRIEFING_USE_XPLANE_SDK
  xplane_debug_log(("set_origin_icao = " + icao).c_str());
#endif
}

std::string PluginState::dest_icao() const {
  std::lock_guard<std::mutex> lock(mutex_);
  std::string val = dest_icao_;
#ifdef ZOAL_BRIEFING_USE_XPLANE_SDK
  if (!val.empty()) {
    xplane_debug_log(("dest_icao = " + val).c_str());
  }
#endif
  return val;
}

void PluginState::set_dest_icao(const std::string& icao) {
  std::lock_guard<std::mutex> lock(mutex_);
  dest_icao_ = icao;
#ifdef ZOAL_BRIEFING_USE_XPLANE_SDK
  xplane_debug_log(("set_dest_icao = " + icao).c_str());
#endif
}

bool PluginState::is_ready() const {
  bool val = is_ready_.load();
#ifdef ZOAL_BRIEFING_USE_XPLANE_SDK
  xplane_debug_log(("is_ready = " + std::string(val ? "true" : "false")).c_str());
#endif
  return val;
}

void PluginState::set_ready(bool ready) {
  is_ready_.store(ready);
#ifdef ZOAL_BRIEFING_USE_XPLANE_SDK
  xplane_debug_log(("set_ready = " + std::string(ready ? "true" : "false")).c_str());
#endif
}

} // namespace zoal_briefing::xplane
