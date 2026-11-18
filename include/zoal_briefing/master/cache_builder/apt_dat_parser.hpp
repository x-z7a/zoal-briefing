#ifndef ZOAL_BRIEFING_APT_DAT_PARSER_HPP
#define ZOAL_BRIEFING_APT_DAT_PARSER_HPP

#include "zoal_briefing/master/data_types/airport.hpp"
#include <string>

namespace zoal_briefing::apt {

// Parse APT.DAT content into AirportInfo map.
class AptDatParser {
public:
    void parse(const std::string& raw_text);
    const std::unordered_map<std::string, AirportInfo>& cache() const;
    
private:
    std::unordered_map<std::string, AirportInfo> cache_;
};

} // namespace zoal_briefing::apt

#endif // ZOAL_BRIEFING_APT_DAT_PARSER_HPP
