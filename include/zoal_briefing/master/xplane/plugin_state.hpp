#ifndef ZOAL_BRIEFING_PLUGIN_STATE_HPP
#define ZOAL_BRIEFING_PLUGIN_STATE_HPP

#include <string>
#include <mutex>

namespace zoal_briefing::xplane {

// Holds plugin-wide runtime state: current airport, active briefing text, etc.
class PluginState {
public:
    // Current ICAO code being briefed (from SimBrief or pilot position).
    std::string current_icao() const;
    void set_current_icao(const std::string& icao);

    // Active briefing text currently being spoken.
    std::string active_briefing() const;
    void set_active_briefing(const std::string& briefing);

    // Whether the LLM bridge is available (health check passed).
    bool llm_available() const;
    void set_llm_available(bool available);

private:
    std::string current_icao_;
    std::string active_briefing_;
    bool llm_available_{true};
    mutable std::mutex mutex_;
};

// Global singleton.
PluginState& get_plugin_state();

} // namespace zoal_briefing::xplane

#endif // ZOAL_BRIEFING_PLUGIN_STATE_HPP
