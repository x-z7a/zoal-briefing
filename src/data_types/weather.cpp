#include "zoal_briefing/master/data_types/weather.hpp"

namespace zoal_briefing::wx {

void GRIBParser::parse_all(const std::string& grib_dir) {
    // TODO: Parse GRIB2 files with eccodes into WindProfile structs.
}

std::vector<WindProfile> GRIBParser::get_winds_for(const std::string& icao,
                                                     double lat, double lon) const {
    static const std::vector<WindProfile> empty;
    auto it = winds_.find(icao);
    if (it != winds_.end()) return it->second;
    return empty;
}

METARWorker::METARWorker(const std::string& xplane_data_dir) {
    // TODO: Initialize METAR fetching from X-Plane data or NOAA.
}

void METARWorker::fetch_metar(const std::string& icao) {
    // TODO: HTTP GET to get METAR for ICAO, parse into cached_metar_.
    (void)icao;
}

std::optional<METARData> METARWorker::get_latest() const {
    return cached_metar_;
}

METARData METARWorker::parse(const std::string& metar_text) {
    // TODO: Parse raw METAR string into METARData struct.
    return {};
}

} // namespace zoal_briefing::wx
