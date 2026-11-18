#include "zoal_briefing/master/cache_builder/apt_dat_parser.hpp"

namespace zoal_briefing::apt {

void AptDatParser::parse(const std::string& raw_text) {
    // TODO: Parse APT.DAT binary/text into AirportInfo map.
    // This is the core startup caching logic.
}

const std::unordered_map<std::string, AirportInfo>& AptDatParser::cache() const {
    return cache_;
}

} // namespace zoal_briefing::apt
