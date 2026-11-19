#include "zoal_briefing/cache_builder/cache_builder.hpp"

#include "zoal_briefing/log.hpp"

namespace zoal_briefing::cache {

CacheBuilder::CacheBuilder(
    const std::string& apt_dat_path,
    const std::string& cifp_dir,
    const std::string& grib_dir)
    : apt_dat_path_(apt_dat_path),
      cifp_dir_(cifp_dir),
      grib_dir_(grib_dir) {
}

void CacheBuilder::log(const std::string& msg) const {
    zoal_briefing::log::info("cache", msg);
}

void CacheBuilder::start() {
    worker_ = std::thread([this] {
        log("starting async parsing...");
        
        // 1. Parse APT.DAT if available
        if (!apt_dat_path_.empty()) {
            // TODO: Wire up AptDatParser
            log("APT.DAT placeholder loaded");
        }
        
        // 2. Parse CIFP if available  
        if (!cifp_dir_.empty()) {
            // TODO: Wire up CifpParser
            log("CIFP placeholder loaded");
        }
        
        // 3. Parse GRIB if available
        if (!grib_dir_.empty()) {
            // TODO: Wire up GribParser  
            log("GRIB placeholder loaded");
        }
        
        ready_ = true;
        log("startup cache build complete");
    });
}

bool CacheBuilder::is_ready() const {
    return ready_.load();
}

} // namespace zoal_briefing::cache
