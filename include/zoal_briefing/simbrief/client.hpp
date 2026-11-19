#ifndef ZOAL_BRIEFING_SIMBRIEF_CLIENT_HPP
#define ZOAL_BRIEFING_SIMBRIEF_CLIENT_HPP

#include "zoal_briefing/data_types/flight_plan.hpp"
#include "zoal_briefing/util/http_client.hpp"

#include <functional>
#include <optional>
#include <string>

namespace zoal_briefing::simbrief {

// The latest-OFP URL for a pilot: all digits is a SimBrief pilot ID
// (userid=), anything else a username. Empty when the pilot is empty.
std::string latest_ofp_url(const std::string &pilot);

struct FetchResult {
  std::optional<fp::FlightPlan> plan;
  std::string raw; // the OFP JSON as received, for the on-disk cache
  std::string error;
};

// Fetches and parses the pilot's latest OFP. Blocking: worker threads only.
// No API key is needed for this endpoint.
FetchResult fetch_latest(util::HttpClient &http, const std::string &pilot,
                         std::function<bool()> cancelled = {});

} // namespace zoal_briefing::simbrief

#endif // ZOAL_BRIEFING_SIMBRIEF_CLIENT_HPP
