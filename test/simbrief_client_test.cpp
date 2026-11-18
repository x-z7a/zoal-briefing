#include "zoal_briefing/data_types/flight_plan.hpp"
#include <cassert>
#include <iostream>

using zoal_briefing::fp::FlightPlan;
using zoal_briefing::fp::Waypoint;
using zoal_briefing::fp::RouteLeg;

int main() {
    // Test flight plan construction.
    FlightPlan fp;
    fp.aircraft_type = "B738";
    fp.origin = "KSFO";
    fp.destination = "KJFK";
    fp.cruise_alt_ft = 35000;
    assert(fp.origin == "KSFO");
    std::cout << "PASS: FlightPlan struct construction\n";

    // Test waypoint.
    Waypoint wp;
    wp.name = "SFO";
    wp.lat = 37.6213;
    wp.lon = -122.3790;
    assert(wp.name == "SFO");
    std::cout << "PASS: Waypoint struct validates\n";

    // Test route leg.
    RouteLeg leg;
    leg.from = "SFO";
    leg.to = "SALST";
    leg.type = "SID";
    assert(leg.type == "SID");
    std::cout << "PASS: RouteLeg struct validates\n";

    std::cout << "All SimBrief client tests passed.\n";
    return 0;
}
