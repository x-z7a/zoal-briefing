#ifndef ZOAL_BRIEFING_WEATHER_SUMMARIZER_HPP
#define ZOAL_BRIEFING_WEATHER_SUMMARIZER_HPP

#include "zoal_briefing/data_types/weather.hpp"
#include <string>
#include <vector>
#include <optional>

namespace zoal_briefing::wx {

// WeatherSummarizer — Aggregates available weather data into a WeatherSummary.
class WeatherSummarizer {
public:
    WeatherSummarizer() = default;
    
    // Run the summary from METAR and GRIB sources.
    WeatherSummary summarize(const MetarFileData& metar, 
                             const GribData& grib) const;

    // Build a simplified summary from just a METAR.
    WeatherSummary build_from_metar(const MetarFileData& metar) const;

private:
};

} // namespace zoal_briefing::wx

#endif // ZOAL_BRIEFING_WEATHER_SUMMARIZER_HPP
