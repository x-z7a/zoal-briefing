#include "zoal_briefing/master/cache_builder/metar_file_parser.hpp"
#include "zoal_briefing/master/data_types/weather.hpp"
#include <iostream>
#include <fstream>
#include <regex>

namespace zoal_briefing::cache {

bool MetarFileParser::parse_file(const std::filesystem::path& filepath, zoal_briefing::wx::MetarFileData& out) {
    if (!std::filesystem::exists(filepath)) return false;
    
    std::ifstream file(filepath);
    if (!file.is_open()) return false;
    
    // X-Plane METAR files often contain the raw string on line 1 or are structured text
    std::string line;
    std::getline(file, line);
    
    out.station = parse_station(line);
    out.wind_speed_kt = parse_wind_speed(line);
    out.visibility_sm = parse_visibility_sm(line);
    out.weather_codes = parse_weather_codes(line);
    out.raw_text = line;
    
    return true;
}

std::string MetarFileParser::parse_station(const std::string& raw) {
    // First 4 characters are usually the station ID (e.g., KORD)
    if (raw.length() >= 4) {
        return raw.substr(0, 4);
    }
    return "";
}

int MetarFileParser::parse_wind_speed(const std::string& raw) {
    // Wind in METAR is typically: "27015G25KT" where XX=direction, YY=speed, Z=GXX or nothing
    // Find "KT" suffix pattern and extract 2 digits before it (or G-prefixed gust)
    std::regex wind_re(R"((\d{3})(\d{2})G?(\d{0,2})KT)");
    std::smatch match;
    if (std::regex_search(raw, match, wind_re)) {
        return std::stoi(match[2].str());  // Base speed is always the last 2 digits before G/kt
    }
    return 0;
}

double MetarFileParser::parse_visibility_sm(const std::string& raw) {
    // Visibility: "10SM", "5SM", "9999m" (METAR), etc.
    std::regex vis_re(R"(^[A-Z0-9]+ +)?(\d+(?:\.\d+)?)\s*SM\b)");
    std::smatch match;
    if (std::regex_search(raw, match, vis_re)) {
        return std::stod(match[1].str());
    }
    return 10.0;
}

std::vector<std::string> MetarFileParser::parse_weather_codes(const std::string& raw) {
    std::vector<std::string> codes;
    // Common weather codes: +RA, -SN, BR, FG, TS, etc.
    static const std::vector<std::string> patterns = {
        "+TSRA", "-TSRA", "TSRA", "+RASN", "-RASN", "RASN",
        "+GR", "-GR", "GR", "+HAIL", "HAIL",
        "+FZRA", "-FZRA", "FZRA", "+DZ", "-DZ", "DZ",
        "+SN", "-SN", "SN", "+SG", "-SG", "SG",
        "+PL", "-PL", "PL", "+GS", "-GS", "GS",
        "RA", "DZ", "SN", "SG", "PL", "GR", "GS",
        "FZDZ", "FZRA", "UP", "BR", "FG", "FU", "VA",
        "DU", "HZ", "SQ", "SS", "NSW", "VCTS", "VCSH",
        "+FC", "-FC", "FC", "PY", "PO"
    };
    
    for (const auto& pattern : patterns) {
        size_t pos = 0;
        while ((pos = raw.find(pattern, pos)) != std::string::npos) {
            // Make sure it's not part of a longer word
            bool is_boundary = true;
            if (pos > 0 && std::isalpha(raw[pos - 1])) is_boundary = false;
            size_t end = pos + pattern.length();
            if (end < raw.size() && std::isalpha(raw[end])) is_boundary = false;
            
            if (is_boundary) {
                codes.push_back(pattern);
            }
            pos += pattern.length();
        }
    }
    
    return codes;
}

} // namespace zoal_briefing::cache
