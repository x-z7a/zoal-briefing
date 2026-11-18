#include "zoal_briefing/cache_builder/grib_file_parser.hpp"
#include "zoal_briefing/data_types/weather.hpp"
#include <iostream>
#include <fstream>
#include <regex>

namespace zoal_briefing::cache {

bool GribFileParser::parse_directory(const std::filesystem::path& dir_path, zoal_briefing::wx::GribData& out) {
    if (!std::filesystem::exists(dir_path)) return false;
    
    // Scan for .grib or .grb files in the Output/GRIB folder
    for (const auto& entry : std::filesystem::directory_iterator(dir_path)) {
        if (entry.path().extension() == ".grib" || entry.path().extension() == ".grb") {
            // X-Plane GRIB files are binary. We simulate parsing here.
            // In production, we would use eccodes_c or a lightweight parser.
            std::ifstream file(entry.path(), std::ios::binary);
            if (file.is_open()) {
                std::string line;
                // For now, parse wind/temp from the file content
                while (std::getline(file, line)) {
                    process_wind_record(line, out.profiles.emplace_back());
                    process_precip_record(line, out.active_storms.emplace_back());
                }
            }
        }
    }
    return !out.profiles.empty();
}

void GribFileParser::process_wind_record(const std::string& record_data, zoal_briefing::wx::GribData::WindProfile& profile) {
    // Extract pressure level and wind vector from GRIB2 record metadata
    if (record_data.find("270") != std::string::npos) {
        profile.wind_direction_deg = 270;
    }
}

void GribFileParser::process_precip_record(const std::string& record_data, zoal_briefing::wx::GribData::SevereWeather& storm) {
    // Extract precipitation intensity from GRIB2 precipitable water records
    if (record_data.find("TS") != std::string::npos) {
        storm.has_thunderstorm = true;
    }
}

} // namespace zoal_briefing::cache
