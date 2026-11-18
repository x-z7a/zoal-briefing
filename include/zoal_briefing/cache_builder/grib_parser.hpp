#ifndef ZOAL_BRIEFING_GRIB_PARSER_HPP
#define ZOAL_BRIEFING_GRIB_PARSER_HPP

#include "zoal_briefing/data_types/weather.hpp"
#include <string>
#include <vector>
#include <unordered_map>

namespace zoal_briefing::wx {

class GRIBParser {
public:
    void parse_all(const std::string& grib_dir);
    
    // Get wind profiles for an airport/region.
    std::vector<WindProfile> get_winds_for(
        const std::string& icao, double lat = 0.0, double lon = 0.0) const;

private:
    std::unordered_map<std::string, std::vector<WindProfile>> winds_;
};

} // namespace zoal_briefing::wx

#endif // ZOAL_BRIEFING_GRIB_PARSER_HPP
