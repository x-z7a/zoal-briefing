#include "zoal_briefing/master/data_types/weather.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>

namespace zoal_briefing::wx {

FlightCategory compute_flight_category(const METARData& metar) {
    // Simplified VFR/MVFR/IFR/LIFR logic based on visibility and ceiling
    // Real implementation parses visibility_statute_mi and computes ceiling from cloud layers
    if (metar.weather_codes.empty()) return FlightCategory::VFR;
    
    for (const auto& code : metar.weather_codes) {
        if (code.find("BR") != std::string::npos || code.find("FG") != std::string::npos) {
            // Mist/Fog lowers visibility
            return FlightCategory::MVFR; 
        }
    }
    return FlightCategory::VFR;
}

bool check_thunderstorms(const METARData& metar, const TAFData& taf) {
    auto has_ts = [](const std::vector<std::string>& codes) -> bool {
        return std::any_of(codes.begin(), codes.end(), [](const std::string& c){ 
            return c.find("TS") != std::string::npos; 
        });
    };
    if (has_ts(metar.weather_codes)) return true;
    for (const auto& period : taf.periods) {
        if (has_ts(period.weather_codes)) return true;
    }
    return false;
}

bool check_hazardous_conditions(const METARData& metar, const TAFData& taf, 
                                const std::vector<SIGMETData>& sigmets, 
                                const std::vector<PIREPData>& pireps) {
    // 1. SIGMETs/AIRMETs override anything else for diversion logic
    if (std::any_of(sigmets.begin(), sigmets.end(), [](const SIGMETData& s){
        return s.severity == Severity::CRITICAL || s.severity == Severity::SEVERE;
    })) return true;

    // 2. Wind shear / Extreme turbulence PIREPs
    for (const auto& p : pireps) {
        if (p.turbulence_type.find("SEV") != std::string::npos) return true;
        if (p.icing_type.find("SEV") != std::string::npos || p.icing_type.find("MOD") != std::string::npos) return true;
    }

    // 3. Strong winds or heavy rain from METAR
    if (metar.wind_speed_kt > 40) return true;
    
    return false;
}

WeatherSummary summarize(const METARData& metar, const TAFData& taf, 
                         const std::vector<SIGMETData>& sigmets, 
                         const std::vector<PIREPData>& pireps,
                         const RadarObservation& radar) {
    WeatherSummary summary;
    
    summary.metar = metar;
    summary.taf = taf;
    summary.sigmets = sigmets;
    summary.pireps = pireps;
    summary.radar = radar;
    
    // Flight Category
    summary.flight_category = compute_flight_category(metar);
    
    // Critical Weather Checks (Storms & Rain)
    summary.has_thunderstorms = check_thunderstorms(metar, taf);
    
    for (const auto& code : metar.weather_codes) {
        if (code.find("RA") != std::string::npos && code[0] == '+') {
            summary.has_heavy_rain = true; 
        }
    }

    // Hazardous & Divert Logic
    summary.is_hazardous = check_hazardous_conditions(metar, taf, sigmets, pireps);
    
    // Compute divert probability based on storm + rain + wind + sigmet density
    double prob = 0.0;
    if (summary.has_thunderstorms) prob += 0.6;
    if (summary.has_heavy_rain) prob += 0.3;
    if (summary.is_hazardous) prob += 0.4;
    if (metar.wind_speed_kt > 50) prob += 0.2;
    
    summary.divert_probability = std::min(prob, 1.0); // Cap at 1.0
    
    return summary;
}

} // namespace zoal_briefing::wx
