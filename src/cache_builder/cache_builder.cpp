#include "zoal_briefing/master/cache_builder/cache_builder.hpp"
#include <atomic>

namespace zoal_briefing::cache {

CacheBuilder::CacheBuilder(const std::string& apt_dat_path,
                           const std::string& cifp_dir,
                           const std::string& grib_dir,
                           std::optional<std::string> navigraph_key)
    : apt_dat_path_(apt_dat_path),
      cifp_dir_(cifp_dir),
      grib_dir_(grib_dir),
      navigraph_key_(std::move(navigraph_key)) {}

void CacheBuilder::start() {
    worker_ = std::thread([this] {
        // 1. Parse APT.DAT (airport/runway data)
        // 2. Parse CIFP (terminal navigation)
        // 3. Parse GRIB (weather profiles)
        // 4. Fetch Navigraph SID/STAR (if key present)
        ready_ = true;
    });
}

bool CacheBuilder::is_ready() const {
    return ready_.load();
}

} // namespace zoal_briefing::cache
