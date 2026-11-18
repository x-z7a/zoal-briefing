#pragma once

#include <string>
#include <vector>
#include <optional>

namespace zoal_briefing::wx {

enum class FlightCategory { VFR, MVFR, IFR, LIFR, UNKNOWN };
enum class Severity { LOW, MODERATE, SEVERE, CRITICAL };

struct METARData {
    std::string station;
    int wind_speed_kt = 0;
    std::string visibility_statute_mi = "";
    std::vector<std::string> weather_codes; // e.g., "+TSRA", "BR", "FG"
    double temperature_c = 0.0;
    double dewpoint_c = 0.0;
    double altimeter_inhg = 29.92;
};

struct TAFData {
    std::string station;
    struct TimeGroup {
        std::string valid_from;
        std::string valid_to;
        int wind_speed_kt = 0;
        std::vector<std::string> weather_codes;
    };
    std::vector<TimeGroup> periods;
};

struct SIGMETData {
    std::string id;
    std::string issuing_station;
    std::string text;
    Severity severity = Severity::MODERATE;
};

struct PIREPData {
    std::string location;
    std::string altitude_fl;
    std::string turbulence_type; // e.g., "MOD", "SEV"
    std::string icing_type;      // e.g., "TRACE", "LIGHT RIME"
    std::string wind_dir_deg_t;
};

struct RadarObservation {
    std::string source; // e.g., NEXRAD
    struct Cell {
        float lat = 0.0;
        float lon = 0.0;
        float intensity_dbz = 0.0; // Rain/Storm intensity
        float max_ebzh = 0.0;      // Max Echo Height
    };
    std::vector<Cell> cells;
};

struct WeatherSummary {
    METARData metar;
    TAFData taf;
    std::vector<SIGMETData> sigmets;
    std::vector<PIREPData> pireps;
    RadarObservation radar;
    
    FlightCategory flight_category = FlightCategory::UNKNOWN;
    bool has_thunderstorms = false;
    bool has_heavy_rain = false;
    bool is_hazardous = false;
    double divert_probability = 0.0; // 0.0 to 1.0
    
    // --- Priority Helpers for Briefing ---
    bool should_divert() const {
        if (is_hazardous && divert_probability > 0.75) return true;
        if (has_thunderstorms && metar.wind_speed_kt > 30) return true;
        if (std::any_of(sigmets.begin(), sigmets.end(), [](const SIGMETData& s){ return s.severity == Severity::CRITICAL; })) return true;
        return false;
    }
    
    std::string get_rain_intensity_label() const {
        if (has_heavy_rain) return "HEAVY_RAIN";
        for (const auto& code : metar.weather_codes) {
            if (code.find("RA") != std::string::npos && code[0] == '+') return "MODERATE_RAIN";
            if (code.find("RA") != std::string::npos && code[0] != '+') return "LIGHT_RAIN";
        }
        return "NO_RAIN";
    }
};

} // namespace zoal_briefing::wx
