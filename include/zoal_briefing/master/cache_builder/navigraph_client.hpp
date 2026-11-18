#ifndef ZOAL_BRIEFING_NAVIGRAPH_CLIENT_HPP
#define ZOAL_BRIEFING_NAVIGRAPH_CLIENT_HPP

#include <string>
#include <optional>
#include <vector>

namespace zoal_briefing::nav {

// Simulated Navigraph interface for SID/STAR data.
// In production, this would call the Navigraph AIP API.
class NavigraphClient {
public:
    NavigraphClient(std::string api_key, std::string base_url);
    
    // Fetch SID/STAR JSON and cache it.
    void load_procedures(const std::string& icao_code);
    bool has_procedures() const;

private:
    std::string api_key_;
    std::string base_url_;
    bool loaded_{false};
};

} // namespace zoal_briefing::nav

#endif // ZOAL_BRIEFING_NAVIGRAPH_CLIENT_HPP
