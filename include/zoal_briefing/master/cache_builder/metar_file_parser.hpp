#pragma once

#include <string>
#include <vector>
#include <filesystem>

namespace zoal_briefing::cache {

class MetarFileParser {
public:
    // Parse a single METAR string (in-memory)
    static std::string parse_station(const std::string& line);
    static int parse_wind_speed(const std::string& raw);
    static double parse_visibility_sm(const std::string& raw);
    static std::vector<std::string> parse_weather_codes(const std::string& raw);

    // Parse a METAR file from X-Plane's Output/real weather folder
    static bool parse_file(const std::filesystem::path& filepath, class MetarFileData& out);
};

} // namespace zoal_briefing::cache
