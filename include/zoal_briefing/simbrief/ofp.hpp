#ifndef ZOAL_BRIEFING_SIMBRIEF_OFP_HPP
#define ZOAL_BRIEFING_SIMBRIEF_OFP_HPP

#include "zoal_briefing/data_types/flight_plan.hpp"

#include <optional>
#include <string>

namespace zoal_briefing::simbrief {

// Maps SimBrief's OFP JSON (xml.fetcher.php?json=1) to a FlightPlan.
// SimBrief converts its XML to JSON, so numbers arrive as strings, empty
// values as {} and a single alternate or fix as an object instead of an array;
// the parser accepts all of that. On failure returns nullopt and says why:
// invalid JSON, or SimBrief's own "fetch.status" ("Error: Unknown UserID").
std::optional<fp::FlightPlan> parse_ofp(const std::string &json_text, std::string &error);

} // namespace zoal_briefing::simbrief

#endif // ZOAL_BRIEFING_SIMBRIEF_OFP_HPP
