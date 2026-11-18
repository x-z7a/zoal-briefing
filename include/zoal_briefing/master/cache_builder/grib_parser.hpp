#ifndef ZOAL_BRIEFING_GRIB_PARSER_HPP
#define ZOAL_BRIEFING_GRIB_PARSER_HPP

#include "zoal_briefing/master/data_types/weather.hpp"
#include <string>

namespace zoal_briefing::wx {

class GRIBParserImpl : public GRIBParser {
public:
    void parse_all(const std::string& grib_dir) override;
};

} // namespace zoal_briefing::wx

#endif // ZOAL_BRIEFING_GRIB_PARSER_HPP
