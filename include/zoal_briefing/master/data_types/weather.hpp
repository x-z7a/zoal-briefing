#pragma once

#include <string>
#include <vector>
#include <optional>
#include <filesystem>

namespace zoal_briefing::wx {

// --- Local File Data Structures (X-Plane Output Folder) ---

struct MetarFileData {
    std::string station;
    int wind_speed_kt = 0;
    double visibility_sm = 10.0;
    std::vector<std::string> weather_codes; // e.g., "+TSRA", "BR", "FG"
    double temperature_c = 0.0;
    double dewpoint_c = 0.0;
    std::string raw_text;
};

struct GribData {
    // Wind profiles (standard pressure levels)
    struct WindProfile {
        int pressure_mb = 0; // e.g., 1000, 850, 700...
        int wind_direction_deg = 0;
        int wind_speed_kt = 0;
        double temp_c = 0.0;
    };
    std::vector<WindProfile> profiles;
    
    // Storm/Severe weather indicators from GRIB
    struct SevereWeather {
        double lat = 0.0;
        double lon = 0.0;
        double precipitation_rate_mm_hr = 0.0; // e.g., >10mm/hr is heavy rain
        bool has_thunderstorm = false;
    };
    std::vector<SevereWeather> active_storms;
};

// --- Summary Aggregation (Local Data Only) ---

struct WeatherSummary {
    MetarFileData metar;
    GribData grib;
    
    // Derived Flags for Briefing Logic
    bool has_thunderstorms = false;
    bool is_hazardous = false;
    std::string divert_reason; // "Wind Shear", "Heavy Rain", "Storms"
    
    // --- Priority Helpers ---
    
    bool should_divert() const {
        if (is_hazardous && !divert_reason.empty()) return true;
        for (const auto& storm : grib.active_storms) {
            if (storm.precipitation_rate_mm_hr > 5.0f || storm.has_thunderstorm) return true;
        }
        return false;
    }
    
    std::string get_rain_label() const {
        for (const auto& code : metar.weather_codes) {
            if (code.find("RA") != std::string::npos && code[0] == '+') return "HEAVY_RAIN";
            if (code.find("RA") != std::string::npos && code[0] != '+') return "MODERATE_RAIN";
        }
        return "NO_RAIN";
    }
};

} // namespace zoal_briefing::wx
