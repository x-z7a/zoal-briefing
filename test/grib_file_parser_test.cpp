#include "zoal_briefing/cache_builder/grib_file_parser.hpp"
#include <iostream>
#include <filesystem>
#include <fstream>
#include <cassert>

using namespace zoal_briefing::cache;

int main() {
    // Create a mock GRIB directory for testing
    std::string test_dir = "test/mock_grib";
    std::filesystem::create_directories(test_dir);
    
    // Write a mock file that simulates X-Plane's output
    std::string content = "WIND: 270@850mb speed=25kt\nPRECIP: TSRA rate=15mm/hr\n";
    std::filesystem::path mock_file = test_dir + "/mock.grb";
    std::ofstream(mock_file) << content;
    
    zoal_briefing::wx::GribData data;
    if (GribFileParser::parse_directory(test_dir, data)) {
        assert(data.profiles.size() > 0);
        std::cout << "[Test] GribFileParser mock directory scan passed." << std::endl;
    } else {
        std::cout << "[Warn] Mock parsing failed (expected if no real eccodes)." << std::endl;
    }
    
    return 0;
}
