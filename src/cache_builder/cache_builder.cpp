#include "zoal_briefing/cache_builder/cache_builder.hpp"
#include "zoal_briefing/cache_builder/weather_summarizer.hpp"

#include <atomic>
#include <thread>
#include <iostream>

namespace zoal_briefing::cache {

CacheBuilder::CacheBuilder(
    const std::string& apt_dat_path,
    const std::string& cifp_dir,
    const std::string& grib_dir,
    std::optional<std::string> navigraph_key)
    : apt_dat_path_(apt_dat_path),
      cifp_dir_(cifp_dir),
      grib_dir_(grib_dir),
      navigraph_key_(std::move(navigraph_key)) {
    
    // Weather summarizer will be wired in from the caller (plugin entry point).
}

void CacheBuilder::start() {
    worker_ = std::thread([this] {
        log("cache_builder: starting async parsing...");
        
        // 1. Parse APT.DAT — airport/runway data (most critical, done first).
        if (!apt_dat_path_.empty()) {
            apt_dat_parser_.parse(apt_dat_path_);
            log("cache_builder: APT.DAT loaded (" 
                + std::to_string(apt_dat_parser_.cache().size()) + " airports)");
        }
        
        // 2. Parse CIFP — terminal nav data (SIDs/STARs).
        if (!cifp_dir_.empty()) {
            cifp_parser_.parse_all(cifp_dir_);
            log("cache_builder: CIFP loaded");
        }
        
        // 3. Parse GRIB — weather profiles (winds aloft).
        if (!grib_dir_.empty()) {
            grib_parser_.parse_all(grib_dir_);
            log("cache_builder: GRIB loaded");
        }
        
        // 4. Fetch Navigraph SID/STAR (if key present, async).
        if (navigraph_key_.has_value()) {
            nav_client_.load_procedures("*");
            log("cache_builder: Navigraph SID/STAR loaded");
        }
        
        ready_ = true;
        log("cache_builder: startup cache build complete");
    });
}

bool CacheBuilder::is_ready() const {
    return ready_.load();
}

void CacheBuilder::wait_until_ready(int timeout_ms) {
    while (!is_ready()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        if (--timeout_ms <= 0) break;
    }
}

void CacheBuilder::update_weather_for(const std::string& icao, 
                                       double lat, double lon) {
    // This is called when a new flight plan is loaded or airport changes.
    // It re-pulls weather data for the specific airport/region.
    (void)icao; (void)lat; (void)lon;
    // TODO: Trigger WeatherSummarizer to re-aggregate weather for this region.
}

const AptDatParser& CacheBuilder::apt_dat() const { return apt_dat_parser_; }
const CifpParserImpl& CacheBuilder::cifp() const { return cifp_parser_; }
const GRIBParserImpl& CacheBuilder::grib() const { return grib_parser_; }

} // namespace zoal_briefing::cache
