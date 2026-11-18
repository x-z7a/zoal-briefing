#ifndef ZOAL_BRIEFING_SIMBRIEF_CLIENT_HPP
#define ZOAL_BRIEFING_SIMBRIEF_CLIENT_HPP

#include "zoal_briefing/master/data_types/flight_plan.hpp"
#include <string>
#include <optional>
#include <thread>
#include <atomic>

namespace zoal_briefing::simbrief {

// Detects when SimBrief loads a new flight plan and retrieves the data.
class Client {
public:
    explicit Client(const std::string& simbrief_api_key,
                    const std::string& simbrief_base_url);

    // Monitor SimBrief state changes (datarefs + file watcher).
    void start_monitoring();
    
    // Stop monitoring.
    void stop_monitoring();
    
    // Fetch the current flight plan data.
    std::optional<Fp::FlightPlan> fetch_flight_plan();
    
    // Check if a new flight plan has been loaded.
    bool has_new_plan() const;

private:
    std::string api_key_;
    std::string base_url_;
    std::atomic<bool> monitoring_{false};
    std::optional<Fp::FlightPlan> cached_plan_;
};

} // namespace zoal_briefing::simbrief

#endif // ZOAL_BRIEFING_SIMBRIEF_CLIENT_HPP
