#ifndef ZOAL_BRIEFING_WEATHER_HPP
#define ZOAL_BRIEFING_WEATHER_HPP

#include <string>
#include <vector>
#include <cstdint>
#include <optional>

namespace zoal_briefing::wx {

struct WindProfile {
    double pressure_alt_ft;
    double wind_dir_deg;
    double wind_speed_kt;
    double temp_c;
};

struct METARData {
    std::string raw;
    std::string station;
    double vis_statute_mi{};
    int wind_dir_deg{};
    int wind_speed_kt{};
    int wind_gust_kt{};
    char visibility_var[5] = {}; // e.g., "V04" 
    int ceiling_ft{};
    std::string weather_codes;   // "+RASG TSRA"
    char runway_visual_range[1024] = {};
    std::vector<std::string> clouds; // "FEW020 BKN040"
    std::string temp_c;
    std::string dewpoint_c;
    double altimeter_inhg{};
};

class GRIBParser {
public:
    void parse_all(const std::string& grib_dir);
    std::vector<WindProfile> get_winds_for(const std::string& icao, 
                                          double lat, double lon) const;
private:
    std::unordered_map<std::string, std::vector<WindProfile>> winds_;
};

class METARWorker {
public:
    METARWorker(const std::string& xplane_data_dir);

    // Fetch current METAR for a station (async HTTP).
    void fetch_metar(const std::string& icao);
    
    // Get the latest parsed METAR.
    std::optional<METARData> get_latest() const;
    
    // Parse raw METAR string into struct.
    static METARData parse(const std::string& metar_text);

private:
    std::optional<METARData> cached_metar_;
};

} // namespace zoal_briefing::wx

#endif // ZOAL_BRIEFING_WEATHER_HPP
