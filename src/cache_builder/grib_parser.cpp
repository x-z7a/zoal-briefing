#include "zoal_briefing/cache_builder/grib_parser.hpp"

namespace zoal_briefing::wx {

void GRIBParser::parse_all(const std::string& grib_dir) {
    (void)grib_dir;
    // TODO: Parse GRIB2 binary files for wind/temperature profiles.
}

std::vector<GribData::WindProfile> GRIBParser::get_winds_for(
    const std::string& icao, double lat, double lon) const {
    
    (void)icao; (void)lat; (void)lon;
    static const std::vector<GribData::WindProfile> empty;
    return empty;
}

} // namespace zoal_briefing::wx
