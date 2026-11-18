#ifndef ZOAL_BRIEFING_PLUGIN_STATE_HPP
#define ZOAL_BRIEFING_PLUGIN_STATE_HPP

#include <string>
#include <mutex>
#include <atomic>

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

    // Current flight plan info.
    std::string origin_icao() const;
    void set_origin_icao(const std::string& icao);
    
    std::string dest_icao() const;
    void set_dest_icao(const std::string& icao);

    // Plugin lifecycle state.
    bool is_ready() const;
    void set_ready(bool ready);

private:
    mutable std::mutex mutex_;
    std::string current_icao_;
    std::string active_briefing_;
    std::string origin_icao_;
    std::string dest_icao_;
    std::atomic<bool> llm_available_{true};
    std::atomic<bool> is_ready_{false};
};

// Global singleton.
PluginState& get_plugin_state();

} // namespace zoal_briefing::xplane

#endif // ZOAL_BRIEFING_PLUGIN_STATE_HPP
