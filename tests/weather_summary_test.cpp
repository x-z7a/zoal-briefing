#include "zoal_briefing/data_types/weather.hpp"
#include <iostream>
#include <cassert>

using namespace zoal_briefing::wx;

int main() {
    MetarFileData metar;
    metar.station = "KORD";
    metar.wind_speed_kt = 25;
    metar.visibility_sm = 10.0;
    metar.weather_codes = { "+TSRA" };
    metar.temperature_c = 23.0;
    metar.dewpoint_c = 13.0;
    metar.raw_text = "KORD 171951Z 27015G25KT 10SM FEW045 23/13 A3002";
    
    GribData grib;
    // No thunderstorm stubs - just test the metar-based logic
    
    WeatherSummary sum = summarize(metar, grib);
    
    assert(sum.has_thunderstorms == true);
    assert(sum.should_divert() == false); // Needs more hazard density to trigger divert automatically in this simple stub
    
    std::cout << "[Test] Weather summary logic passed." << std::endl;
    return 0;
}
