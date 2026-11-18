#ifndef ZOAL_BRIEFING_CACHE_BUILDER_HPP
#define ZOAL_BRIEFING_CACHE_BUILDER_HPP

#include <string>
#include <thread>
#include <atomic>

namespace zoal_briefing::cache {

// Orchestrates the startup caching of airport data, GRIB weather, and 
// (optionally) Navigraph SID/STAR procedures.
class CacheBuilder {
public:
    explicit CacheBuilder(const std::string& apt_dat_path,
                          const std::string& cifp_dir,
                          const std::string& grib_dir,
                          std::optional<std::string> navigraph_key = std::nullopt);

    // Start the async parsing. Non-blocking.
    void start();
    
    // Check if parsing is done (for data-ready trigger).
    bool is_ready() const;

private:
    std::string apt_dat_path_;
    std::string cifp_dir_;
    std::string grib_dir_;
    std::optional<std::string> navigraph_key_;
    std::atomic<bool> ready_{false};
    std::thread worker_;
};

} // namespace zoal_briefing::cache

#endif // ZOAL_BRIEFING_CACHE_BUILDER_HPP
