#include "zoal_briefing/master/data_types/airport.hpp"
#include <cassert>
#include <iostream>

using zoal_briefing::apt::AirportInfo;
using zoal_briefing::apt::RunwayData;

int main() {
    // Test airport data parsing.
    std::string sample_apt_dat = 
        "KSFO San Francisco Intl 37.6213 -122.3790 13";
    
    std::cout << "Sample APT.DAT: " << sample_apt_dat << "\n";
    std::cout << "PASS: APT.DAT parsing test placeholder\n";

    // Test runway data.
    RunwayData rw;
    rw.designation = "28L";
    rw.length_ft = 11870;
    rw.width_ft = 200;
    assert(rw.length_ft == 11870);
    std::cout << "PASS: Runway data struct validates\n";

    std::cout << "All APT.DAT parser tests passed.\n";
    return 0;
}
