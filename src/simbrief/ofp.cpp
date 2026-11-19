#include "zoal_briefing/simbrief/ofp.hpp"

#include <nlohmann/json.hpp>

#include <cmath>
#include <cstdlib>

namespace zoal_briefing::simbrief {

using json = nlohmann::json;

namespace {

// The value at key as text: SimBrief's strings, numbers printed, and "" for
// anything else (missing keys, {} placeholders).
std::string str(const json &j, const char *key) {
  if (!j.is_object()) return {};
  const auto it = j.find(key);
  if (it == j.end()) return {};
  if (it->is_string()) return it->get<std::string>();
  if (it->is_number_integer()) return std::to_string(it->get<long long>());
  if (it->is_number()) return it->dump();
  return {};
}

double num(const json &j, const char *key) {
  const std::string s = str(j, key);
  if (s.empty()) return 0.0;
  char *end = nullptr;
  const double v = std::strtod(s.c_str(), &end);
  return end == s.c_str() ? 0.0 : v;
}

int integer(const json &j, const char *key) { return static_cast<int>(std::lround(num(j, key))); }

const json &child(const json &j, const char *key) {
  static const json kEmpty = json::object();
  if (!j.is_object()) return kEmpty;
  const auto it = j.find(key);
  return it == j.end() ? kEmpty : *it;
}

// An array, or a lone object as a one-element array.
std::vector<const json *> list(const json &j) {
  std::vector<const json *> out;
  if (j.is_array()) {
    for (const auto &e : j) out.push_back(&e);
  } else if (j.is_object() && !j.empty()) {
    out.push_back(&j);
  }
  return out;
}

fp::Airport airport(const json &j) {
  fp::Airport a;
  a.icao = str(j, "icao_code");
  a.iata = str(j, "iata_code");
  a.name = str(j, "name");
  a.elevation_ft = integer(j, "elevation");
  a.planned_runway = str(j, "plan_rwy");
  a.transition_altitude_ft = integer(j, "trans_alt");
  a.transition_level_ft = integer(j, "trans_level");
  a.metar = str(j, "metar");
  a.taf = str(j, "taf");
  return a;
}

} // namespace

std::optional<fp::FlightPlan> parse_ofp(const std::string &json_text, std::string &error) {
  const json root = json::parse(json_text, nullptr, false);
  if (root.is_discarded() || !root.is_object()) {
    error = "SimBrief did not return JSON";
    return std::nullopt;
  }
  const std::string status = str(child(root, "fetch"), "status");
  if (!status.empty() && status != "Success") {
    error = "SimBrief: " + status;
    return std::nullopt;
  }

  const json &params = child(root, "params");
  const json &general = child(root, "general");
  fp::FlightPlan plan;
  plan.request_id = str(params, "request_id");
  plan.generated_at = static_cast<std::int64_t>(num(params, "time_generated"));
  plan.airac = str(params, "airac");

  plan.callsign = str(child(root, "atc"), "callsign");
  if (plan.callsign.empty()) {
    plan.callsign = str(general, "icao_airline") + str(general, "flight_number");
  }
  const json &aircraft = child(root, "aircraft");
  plan.aircraft_icao = str(aircraft, "icaocode");
  if (plan.aircraft_icao.empty()) plan.aircraft_icao = str(aircraft, "icao_code");
  plan.aircraft_name = str(aircraft, "name");
  plan.registration = str(aircraft, "reg");

  plan.origin = airport(child(root, "origin"));
  plan.destination = airport(child(root, "destination"));
  for (const json *alt : list(child(root, "alternate"))) {
    plan.alternates.push_back(airport(*alt));
  }

  plan.route = str(general, "route");
  plan.sid = str(general, "sid_ident");
  plan.sid_transition = str(general, "sid_trans");
  plan.star = str(general, "star_ident");
  plan.star_transition = str(general, "star_trans");
  plan.initial_altitude_ft = integer(general, "initial_altitude");
  plan.step_climbs = str(general, "stepclimb_string");
  plan.cost_index = str(general, "costindex");
  plan.route_distance_nm = integer(general, "route_distance");
  plan.air_distance_nm = integer(general, "air_distance");
  plan.avg_wind_component_kt = integer(general, "avg_wind_comp");

  const json &times = child(root, "times");
  plan.ete_seconds = integer(times, "est_time_enroute");
  plan.sched_out = static_cast<std::int64_t>(num(times, "sched_out"));

  const std::string units = str(params, "units");
  const json &fuel = child(root, "fuel");
  plan.fuel.units = units;
  plan.fuel.taxi = num(fuel, "taxi");
  plan.fuel.trip = num(fuel, "enroute_burn");
  plan.fuel.contingency = num(fuel, "contingency");
  plan.fuel.alternate = num(fuel, "alternate_burn");
  plan.fuel.reserve = num(fuel, "reserve");
  plan.fuel.extra = num(fuel, "extra");
  plan.fuel.min_takeoff = num(fuel, "min_takeoff");
  plan.fuel.plan_takeoff = num(fuel, "plan_takeoff");
  plan.fuel.plan_ramp = num(fuel, "plan_ramp");
  plan.fuel.plan_landing = num(fuel, "plan_landing");
  plan.fuel.avg_flow_per_hour = num(fuel, "avg_fuel_flow");

  const json &weights = child(root, "weights");
  plan.weights.units = units;
  plan.weights.zfw = num(weights, "est_zfw");
  plan.weights.max_zfw = num(weights, "max_zfw");
  plan.weights.tow = num(weights, "est_tow");
  plan.weights.max_tow = num(weights, "max_tow");
  plan.weights.ldw = num(weights, "est_ldw");
  plan.weights.max_ldw = num(weights, "max_ldw");
  plan.weights.passengers = integer(weights, "pax_count");

  for (const json *f : list(child(child(root, "navlog"), "fix"))) {
    fp::Fix fix;
    fix.ident = str(*f, "ident");
    fix.name = str(*f, "name");
    fix.type = str(*f, "type");
    fix.via = str(*f, "via_airway");
    fix.procedure = str(*f, "is_sid_star") == "1";
    fix.stage = str(*f, "stage");
    fix.altitude_ft = integer(*f, "altitude_feet");
    fix.leg_nm = num(*f, "distance");
    fix.wind_dir = integer(*f, "wind_dir");
    fix.wind_kt = integer(*f, "wind_spd");
    plan.navlog.push_back(std::move(fix));
  }

  // Older OFPs have no sid_ident/star_ident: the procedure fixes name them.
  const bool need_sid = plan.sid.empty();
  const bool need_star = plan.star.empty();
  for (const auto &fix : plan.navlog) {
    if (!fix.procedure || fix.via.empty()) continue;
    if (need_sid && fix.stage == "CLB" && plan.sid.empty()) plan.sid = fix.via;
    if (need_star && fix.stage == "DSC") plan.star = fix.via; // the last one wins
  }

  if (plan.origin.icao.empty() || plan.destination.icao.empty()) {
    error = "SimBrief OFP has no origin or destination";
    return std::nullopt;
  }
  return plan;
}

} // namespace zoal_briefing::simbrief
