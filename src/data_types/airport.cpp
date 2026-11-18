#include "zoal_briefing/master/data_types/airport.hpp"

namespace zoal_briefing::apt {

void AirportCache::load_from_apt_dat(const std::string& path) {
    // TODO: Read APT.DAT file, parse into AirportInfo structs.
}

std::optional<AirportInfo> AirportCache::get(const std::string& icao) const {
    auto it = cache_.find(icao);
    if (it != cache_.end()) {
        return it->second;
    }
    return std::nullopt;
}

std::vector<AirportInfo> AirportCache::parse_apt_dat(const std::string& raw_text) {
    // TODO: Parse APT.DAT binary/text into AirportInfo vector.
    return {};
}

} // namespace zoal_briefing::apt
