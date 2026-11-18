#include "zoal_briefing/master/cache_builder/radar_client.hpp"
#include <iostream>
#include <curl/curl.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace zoal_briefing::wx {

static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* user) {
    auto* out = static_cast<std::string*>(user);
    out->append(static_cast<char*>(contents), size * nmemb);
    return size * nmemb;
}

RadarClient::RadarClient(const std::string& api_base_url)
    : base_url_(api_base_url) {
    
    static bool init_done = false;
    if (!init_done) {
        curl_global_init(CURL_GLOBAL_DEFAULT);
        init_done = true;
    }
}

void RadarClient::fetch_region(double center_lat, double center_lon, 
                                double radius_nm) {
    // TODO: Fetch NEXRAD/WSR-88D radar composites from NOAA.
    // e.g., https://nowcoast.noaa.gov/api/radar/nexrad/v1?lat=37&lon=-122&radius=50
    
    std::string url = base_url_ + "/api/radar/nexrad/v1?lat=" 
                      + std::to_string(center_lat) 
                      + "&lon=" + std::to_string(center_lon)
                      + "&radius=" + std::to_string(radius_nm);
    
    CURL* curl = curl_easy_init();
    if (!curl) return;
    
    std::string response_body;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_body);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15.0);
    
    auto res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    
    if (res == CURLE_OK && !response_body.empty()) {
        try {
            auto j = json::parse(response_body, nullptr, false);
            observations_.clear();
            
            for (const auto& cell : j["cells"]) {
                RadarObservation::Obs obs;
                obs.lat = cell.value("lat", 0.0);
                obs.lon = cell.value("lon", 0.0);
                obs.dbz = cell.value("dbz", 0);
                
                // Classify intensity from dBZ.
                if (obs.dbz >= 65) {
                    obs.severity = RadarObservation::Obs::Severity::EXTREME;
                    obs.precip_type = RadarObservation::Obs::PrecipType::HAIL;
                } else if (obs.dbz >= 45) {
                    obs.severity = RadarObservation::Obs::Severity::HEAVY;
                    obs.precip_type = RadarObservation::Obs::PrecipType::RAIN;
                } else if (obs.dbz >= 20) {
                    obs.severity = RadarObservation::Obs::Severity::LIGHT;
                    obs.precip_type = RadarObservation::Obs::PrecipType::RAIN_SNOW_MIX;
                } else {
                    obs.severity = RadarObservation::Obs::Severity::CLEAR;
                    obs.precip_type = RadarObservation::Obs::PrecipType::NONE;
                }
                
                observations_.push_back(obs);
            }
        } catch (...) {
            // JSON parse error.
        }
    }
}

const std::vector<RadarObservation>& RadarClient::get_observations() const {
    return observations_;
}

} // namespace zoal_briefing::wx
