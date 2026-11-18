#ifndef ZOAL_BRIEFING_CIFP_PARSER_HPP
#define ZOAL_BRIEFING_CIFP_PARSER_HPP

#include "zoal_briefing/data_types/cifp.hpp"
#include <string>

namespace zoal_briefing::cifp {

class CifpParserImpl : public CifpParser {
public:
    void parse_all(const std::string& cifp_dir) override;
};

} // namespace zoal_briefing::cifp

#endif // ZOAL_BRIEFING_CIFP_PARSER_HPP
