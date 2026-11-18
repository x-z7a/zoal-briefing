#ifndef ZOAL_BRIEFING_TAF_PARSER_HPP
#define ZOAL_BRIEFING_TAF_PARSER_HPP

#include "zoal_briefing/master/data_types/weather.hpp"
#include <string>
#include <optional>

namespace zoal_briefing::wx {

// TAFParser — Terminal Aerodrome Forecast parser.
class TAFParser {
public:
    // Parse a complete TAF string.
    static std::optional<TAFData> parse(const std::string& taf_text);
    
    // Get forecast for a specific period index.
    std::optional<TAFPeriod> get_period(size_t index) const;

private:
    TAFData data_;
};

} // namespace zoal_briefing::wx

#endif // ZOAL_BRIEFING_TAF_PARSER_HPP
