#ifndef ZOAL_BRIEFING_PIREP_CLIENT_HPP
#define ZOAL_BRIEFING_PIREP_CLIENT_HPP

#include "zoal_briefing/master/data_types/weather.hpp"
#include <string>
#include <vector>

namespace zoal_briefing::wx {

// PIREPClient — Fetches Pilot Reports from NOAA Aviation Weather Center.
class PIREPClient {
public:
    explicit PIREPClient(const std::string& api_base_url);
    
    // Fetch all PIREPs near an airport (lat/lon within radius).
    void fetch_near(double lat, double lon, double radius_nm);
    
    const std::vector<PIREPData>& get_pireps() const;

private:
    std::string base_url_;
    std::vector<PIREPData> pireps_;
};

} // namespace zoal_briefing::wx

#endif // ZOAL_BRIEFING_PIREP_CLIENT_HPP
