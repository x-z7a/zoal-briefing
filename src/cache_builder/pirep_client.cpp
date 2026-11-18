#include "zoal_briefing/master/cache_builder/pirep_client.hpp"
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

PIREPClient::PIREPClient(const std::string& api_base_url)
    : base_url_(api_base_url) {
    
    // Initialize libcurl.
    static bool init_done = false;
    if (!init_done) {
        curl_global_init(CURL_GLOBAL_DEFAULT);
        init_done = true;
    }
}

void PIREPClient::fetch_near(double lat, double lon, double radius_nm) {
    // TODO: HTTP GET to NOAA PIREP API.
    // e.g., https://aviationweather.gov/api/data/pirep?lat=37&lon=-122&radius=50
    
    std::string url = base_url_ + "/api/data/pirep";
    
    CURL* curl = curl_easy_init();
    if (!curl) return;
    
    std::string response_body;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_body);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10.0);
    
    auto res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    
    if (res == CURLE_OK && !response_body.empty()) {
        try {
            auto j = json::parse(response_body, nullptr, false);
            pireps_.clear();
            
            for (const auto& item : j) {
                PIREPData p;
                p.raw_report = item.value("report", "");
                
                PIREPData::Observation obs;
                obs.lat = item.value("latitude", lat);
                obs.lon = item.value("longitude", lon);
                obs.altitude_ft = item.value("altitude", 0) * 1000;
                
                // Parse turbulence severity.
                std::string turb_str = item.value("turbulence", "");
                if (turb_str.find("SEV") != std::string::npos || 
                    turb_str == "S" || turb_str == "SMTG") {
                    obs.turb = PIREPData::Observation::TurbSeverity::SEV_MTG;
                } else if (turb_str.find("MOD") != std::string::npos) {
                    obs.turb = PIREPData::Observation::TurbSeverity::MOD;
                } else {
                    obs.turb = PIREPData::Observation::TurbSeverity::NONE;
                }
                
                // Parse icing severity.
                std::string ice_str = item.value("icing", "");
                if (ice_str.find("SEV") != std::string::npos || ice_str == "S") {
                    obs.ice = PIREPData::Observation::IceSeverity::SEVERE;
                } else if (ice_str.find("MOD") != std::string::npos) {
                    obs.ice = PIREPData::Observation::IceSeverity::MODERATE;
                } else if (ice_str.find("LGT") != std::string::npos) {
                    obs.ice = PIREPData::Observation::IceSeverity::LIGHT;
                } else {
                    obs.ice = PIREPData::Observation::IceSeverity::NONE;
                }
                
                p.observations.push_back(obs);
                pireps_.push_back(std::move(p));
            }
        } catch (...) {
            // JSON parse error, keep existing data.
        }
    }
}

const std::vector<PIREPData>& PIREPClient::get_pireps() const {
    return pireps_;
}

} // namespace zoal_briefing::wx
