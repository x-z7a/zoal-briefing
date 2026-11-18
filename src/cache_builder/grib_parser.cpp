#include "zoal_briefing/cache_builder/grib_parser.hpp"
#include <iostream>

namespace zoal_briefing::wx {

// GRIBParser — Parses GRIB2 weather files into WindProfile structs.

void GRIBParser::parse_all(const std::string& grib_dir) {
    // TODO: Parse GRIB2 files using eccodes into WindProfile structs.
    // Each .grb file contains pressure level × time point data.
}

std::vector<WindProfile> GRIBParser::get_winds_for(
    const std::string& icao, double lat, double lon) const {
    
    static const std::vector<WindProfile> empty;
    (void)lat; (void)lon;
    
    auto it = winds_.find(icao);
    if (it != winds_.end()) return it->second;
    return empty;
}

} // namespace zoal_briefing::wx
