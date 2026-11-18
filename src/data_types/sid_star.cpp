#include "zoal_briefing/data_types/sid_star.hpp"

namespace zoal_briefing::nav {

void SidStarCache::load(const std::string& airport_code, 
                        const std::vector<SIDProcedure>& sids,
                        const std::vector<STARProcedure>& stars) {
    for (const auto& sid : sids) {
        sids_[sid.runway].push_back(sid);
    }
    for (const auto& star : stars) {
        stars_[star.runway].push_back(star);
    }
}

std::optional<SIDProcedure> SidStarCache::get_sid(const std::string& runway) const {
    auto it = sids_.find(runway);
    if (it != sids_.end() && !it->second.empty()) {
        return it->second[0];
    }
    return std::nullopt;
}

std::optional<STARProcedure> SidStarCache::get_star(const std::string& runway) const {
    auto it = stars_.find(runway);
    if (it != stars_.end() && !it->second.empty()) {
        return it->second[0];
    }
    return std::nullopt;
}

} // namespace zoal_briefing::nav
