#include "zoal_briefing/master/cache_builder/sigmet_client.hpp"

#include <iostream>
#include <curl/curl.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace zoal_briefing::wx {

// Helper: curl write callback.
static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* user) {
    auto* out = static_cast<std::string*>(user);
    out->append(static_cast<char*>(contents), size * nmemb);
    return size * nmemb;
}

SigmetClient::SigmetClient(const std::string& api_base_url)
    : base_url_(api_base_url) {
    
    // Initialize libcurl.
    static bool init_done = false;
    if (!init_done) {
        curl_global_init(CURL_GLOBAL_DEFAULT);
        init_done = true;
    }
}

void SigmetClient::fetch_all() {
    // TODO: HTTP GET to NOAA SIGMET/AIRMET API.
    // e.g., https://aviationweather.gov/api/data/sigmet?format=json
    
    // For now, stub with a sample response structure.
    std::string url = base_url_ + "/api/data/sigmet?format=json";
    
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
            sigmet_airmet_.clear();
            
            for (const auto& item : j) {
                SignificantWeather sig;
                sig.identifier = item.value("identifier", "");
                sig.issue_time = item.value("issue_time", "");
                sig.valid_from = item.value("valid_from", "");
                sig.valid_to = item.value("valid_to", "");
                
                // Parse issues from the JSON response.
                for (const auto& issue : item["issues"]) {
                    SignificantWeather::Issue i;
                    i.category = issue.value("category", 
                        SignificantWeather::Issue::Category::TURBULENCE);
                    i.description = issue.value("description", "");
                    
                    // Parse severity from text.
                    std::string sev_str = issue.value("severity", "");
                    if (sev_str.find("SEVERE") != std::string::npos || 
                        sev_str == "S") {
                        i.severity = SignificantWeather::Issue::Severity::SEVERE;
                    } else {
                        i.severity = SignificantWeather::Issue::Severity::MODERATE;
                    }
                    
                    sig.issues.push_back(i);
                }
                
                sig.region_description = item.value("region", "");
                sigmet_airmet_.push_back(std::move(sig));
            }
        } catch (...) {
            // JSON parse error, keep existing data.
        }
    }
}

std::vector<SignificantWeather> SigmetClient::get_in_region(
    double lat, double lon, double radius_nm) const {
    
    std::vector<SignificantWeather> result;
    for (const auto& sig : sigmet_airmet_) {
        // TODO: Filter by region using the bounds.
        // For now, return all.
        (void)lat; (void)lon; (void)radius_nm;
        result.push_back(sig);
    }
    return result;
}

} // namespace zoal_briefing::wx
