#include "zoal_briefing/data_types/weather.hpp"
#include <iostream>

namespace zoal_briefing::wx {

WeatherSummary summarize(const MetarFileData& metar, const GribData& grib) {
    WeatherSummary summary;
    
    // 1. Populate from METAR files (Real Weather Folder)
    summary.metar = metar;
    
    // Check for storms/heavy rain in METAR text
    for (const auto& code : metar.weather_codes) {
        if (code.find("TS") != std::string::npos) {
            summary.has_thunderstorms = true;
        }
        if (code.find("+RA") != std::string::npos) {
            summary.is_hazardous = true;
        }
    }

    // 2. Populate from GRIB files (Output/real weather / GRIB folder)
    summary.grib = grib;

    // Check for storms/heavy rain in GRIB precipitation intensity
    for (const auto& storm : grib.active_storms) {
        if (storm.precipitation_rate_mm_hr > 5.0f) {
            summary.is_hazardous = true;
        }
        if (storm.has_thunderstorm) {
            summary.has_thunderstorms = true;
        }
    }

    // 3. Finalize Divert Logic
    if (summary.has_thunderstorms && metar.wind_speed_kt > 30) {
        summary.divert_reason = "Wind Shear + TS";
    } else if (grib.active_storms.size() > 2) {
        summary.divert_reason = "Severe Weather Grid";
    }

    return summary;
}

} // namespace zoal_briefing::wx
