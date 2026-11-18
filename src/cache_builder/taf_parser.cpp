#include "zoal_briefing/cache_builder/taf_parser.hpp"
#include <iostream>

namespace zoal_briefing::wx {

// TAFParser — Terminal Aerodrome Forecast parser.
// Parses raw TAF text (e.g., from NOAA) into structured periods.

std::optional<TAFData> TAFParser::parse(const std::string& taf_text) {
    if (taf_text.empty()) return std::nullopt;
    
    TAFData taf;
    taf.raw = taf_text;
    // TODO: Parse the full TAF string into periods.
    // Look for "AMD", "PROBE", "FM" change indicators.
    return taf;
}

std::optional<TAFPeriod> TAFParser::get_period(size_t index) const {
    if (index < data_.periods.size()) {
        return data_.periods[index];
    }
    return std::nullopt;
}

} // namespace zoal_briefing::wx
