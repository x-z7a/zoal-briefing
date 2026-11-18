#include "zoal_briefing/master/xplane/plugin_state.hpp"

namespace zoal_briefing::xplane {

std::string PluginState::current_icao() const { return current_icao_; }
void PluginState::set_current_icao(const std::string& icao) {
    std::lock_guard<std::mutex> lock(mutex_);
    current_icao_ = icao;
}

std::string PluginState::active_briefing() const { return active_briefing_; }
void PluginState::set_active_briefing(const std::string& briefing) {
    std::lock_guard<std::mutex> lock(mutex_);
    active_briefing_ = briefing;
}

bool PluginState::llm_available() const { return llm_available_; }
void PluginState::set_llm_available(bool available) {
    std::lock_guard<std::mutex> lock(mutex_);
    llm_available_ = available;
}

PluginState& get_plugin_state() {
    static PluginState instance;
    return instance;
}

} // namespace zoal_briefing::xplane
