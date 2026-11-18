#include "zoal_briefing/data_types/cifp.hpp"

namespace zoal_briefing::cifp {

void CifpParser::parse_all(const std::string& cifp_dir) {
    // TODO: Read CIFP directory, parse NAVDAT/TOFL/etc. files.
}

const std::vector<TerminalNavData>& CifpParser::get_terminal(
    const std::string& icao) const {
    static const std::vector<TerminalNavData> empty;
    auto it = terminal_.find(icao);
    if (it != terminal_.end()) return it->second;
    return empty;
}

} // namespace zoal_briefing::cifp
