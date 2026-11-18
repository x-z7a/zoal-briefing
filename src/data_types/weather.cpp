#include "zoal_briefing/data_types/weather.hpp"
#include <sstream>
#include <algorithm>
#include <cmath>
#include <regex>

namespace zoal_briefing::wx {

// ── Basic METAR parsing stub ─────────────────────────────────────────────────
WeatherSummary summarize(const MetarFileData& metar, const GribData& grib) {
    WeatherSummary summary;
    summary.metar = metar;
    summary.grib = grib;
    
    // Check for hazardous conditions from METAR
    for (const auto& code : metar.weather_codes) {
        if (code.find("TS") != std::string::npos || 
            code.find("+FC") != std::string::npos) {
            summary.has_thunderstorms = true;
            summary.divert_reason = "Thunderstorms";
        }
        if (code.find("+RA") != std::string::npos || 
            code.find("+FZRA") != std::string::npos) {
            summary.is_hazardous = true;
            if (summary.divert_reason.empty()) {
                summary.divert_reason = "Heavy Precipitation";
            }
        }
    }
    
    // Check GRIB data for storms
    for (const auto& storm : grib.active_storms) {
        if (storm.precipitation_rate_mm_hr > 5.0 || storm.has_thunderstorm) {
            summary.is_hazardous = true;
            if (summary.divert_reason.empty()) {
                summary.divert_reason = "Grib Storm Data";
            }
        }
    }
    
    return summary;
}

} // namespace zoal_briefing::wx
