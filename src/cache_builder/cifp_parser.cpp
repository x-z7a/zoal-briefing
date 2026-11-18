#include "zoal_briefing/cache_builder/cifp_parser.hpp"

namespace zoal_briefing::cifp {

void CifpParserImpl::parse_all(const std::string& cifp_dir) {
    (void)cifp_dir;
    // TODO: Parse CIFP files (TOFL, NAVDAT, etc.) into terminal nav data.
}

const std::vector<TerminalNavData>& CifpParserImpl::get_terminal(
    const std::string& icao) const {
    
    static std::vector<TerminalNavData> empty;
    (void)icao;
    return empty;
}

} // namespace zoal_briefing::cifp
