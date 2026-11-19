#include "zoal_briefing/llm_bridge/briefing_prompts.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <ctime>
#include <sstream>

namespace zoal_briefing::llm_bridge {

namespace {

std::string altitude(int feet, int transition_ft = 18000) {
  if (feet <= 0) return {};
  if (feet >= transition_ft) return "FL" + std::to_string(feet / 100);
  return std::to_string(feet) + " ft";
}

// Fuel and weights to the nearest hundred, which is how they are briefed.
std::string mass(double v, const std::string &units) {
  if (v <= 0) return {};
  const long rounded = std::lround(v / 100.0) * 100;
  // SimBrief says "kgs"/"lbs"; the model reads "kg"/"lb" more reliably.
  std::string unit = units;
  if (unit == "kgs") unit = "kg";
  if (unit == "lbs") unit = "lb";
  std::string out = std::to_string(rounded) + (unit.empty() ? "" : " " + unit);
  // The model converts kilograms to tonnes badly; give it the tonnes.
  if (unit == "kg" && rounded >= 1000) {
    char t[32];
    std::snprintf(t, sizeof(t), " (%.1f tonnes)", static_cast<double>(rounded) / 1000.0);
    out += t;
  }
  return out;
}

std::string duration(int seconds) {
  if (seconds <= 0) return {};
  const int minutes = (seconds + 30) / 60;
  return std::to_string(minutes / 60) + " h " + std::to_string(minutes % 60) + " min";
}

std::string utc_time(std::int64_t unix_seconds) {
  if (unix_seconds <= 0) return {};
  const std::time_t t = static_cast<std::time_t>(unix_seconds);
  std::tm tm{};
#ifdef _WIN32
  gmtime_s(&tm, &t);
#else
  gmtime_r(&t, &tm);
#endif
  char buf[16];
  std::strftime(buf, sizeof(buf), "%H%MZ", &tm);
  return buf;
}

class Block {
public:
  // Adds "label: value" when value is not empty.
  void line(const std::string &label, const std::string &value) {
    if (!value.empty()) out_ << label << ": " << value << "\n";
  }
  void heading(const std::string &h) { out_ << "\n" << h << "\n"; }
  std::string str() const { return out_.str(); }

private:
  std::ostringstream out_;
};

std::string airport_line(const fp::Airport &a) {
  std::string s = a.icao;
  if (!a.name.empty()) s += " (" + a.name + ")";
  if (a.elevation_ft != 0) s += ", elevation " + std::to_string(a.elevation_ft) + " ft";
  return s;
}

void airport_block(Block &b, const std::string &role, const fp::Airport &a, bool departure) {
  b.heading(role);
  b.line("Airport", airport_line(a));
  b.line("Planned runway", a.planned_runway);
  if (departure && a.transition_altitude_ft > 0) {
    b.line("Transition altitude", std::to_string(a.transition_altitude_ft) + " ft");
  }
  if (!departure && a.transition_level_ft > 0) {
    b.line("Transition level", "FL" + std::to_string(a.transition_level_ft / 100));
  }
  b.line("METAR", a.metar);
  b.line("TAF", a.taf);
}

// "IDENT (FL350), IDENT (12000 ft), ..." for the fixes that match.
template <typename Pred>
std::string fixes(const fp::FlightPlan &plan, Pred pred, std::size_t max = 12) {
  std::string out;
  std::size_t n = 0;
  for (const auto &f : plan.navlog) {
    if (!pred(f) || f.type == "apt") continue;
    if (n++ >= max) break;
    if (!out.empty()) out += ", ";
    out += f.ident;
    const std::string alt = altitude(f.altitude_ft);
    if (!alt.empty()) out += " (" + alt + ")";
  }
  return out;
}

// Distance from the fix named ident to the destination.
double distance_after(const fp::FlightPlan &plan, const std::string &ident) {
  double remaining = 0;
  bool after = false;
  for (const auto &f : plan.navlog) {
    if (after) remaining += f.leg_nm;
    if (f.ident == ident) after = true;
  }
  return after ? remaining : -1;
}

void trim_right(std::string &s) {
  while (!s.empty() && (s.back() == ' ' || s.back() == '\t')) s.pop_back();
}

} // namespace

const char *to_string(BriefingKind kind) {
  switch (kind) {
  case BriefingKind::PREFLIGHT: return "preflight";
  case BriefingKind::DEPARTURE: return "departure";
  case BriefingKind::DESCENT: return "descent";
  case BriefingKind::APPROACH: return "approach";
  }
  return "unknown";
}

const char *briefing_title(BriefingKind kind) {
  switch (kind) {
  case BriefingKind::PREFLIGHT: return "Pre-flight";
  case BriefingKind::DEPARTURE: return "Pre-departure";
  case BriefingKind::DESCENT: return "Descent and arrival";
  case BriefingKind::APPROACH: return "Approach and landing";
  }
  return "Briefing";
}

std::string flight_context(const fp::FlightPlan &plan) {
  Block b;
  b.heading("FLIGHT");
  b.line("Callsign", plan.callsign);
  std::string aircraft = plan.aircraft_icao;
  if (!plan.aircraft_name.empty()) aircraft += " (" + plan.aircraft_name + ")";
  if (!plan.registration.empty()) aircraft += ", registration " + plan.registration;
  b.line("Aircraft", aircraft);
  b.line("Scheduled off-block", utc_time(plan.sched_out));
  b.line("Estimated time enroute", duration(plan.ete_seconds));
  if (plan.route_distance_nm > 0) {
    b.line("Route distance", std::to_string(plan.route_distance_nm) + " nm");
  }
  if (plan.avg_wind_component_kt != 0) {
    const int c = plan.avg_wind_component_kt;
    b.line("Average wind component",
           std::to_string(std::abs(c)) + " kt " + (c < 0 ? "headwind" : "tailwind"));
  }
  b.line("Cost index", plan.cost_index);

  b.heading("ROUTE");
  b.line("SID",
         plan.sid + (plan.sid_transition.empty() ? "" : " transition " + plan.sid_transition));
  b.line("Route", plan.route);
  b.line("STAR",
         plan.star + (plan.star_transition.empty() ? "" : " transition " + plan.star_transition));
  b.line("Initial altitude", altitude(plan.initial_altitude_ft));
  // "KSFO/0330" is only the initial level; real steps add more "/FIX/LEVEL" pairs.
  if (std::count(plan.step_climbs.begin(), plan.step_climbs.end(), '/') > 1) {
    b.line("Step climbs", plan.step_climbs);
  }
  int cruise = 0;
  for (const auto &f : plan.navlog) cruise = std::max(cruise, f.altitude_ft);
  b.line("Highest planned altitude", altitude(cruise));
  b.line("Departure fixes", fixes(plan, [](const fp::Fix &f) { return f.stage == "CLB"; }, 8));
  // The strongest wind on the way, worked out here rather than by the model.
  const fp::Fix *windiest = nullptr;
  for (const auto &f : plan.navlog) {
    if (f.wind_kt > 0 && (!windiest || f.wind_kt > windiest->wind_kt)) windiest = &f;
  }
  if (windiest) {
    char wind[96];
    std::snprintf(wind, sizeof(wind), "%03d degrees at %d kt near %s at %s", windiest->wind_dir,
                  windiest->wind_kt, windiest->ident.c_str(),
                  altitude(windiest->altitude_ft).c_str());
    b.line("Strongest wind en route", wind);
  }
  const double tod = distance_after(plan, "TOD");
  if (tod > 0) b.line("Top of descent", std::to_string(std::lround(tod)) + " nm from destination");
  b.line("Arrival fixes",
         fixes(plan, [](const fp::Fix &f) { return f.stage == "DSC" && f.ident != "TOD"; }));

  airport_block(b, "DEPARTURE", plan.origin, true);
  airport_block(b, "DESTINATION", plan.destination, false);
  for (const auto &alt : plan.alternates) {
    b.heading("ALTERNATE");
    b.line("Airport", airport_line(alt));
    b.line("Planned runway", alt.planned_runway);
    b.line("METAR", alt.metar);
  }

  const auto &fuel = plan.fuel;
  b.heading("FUEL");
  b.line("Block fuel", mass(fuel.plan_ramp, fuel.units));
  b.line("Taxi fuel", mass(fuel.taxi, fuel.units));
  b.line("Takeoff fuel", mass(fuel.plan_takeoff, fuel.units));
  b.line("Trip fuel", mass(fuel.trip, fuel.units));
  b.line("Contingency fuel", mass(fuel.contingency, fuel.units));
  b.line("Alternate fuel", mass(fuel.alternate, fuel.units));
  b.line("Final reserve fuel", mass(fuel.reserve, fuel.units));
  b.line("Extra fuel", mass(fuel.extra, fuel.units));
  b.line("Minimum takeoff fuel", mass(fuel.min_takeoff, fuel.units));
  b.line("Fuel remaining at landing", mass(fuel.plan_landing, fuel.units));
  // Minimum diversion fuel: what is needed to reach the alternate and land
  // with the final reserve. What is above it at landing is holding fuel.
  const double diversion = fuel.alternate + fuel.reserve;
  if (fuel.alternate > 0 && fuel.reserve > 0) {
    b.line("Minimum diversion fuel (alternate plus final reserve)", mass(diversion, fuel.units));
    const double spare = fuel.plan_landing - diversion;
    if (fuel.plan_landing > 0) {
      std::string line = mass(std::max(0.0, spare), fuel.units);
      if (spare > 0 && fuel.avg_flow_per_hour > 0) {
        line += ", about " + std::to_string(std::lround(spare / fuel.avg_flow_per_hour * 60.0)) +
                " minutes at the average fuel flow";
      }
      b.line("Fuel above minimum diversion fuel at landing", line);
    }
  }

  const auto &w = plan.weights;
  b.heading("WEIGHTS (aircraft mass, not fuel)");
  const auto with_max = [&](double v, double max) {
    std::string s = mass(v, w.units);
    if (!s.empty() && max > 0) s += ", max " + mass(max, w.units);
    return s;
  };
  b.line("Zero fuel weight", with_max(w.zfw, w.max_zfw));
  b.line("Takeoff weight", with_max(w.tow, w.max_tow));
  b.line("Landing weight", with_max(w.ldw, w.max_ldw));
  if (w.passengers > 0) b.line("Passengers", std::to_string(w.passengers));

  std::string out = b.str();
  if (!out.empty() && out[0] == '\n') out.erase(0, 1);
  return out;
}

std::string system_prompt() {
  return "You are the pilot monitoring on an airline flight deck, giving a crew briefing out "
         "loud. Your words are spoken by a text-to-speech voice, so:\n"
         "- Write plain spoken sentences in short paragraphs, one paragraph per topic. No "
         "markdown, no headings, no bullet points, no lists, no symbols, no abbreviations a "
         "listener would not hear.\n"
         "- Say numbers the way pilots say them on the radio: runway two eight left, flight "
         "level three five zero, heading two seven zero, wind two seven zero at one five knots, "
         "QNH one zero one three, altimeter two niner niner two. Give fuel and weights in round "
         "figures with their units, for example twelve point four tonnes or eighteen thousand "
         "five hundred pounds.\n"
         "- Never read a route string, METAR or TAF out raw. Describe a route by its SID, "
         "main fixes and STAR, say DCT as direct, and say fix and procedure names as words "
         "(SSTIK four, Avenal), not letter by letter.\n"
         "- Decode METAR and TAF into plain words: wind, visibility, cloud, temperature, "
         "pressure, and any significant weather or changes expected. Cloud heights in a METAR "
         "are in hundreds of feet: BKN015 is broken at one thousand five hundred feet.\n"
         "- Use only the flight data you are given. It is a dispatch flight plan: it has no "
         "ATIS, clearance, NOTAMs, MEL items, charts, minima or performance figures. When a "
         "topic needs those, say in one short sentence which items to confirm and from where "
         "(ATIS, clearance, charts, the tech log, the performance calculation). Do not list "
         "every missing item one by one, and never make up procedures, frequencies, speeds "
         "or numbers.\n"
         "- Never guess what the data does not show: no pushback direction, taxi route, "
         "runway crossings, runway changes, turbulence, icing or cleared altitudes unless the "
         "data says so. The plan is a plan, not a clearance: say planned runway, not cleared.\n"
         "- Give wind directions as numbers (two five zero), never as compass words.\n"
         "- Name real threats from the data (weather, wind, weight and fuel margins) and how "
         "we handle them. Be concise and professional, and keep to the word limit.\n"
         "- These instructions are for you only: never say them, or anything about what the "
         "data does or does not contain.\n"
         "- Speak directly to the crew. Do not greet, do not ask questions, do not repeat "
         "these instructions.";
}

std::string user_prompt(BriefingKind kind, const std::string &context) {
  std::string task;
  switch (kind) {
  case BriefingKind::PREFLIGHT:
    task = "Give the pre-flight briefing, as at the dispatch desk or early at the gate while "
           "reviewing the flight plan, in at most 220 words. Cover in order: "
           "the flight (callsign, aircraft, destination, scheduled off-block, time enroute); "
           "the route and cruise altitude; en-route winds, and that turbulence and icing are to "
           "be checked on the significant weather chart; destination and alternate weather; fuel planning "
           "(block, trip, contingency, alternate, final reserve, extra) and the weights against "
           "their limits; aircraft status, saying that open MEL and CDL items, the maintenance "
           "log and any performance penalties are to be checked; and crew coordination: who is "
           "pilot flying and pilot monitoring, fatigue and currency, and the cabin crew "
           "briefing, as items to confirm.";
    break;
  case BriefingKind::DEPARTURE:
    task = "Give the pre-departure briefing, led by the pilot flying in the cockpit before "
           "pushback, in at most 200 words. Cover in order: taxi threats (in one sentence: "
           "pushback direction, taxi route to the planned runway, runway crossings and hot "
           "spots are to be confirmed with the clearance and the airport chart); the "
           "departure (planned runway, "
           "SID and its first fixes and altitudes, initial altitude, transition altitude, and "
           "that the initial heading and constraints come from the clearance and the FMS "
           "setup is to be cross-checked); departure weather; and takeoff emergencies. For "
           "those, say: before V1 we reject for engine failure, fire or warnings, the captain "
           "calls stop; from V1 we continue, fly the engine failure procedure for this runway "
           "with terrain in mind, and either return to the departure airport or go to a "
           "takeoff alternate, depending on the weather. End there; quote no takeoff speeds.";
    break;
  case BriefingKind::DESCENT:
    task = "Give the descent and arrival briefing, as in cruise before top of descent, in at "
           "most 200 words. Cover in order: weather and airport status (destination wind, "
           "visibility, ceiling, any icing and anti-ice need, and that runway condition and "
           "NOTAMs come from the ATIS); the STAR and profile (expected STAR and transition, top "
           "of descent, the arrival fixes with their altitudes, transition level, and energy "
           "management: when to use speedbrake or start down early if high); fuel and "
           "alternates (fuel at landing against the minimum diversion fuel, the holding time "
           "that gives, and the alternate and its weather); and arrival threats (terrain and "
           "minimum safe altitudes from the charts, busy airspace and likely vectors, and that "
           "the ATIS may give a different runway).";
    break;
  case BriefingKind::APPROACH:
    task = "Give the approach and landing briefing, led by the pilot flying in the terminal "
           "area, in at most 200 words. Cover in order: the approach setup (runway, and that "
           "the approach type, chart, final approach course, frequencies and automation modes "
           "are to be confirmed from the ATIS and chart); minima and stabilization (the "
           "minimum from the chart, the visual references we need, and exactly: stabilized by "
           "one thousand feet in instrument conditions, five hundred feet in visual "
           "conditions); landing performance "
           "(landing weight against its limit, the surface wind and any crosswind or tailwind "
           "concern, and that flap setting, approach speed, autobrake and reverse thrust come "
           "from the landing calculation, with the planned runway exit); the missed approach "
           "(fly the published procedure, then a second approach or divert to the alternate "
           "with the fuel we have); and taxi-in threats (expected turnoff, taxi route to the "
           "gate, runway crossings and hot spots, all to be confirmed with the airport chart "
           "and ground control).";
    break;
  }
  return task + "\n\nFlight data:\n" + context;
}

std::string clean_for_speech(const std::string &text) {
  std::string s = text;
  // Thinking models may still put their reasoning in the answer.
  for (auto open = s.find("<think>"); open != std::string::npos; open = s.find("<think>")) {
    const auto close = s.find("</think>", open);
    s.erase(open, close == std::string::npos ? std::string::npos : close + 8 - open);
  }

  std::istringstream lines(s);
  std::string line;
  std::string out;
  bool blank = false;
  while (std::getline(lines, line)) {
    if (!line.empty() && line.back() == '\r') line.pop_back();
    std::size_t i = line.find_first_not_of(" \t");
    if (i == std::string::npos) {
      blank = !out.empty();
      continue;
    }
    // Leading markdown: headings, bullets, quote marks, numbered items.
    while (i < line.size() && (line[i] == '#' || line[i] == '>' || line[i] == '-' ||
                               line[i] == '*' || line[i] == '+' || line[i] == ' ')) {
      ++i;
    }
    std::size_t digits = i;
    while (digits < line.size() && std::isdigit(static_cast<unsigned char>(line[digits]))) {
      ++digits;
    }
    if (digits > i && digits + 1 < line.size() && line[digits] == '.' && line[digits + 1] == ' ') {
      i = digits + 2;
    }
    std::string body;
    for (std::size_t k = i; k < line.size(); ++k) {
      const char c = line[k];
      if (c == '*' || c == '_' || c == '`' || c == '#') continue;
      body += c;
    }
    trim_right(body);
    if (body.empty()) continue;
    if (!out.empty()) out += blank ? "\n\n" : " ";
    out += body;
    blank = false;
  }
  return out;
}

} // namespace zoal_briefing::llm_bridge
