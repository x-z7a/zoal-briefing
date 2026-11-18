#include "zoal_briefing/master/data_types/weather.hpp"
#include <iostream>
#include <cassert>

using namespace zoal_briefing::wx;

int main() {
    METARData metar = { "KORD", 25, "10SM", {"+TSRA"}, 23.0, 13.0, 30.02 };
    
    // Simulate heavy storm + rain
    WeatherSummary sum = summarize(metar, TAFData{}, std::vector<SIGMETData>{}, std::vector<PIREPData>{}, RadarObservation{});
    
    assert(sum.has_thunderstorms == true);
    assert(sum.should_divert() == false); // Needs more hazard density to trigger divert automatically in this simple stub
    
    std::cout << "[Test] Weather summary logic passed." << std::endl;
    return 0;
}
