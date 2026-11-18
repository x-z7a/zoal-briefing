#ifndef ZOAL_BRIEFING_WEATHER_SUMMARIZER_HPP
#define ZOAL_BRIEFING_WEATHER_SUMMARIZER_HPP

#include "zoal_briefing/data_types/weather.hpp"
#include <string>
#include <vector>
#include <optional>
#include <unordered_set>

namespace zoal_briefing::wx {

// WeatherSummarizer — Aggregates METAR, TAF, SIGMET/AIRMET, PIREP, and radar
// into a single WeatherSummary for briefing generation.
// 
// IMPORTANT: Storm/rain/weather information is prioritized above all else in
// the output because thunderstorms can create diversion requirements that 
// completely change flight operations.
class WeatherSummarizer {
public:
    // Initialize with sources (pointers are non-owning).
    WeatherSummarizer(const METARWorker* metar_worker,
                      const SigmetClient* sigmet_client = nullptr,
                      const PIREPClient* pirep_client = nullptr,
                      const GRIBParser* grib_parser = nullptr);
    
    // Run the summary — pulls from all available sources.
    // Returns a complete WeatherSummary with flight category, thunderstorm
    // alerts, SIGMET/AIRMET status, and precipitation intensity.
    WeatherSummary summarize(const std::string& icao,
                             double lat = 37.0,
                             double lon = -122.0) const;

    // Build a simplified summary from just a METAR (for quick checks).
    WeatherSummary build_from_metar(const METARData& metar) const;

    // Generate a human-readable summary string for briefing text.
    std::string generate_summary_text(const WeatherSummary& ws) const;

private:
    const METARWorker* metar_worker_;
    const SigmetClient* sigmet_client_;
    const PIREPClient* pirep_client_;
    const GRIBParser* grib_parser_;
};

} // namespace zoal_briefing::wx

#endif // ZOAL_BRIEFING_WEATHER_SUMMARIZER_HPP
