#ifndef ZOAL_BRIEFING_SID_STAR_HPP
#define ZOAL_BRIEFING_SID_STAR_HPP

#include <string>
#include <vector>
#include <unordered_map>

namespace zoal_briefing::nav {

struct NavigraphFix {
    std::string name;
    double lat{};
    double lon{};
};

struct ProceduralEntry {
    std::string fix_name;
    int min_alt_ft{};
    int max_alt_ft{};
};

struct SIDProcedure {
    std::string sid_name;
    std::string runway;
    std::vector<NavigraphFix> fixes;
    std::string transition;
};

struct STARProcedure {
    std::string star_name;
    std::string runway;
    std::vector<NavigraphFix> fixes;
    std::string transition;
};

class SidStarCache {
public:
    void load(const std::string& airport_code, 
              const std::vector<sid_star::SIDProcedure>& sids,
              const std::vector<sid_star::STARProcedure>& stars);
    
    std::optional<SIDProcedure> get_sid(const std::string& runway) const;
    std::optional<STARProcedure> get_star(const std::string& runway) const;

private:
    std::unordered_map<std::string, std::vector<SIDProcedure>> sids_;
    std::unordered_map<std::string, std::vector<STARProcedure>> stars_;
};

} // namespace zoal_briefing::nav

#endif // ZOAL_BRIEFING_SID_STAR_HPP
