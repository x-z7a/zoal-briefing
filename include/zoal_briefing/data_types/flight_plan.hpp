#ifndef ZOAL_BRIEFING_FLIGHT_PLAN_HPP
#define ZOAL_BRIEFING_FLIGHT_PLAN_HPP

#include <cstdint>
#include <string>
#include <vector>

namespace zoal_briefing::fp {

struct Airport {
  std::string icao;
  std::string iata;
  std::string name;
  int elevation_ft = 0;
  std::string planned_runway;
  int transition_altitude_ft = 0;
  int transition_level_ft = 0;
  std::string metar;
  std::string taf;
};

// One navlog row.
struct Fix {
  std::string ident;
  std::string name;
  std::string type;       // wpt, vor, ndb, apt, ltlg
  std::string via;        // airway, or the SID/STAR for procedure fixes
  bool procedure = false; // part of a SID or STAR
  std::string stage;      // CLB, CRZ, DSC
  int altitude_ft = 0;
  double leg_nm = 0; // distance from the previous fix
  int wind_dir = 0;
  int wind_kt = 0;
};

// In the OFP's units (Fuel/Weights::units, "kgs" or "lbs").
struct Fuel {
  std::string units;
  double taxi = 0;
  double trip = 0;
  double contingency = 0;
  double alternate = 0;
  double reserve = 0;
  double extra = 0;
  double min_takeoff = 0;
  double plan_takeoff = 0;
  double plan_ramp = 0;
  double plan_landing = 0;
  double avg_flow_per_hour = 0; // average burn, units per hour
};

struct Weights {
  std::string units;
  double zfw = 0, max_zfw = 0;
  double tow = 0, max_tow = 0;
  double ldw = 0, max_ldw = 0;
  int passengers = 0;
};

// A SimBrief OFP, mapped from its JSON (simbrief::parse_ofp).
struct FlightPlan {
  // SimBrief's id for this OFP: a different id is a new plan.
  std::string request_id;
  std::int64_t generated_at = 0; // unix seconds
  std::string airac;

  std::string callsign;
  std::string aircraft_icao;
  std::string aircraft_name;
  std::string registration;

  Airport origin;
  Airport destination;
  std::vector<Airport> alternates;

  std::string route; // without the SID/STAR
  std::string sid, sid_transition;
  std::string star, star_transition;
  int initial_altitude_ft = 0;
  std::string step_climbs;
  std::string cost_index;
  int route_distance_nm = 0;
  int air_distance_nm = 0;
  int avg_wind_component_kt = 0; // negative is a headwind
  int ete_seconds = 0;
  std::int64_t sched_out = 0; // unix seconds, 0 when not in the plan

  Fuel fuel;
  Weights weights;
  std::vector<Fix> navlog;
};

} // namespace zoal_briefing::fp

#endif // ZOAL_BRIEFING_FLIGHT_PLAN_HPP
