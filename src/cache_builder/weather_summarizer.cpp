#include "zoal_briefing/master/cache_builder/weather_summarizer.hpp"
#include "zoal_briefing/master/data_types/weather.hpp"

#include <iostream>
#include <sstream>
#include <vector>
#include <optional>

namespace zoal_briefing::wx {

WeatherSummarizer::WeatherSummarizer(
    const METARWorker* metar_worker,
    const SigmetClient* sigmet_client,
    const PIREPClient* pirep_client,
    const GRIBParser* grib_parser)
    : metar_worker_(metar_worker),
      sigmet_client_(sigmet_client),
      pirep_client_(pirep_client),
      grib_parser_(grib_parser) {}

WeatherSummary WeatherSummarizer::summarize(
    const std::string& icao, double lat, double lon) const {
    
    WeatherSummary summary;
    summary.station = icao;
    summary.lat = lat;
    summary.lon = lon;
    
    // ── Step 1: METAR — Current surface weather (most critical for safety) ────
    if (metar_worker_) {
        auto metar = metar_worker_->get_latest();
        if (metar.has_value()) {
            summary.metar = std::move(metar);
            
            // Classify flight category immediately.
            double vis = summary.metar->vis_statute_mi;
            int ceil = summary.metar->clouds.empty() ? 99999 : 
                       summary.metar->clouds.front().base_ft;
            
            if (vis < 1 || ceil < 1000) {
                summary.flight_category = WeatherSummary::FlightCategory::LIFR;
            } else if (vis < 3 || ceil < 2000) {
                summary.flight_category = WeatherSummary::FlightCategory::IFR;
            } else if (vis < 5 || ceil < 3000) {
                summary.flight_category = WeatherSummary::FlightCategory::MVFR;
            } else {
                summary.flight_category = WeatherSummary::FlightCategory::VFR;
            }
            
            // Check for thunderstorms.
            for (const auto& p : summary.metar->weather_phenom) {
                if (p.thunderstorm) {
                    summary.has_thunderstorms = true;
                    break;
                }
            }
        }
    }
    
    // ── Step 2: SIGMET / AIRMET — Enroute significant weather ────────────────
    if (sigmet_client_) {
        auto sigmets = sigmet_client_->get_in_region(lat, lon, 50.0);
        for (const auto& sig : sigmets) {
            summary.sigmet_airmet.push_back(sig);
            
            // Check for severe issues that require diversion recommendation.
            for (const auto& issue : sig.issues) {
                if (issue.severity == SignificantWeather::Issue::Severity::SEVERE) {
                    summary.is_hazardous = true;
                }
                
                // Track weather categories.
                switch (issue.category) {
                    case SignificantWeather::Issue::Category::TURBULENCE:
                        summary.weather_categories.insert("turbulence");
                        break;
                    case SignificantWeather::Issue::Category::ICE:
                        summary.weather_categories.insert("icing");
                        break;
                    case SignificantWeather::Issue::Category::VISIBILITY:
                        summary.weather_categories.insert("low_visibility");
                        break;
                    case SignificantWeather::Issue::Category::MOUNTAIN_OBSCURATION:
                        summary.weather_categories.insert("mountain_obscuration");
                        break;
                    case SignificantWeather::Issue::Category::VOLCANIC_ASH:
                        summary.weather_categories.insert("volcanic_ash");
                        break;
                }
            }
        }
    }
    
    // ── Step 3: PIREP — Real-time pilot reports (most reliable for storms) ───
    if (pirep_client_) {
        auto pireps = pirep_client_->get_pireps();
        for (const auto& pirep : pireps) {
            summary.pireps.push_back(pirep);
            
            // Check for severe turbulence or icing.
            for (const auto& obs : pirep.observations) {
                if (obs.turb == PIREPData::Observation::TurbSeverity::SEV || 
                    obs.turb == PIREPData::Observation::TurbSeverity::SEV_MTG) {
                    summary.has_thunderstorms = true;
                    summary.is_hazardous = true;
                }
                if (obs.ice != PIREPData::Observation::IceSeverity::NONE) {
                    summary.weather_categories.insert("icing");
                }
            }
        }
    }
    
    // ── Step 4: GRIB — Winds aloft for the route ─────────────────────────────
    if (grib_parser_) {
        auto winds = grib_parser_->get_winds_for(icao, lat, lon);
        summary.winds_aloft = std::move(winds);
    }
    
    // ── Final Assessment: Check for hazardous conditions ─────────────────────
    if (!summary.metar.has_value()) {
        summary.is_hazardous = false;  // No METAR → assume VFR.
        return summary;
    }
    
    // Check for severe thunderstorms in METAR.
    for (const auto& p : summary.metar->weather_phenom) {
        if (p.thunderstorm && p.intensity >= 
            METARData::Phenomenon::Intensity::HEAVY) {
            summary.is_hazardous = true;
        }
    }
    
    // Check for heavy precipitation.
    if (summary.metar->has_precip()) {
        for (const auto& p : summary.metar->weather_phenom) {
            if (p.intensity >= METARData::Phenomenon::Intensity::HEAVY) {
                summary.is_hazardous = true;
            }
        }
    }
    
    return summary;
}

WeatherSummary WeatherSummarizer::build_from_metar(
    const METARData& metar) const {
    
    WeatherSummary summary;
    summary.metar = metar;
    
    // Classify flight category.
    double vis = metar.vis_statute_mi;
    int ceil = metar.clouds.empty() ? 99999 : metar.clouds.front().base_ft;
    
    if (vis < 1 || ceil < 1000) {
        summary.flight_category = WeatherSummary::FlightCategory::LIFR;
    } else if (vis < 3 || ceil < 2000) {
        summary.flight_category = WeatherSummary::FlightCategory::IFR;
    } else if (vis < 5 || ceil < 3000) {
        summary.flight_category = WeatherSummary::FlightCategory::MVFR;
    } else {
        summary.flight_category = WeatherSummary::FlightCategory::VFR;
    }
    
    // Check for thunderstorms.
    for (const auto& p : metar.weather_phenom) {
        if (p.thunderstorm) {
            summary.has_thunderstorms = true;
        }
    }
    
    return summary;
}

std::string WeatherSummarizer::generate_summary_text(const WeatherSummary& ws) const {
    std::ostringstream ss;
    
    // ── WEATHER FIRST — Most critical information for pilots ───────────────
    ss << "WEATHER SUMMARY FOR " << ws.station << "\n\n";
    
    // Flight Category — must be stated first.
    const char* fc_str = "UNKNOWN";
    switch (ws.flight_category) {
        case WeatherSummary::FlightCategory::VFR: fc_str = "VFR"; break;
        case WeatherSummary::FlightCategory::MVFR: fc_str = "MVFR"; break;
        case WeatherSummary::FlightCategory::IFR: fc_str = "IFR"; break;
        case WeatherSummary::FlightCategory::LIFR: fc_str = "LIFR"; break;
    }
    ss << "FLIGHT CATEGORY: " << fc_str << "\n\n";
    
    // Check for hazardous conditions.
    if (ws.has_thunderstorms || ws.is_hazardous) {
        ss << "*** HAZARDOUS WEATHER DETECTED ***\n";
        if (ws.has_thunderstorms) {
            ss << "- THUNDERSTORMS in the area — DIVERSION MAY BE NECESSARY\n";
        }
        for (const auto& cat : ws.weather_categories) {
            ss << "- " << cat << " reported\n";
        }
        ss << "\n** RECOMMEND DIVERSION TO ALTERNATE AIRPORT **\n\n";
    }
    
    // METAR details.
    if (ws.metar.has_value()) {
        const auto& metar = ws.metar.value();
        ss << "METAR:\n";
        ss << "- Wind: " << metar.wind_dir_deg << "° at " 
           << metar.wind_speed_kt << "KT" 
           << (metar.wind_gust_kt ? " GUSTS " + std::to_string(metar.wind_gust_kt) + "KT" : "")
           << "\n";
        ss << "- Visibility: " << metar.vis_statute_mi << "SM\n";
        ss << "- Clouds: ";
        for (const auto& cl : metar.clouds) {
            ss << cl.base_ft << "ft, ";
        }
        ss << "\n";
        ss << "- Temp/Dewpoint: " << metar.temp_c << "/" << metar.dewpoint_c << "C\n";
        ss << "- Altimeter: " << metar.altimeter_inhg << "inHg\n";
        
        // Precipitation.
        if (metar.has_precip()) {
            ss << "- Precipitation: ";
            for (const auto& p : metar.weather_phenom) {
                ss << "[" << std::to_string(static_cast<int>(p.intensity)) << "] " 
                   << p.code << " ";
            }
            ss << "\n";
        }
    }
    
    // SIGMET/AIRMET.
    if (!ws.sigmet_airmet.empty()) {
        ss << "\nSIGMETs/AIRMETs:\n";
        for (const auto& sig : ws.sigmet_airmet) {
            ss << "- " << sig.identifier << ": " << sig.region_description << "\n";
            for (const auto& issue : sig.issues) {
                ss << "  ** " << std::to_string(static_cast<int>(issue.severity)) 
                   << " " << std::to_string(static_cast<int>(issue.category)) << "**\n";
            }
        }
    }
    
    // PIREPs.
    if (!ws.pireps.empty()) {
        ss << "\nPIREPs:\n";
        for (const auto& pirep : ws.pireps) {
            ss << "- " << pirep.raw_report << "\n";
        }
    }
    
    // Winds aloft.
    if (!ws.winds_aloft.empty()) {
        ss << "\nWinds Aloft:\n";
        for (const auto& wp : ws.winds_aloft) {
            ss << "  " << wp.pressure_mb << "hPa: " 
               << wp.direction_deg << "° at " << wp.speed_kt << "KT\n";
        }
    }
    
    return ss.str();
}

} // namespace zoal_briefing::wx
