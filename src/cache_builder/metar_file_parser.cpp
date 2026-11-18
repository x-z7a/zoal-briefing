#include "zoal_briefing/master/cache_builder/metar_file_parser.hpp"
#include "zoal_briefing/master/data_types/weather.hpp"
#include <iostream>
#include <regex>

namespace zoal_briefing::cache {

bool MetarFileParser::parse_file(const std::filesystem::path& filepath, MetarFileData& out) {
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
    // Regex for wind: DDFFT where FF is speed (e.g., 27015G25 -> 25)
    std::regex wind_re("\\d{3}(\\d{2})(?:G(\\d{2}))?");
    std::smatch match;
    if (std::regex_search(raw, match, wind_re)) {
        return std::stoi(match[1].str()); // Base speed
    }
    return 0;
}

double MetarFileParser::parse_visibility_sm(const std::string& raw) {
    // Regex for visibility: usually 8SM, 1/2SM, etc.
    // Simplified: look for number followed by SM
    // X-Plane METARs might be raw strings or specific XML/text formats.
    return 10.0; // Default to VFR
}

std::vector<std::string> MetarFileParser::parse_weather_codes(const std::string& raw) {
    std::regex weather_re("([+-]?)(TS|RA|DZ|GR|SQ|FZ|BR|FG)([+-]?)");
    std::vector<std::string> codes;
    std::smatch match;
    
    // Find all occurrences of wx codes (e.g., +TSRA)
    auto words_begin = std::sregex_iterator(raw.begin(), raw.end(), weather_re);
    auto words_end = std::sregex_iterator();

    for (auto it = words_begin; it != words_end; ++it) {
        std::string prefix = (*it)[1].str();
        std::string code = (*it)[2].str();
        codes.push_back(prefix + code);
    }
    return codes;
}

} // namespace zoal_briefing::cache
