#ifndef ZOAL_BRIEFING_RADAR_CLIENT_HPP
#define ZOAL_BRIEFING_RADAR_CLIENT_HPP

#include "zoal_briefing/master/data_types/weather.hpp"
#include <string>
#include <vector>

namespace zoal_briefing::wx {

// RadarClient — Fetches precipitation radar data (NEXRAD/WSR-88D).
class RadarClient {
public:
    explicit RadarClient(const std::string& api_base_url);
    
    // Fetch radar composites for a region around lat/lon.
    void fetch_region(double center_lat, double center_lon, 
                      double radius_nm = 50.0);
    
    const std::vector<RadarObservation>& get_observations() const;

private:
    std::string base_url_;
    std::vector<RadarObservation> observations_;
};

} // namespace zoal_briefing::wx

#endif // ZOAL_BRIEFING_RADAR_CLIENT_HPP
