#include "zoal_briefing/cache_builder/navigraph_client.hpp"

namespace zoal_briefing::nav {

NavigraphClient::NavigraphClient(std::string api_key, std::string base_url)
    : api_key_(std::move(api_key)), base_url_(std::move(base_url)) {}

void NavigraphClient::load_procedures(const std::string& icao_code) {
    // TODO: Call Navigraph AIP API for SID/STAR data.
    (void)icao_code;
    loaded_ = true;
}

bool NavigraphClient::has_procedures() const {
    return loaded_;
}

} // namespace zoal_briefing::nav
