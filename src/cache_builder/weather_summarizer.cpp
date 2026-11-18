#include "zoal_briefing/cache_builder/weather_summarizer.hpp"

namespace zoal_briefing::wx {

WeatherSummary WeatherSummarizer::summarize(const MetarFileData& metar, 
                                            const GribData& grib) const {
    // Delegate to the namespace-level summarize function.
    return ::zoal_briefing::wx::summarize(metar, grib);
}

WeatherSummary WeatherSummarizer::build_from_metar(const MetarFileData& metar) const {
    GribData empty_grib;
    return summarize(metar, empty_grib);
}

} // namespace zoal_briefing::wx
