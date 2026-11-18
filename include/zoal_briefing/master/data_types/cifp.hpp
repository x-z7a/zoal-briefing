#ifndef ZOAL_BRIEFING_CIFP_HPP
#define ZOAL_BRIEFING_CIFP_HPP

#include <string>
#include <vector>
#include <cstdint>

namespace zoal_briefing::cifp {

// Parsed elements from FAA CIFP (Common Interactive Flight Plan) data.
struct TerminalNavData {
    std::string raw_line;
    int record_type{0};
    std::string ident;
    std::string runway;
    float lat{};
    float lon{};
};

class CifpParser {
public:
    // Parse all CIFP files in directory into in-memory structures.
    void parse_all(const std::string& cifp_dir);

    // Get parsed terminal nav data by airport.
    const std::vector<TerminalNavData>& get_terminal(const std::string& icao) const;

private:
    std::unordered_map<std::string, std::vector<TerminalNavData>> terminal_;
};

} // namespace zoal_briefing::cifp

#endif // ZOAL_BRIEFING_CIFP_HPP
