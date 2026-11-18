#ifndef ZOAL_BRIEFING_CIFP_PARSER_HPP
#define ZOAL_BRIEFING_CIFP_PARSER_HPP

#include "zoal_briefing/data_types/cifp.hpp"
#include <string>
#include <vector>

namespace zoal_briefing::cifp {

// Simple CIFP parser stub.
class CifpParserImpl {
public:
    void parse_all(const std::string& cifp_dir);
    const std::vector<TerminalNavData>& get_terminal(const std::string& icao) const;

private:
    std::unordered_map<std::string, std::vector<TerminalNavData>> terminal_;
};

} // namespace zoal_briefing::cifp

#endif // ZOAL_BRIEFING_CIFP_PARSER_HPP
