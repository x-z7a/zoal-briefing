#include "zoal_briefing/master/cache_builder/metar_file_parser.hpp"
#include <iostream>
#include <cassert>

using namespace zoal_briefing::cache;

int main() {
    std::string test_metar = "KORD 171951Z 27015G25KT 10SM FEW045 23/13 A3002";
    
    // Test Station Parsing
    assert(MetarFileParser::parse_station(test_metar) == "KORD");
    
    // Test Wind Speed Parsing (extracts 15kt base speed)
    assert(MetarFileParser::parse_wind_speed(test_metar) == 15);
    
    std::cout << "[Test] MetarFileParser logic validated." << std::endl;
    return 0;
}
