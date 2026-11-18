#ifndef ZOAL_BRIEFING_SIGMET_CLIENT_HPP
#define ZOAL_BRIEFING_SIGMET_CLIENT_HPP

#include "zoal_briefing/master/data_types/weather.hpp"
#include <string>
#include <vector>
#include <optional>

namespace zoal_briefing::wx {

// SigmetClient — Fetches SIGMETs and AIRMETs from NOAA.
class SigmetClient {
public:
    explicit SigmetClient(const std::string& api_base_url);
    
    // Fetch all active SIGMETs/AIRMETs.
    void fetch_all();
    
    // Filter by region (lat/lon + radius).
    std::vector<SignificantWeather> get_in_region(
        double lat, double lon, double radius_nm) const;

private:
    std::string base_url_;
    std::vector<SignificantWeather> sigmet_airmet_;
};

} // namespace zoal_briefing::wx

#endif // ZOAL_BRIEFING_SIGMET_CLIENT_HPP
