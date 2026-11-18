#ifndef ZOAL_BRIEFING_FLIGHT_PLAN_HPP
#define ZOAL_BRIEFING_FLIGHT_PLAN_HPP

#include <string>
#include <vector>
#include <optional>
#include <cstdint>

namespace zoal_briefing::fp {

struct Waypoint {
    std::string name;
    double lat{};
    double lon{};
    int altitude_ft{};          // planned altitude at waypoint
};

struct RouteLeg {
    std::string from;
    std::string to;
    std::string type;           // DIRECT, AIRWAY, SID, STAR, etc.
    std::string airway_name;    // e.g., "J91"
};

struct ApproachProcedure {
    std::string runway;
    std::string approach_type;  // ILS, VOR, RNAV, etc.
    std::string name;           // "ILS RWY 25L"
};

// A completed SimBrief flight plan with parsed route, altitude profile, and fuel.
struct FlightPlan {
    std::string aircraft_type;
    std::string origin;
    std::string destination;
    std::string alternate;
    
    std::vector<Waypoint> waypoints;
    std::vector<RouteLeg> legs;
    std::optional<ApproachProcedure> approach;

    double fuel_on_board_kg{};
    double fuel_reserved_kg{};
    int cruise_alt_ft{};
    
    // SimBrief flight ID for tracking changes
    std::string simbrief_flight_id;
    
    // Hash of the complete plan for invalidation detection
    uint64_t plan_hash = 0;
};

} // namespace zoal_briefing::fp

#endif // ZOAL_BRIEFING_FLIGHT_PLAN_HPP
