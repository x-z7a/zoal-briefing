#include "zoal_briefing/master/simbrief_client/client.hpp"

namespace zoal_briefing::simbrief {

Client::Client(const std::string& simbrief_api_key,
               const std::string& simbrief_base_url)
    : api_key_(simbrief_api_key), base_url_(simbrief_base_url) {}

void Client::start_monitoring() {
    monitoring_ = true;
    // TODO: Start file watcher + dataref polling for SimBrief state changes.
}

void Client::stop_monitoring() {
    monitoring_ = false;
    // TODO: Stop file watcher.
}

std::optional<Fp::FlightPlan> Client::fetch_flight_plan() {
    if (!has_new_plan()) return std::nullopt;
    // TODO: Fetch flight plan from SimBrief API or local files.
    return cached_plan_;
}

bool Client::has_new_plan() const {
    // TODO: Check flight plan hash against previous.
    return false;
}

} // namespace zoal_briefing::simbrief
