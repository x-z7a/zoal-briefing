#ifndef ZOAL_BRIEFING_SIMBRIEF_CLIENT_HPP
#define ZOAL_BRIEFING_SIMBRIEF_CLIENT_HPP

#include "zoal_briefing/master/data_types/flight_plan.hpp"
#include <string>
#include <optional>
#include <thread>
#include <atomic>
#include <mutex>

namespace zoal_briefing::simbrief {

// Detects when SimBrief loads a new flight plan and retrieves the data.
// Supports both API polling (.pln file watcher) and HTTP API for 
// fetching detailed flight plans from simbrief.com.
class Client {
public:
    explicit Client(const std::string& simbrief_api_key = "",
                    const std::string& simbrief_base_url = "https://api.simbrief.com");

    // Monitor SimBrief state changes (datarefs + file watcher).
    void start_monitoring();
    
    // Stop monitoring.
    void stop_monitoring();
    
    // Fetch the current flight plan data (from file or API).
    std::optional<Fp::FlightPlan> fetch_flight_plan();
    
    // Check if a new flight plan has been loaded since last check.
    bool has_new_plan() const;
    
    // Trigger weather refresh for the given route/airport.
    void trigger_weather_refresh(const std::string& icao, 
                                  double lat = 37.0,
                                  double lon = -122.0);

    // Convenience accessors for current cached plan.
    std::string current_origin() const;
    std::string current_destination() const;

private:
    // Internal change detection logic.
    bool detect_change();
    
    std::string api_key_;
    std::string base_url_;
    std::atomic<bool> monitoring_{false};
    std::optional<Fp::FlightPlan> cached_plan_;
    mutable std::mutex fp_mutex_;
    bool cache_update_complete_ = false;
};

} // namespace zoal_briefing::simbrief

#endif // ZOAL_BRIEFING_SIMBRIEF_CLIENT_HPP
