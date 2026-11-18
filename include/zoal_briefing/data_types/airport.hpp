#ifndef ZOAL_BRIEFING_AIRPORT_HPP
#define ZOAL_BRIEFING_AIRPORT_HPP

#include <string>
#include <vector>
#include <unordered_map>
#include <optional>

namespace zoal_briefing::apt {

struct RunwayData {
    std::string designation;  // e.g., "25L"
    double mag_var{};
    int length_ft{};
    int width_ft{};
    bool instrument_lighted{true};
};

struct AirportInfo {
    std::string icao_code;     // e.g., "KSFO"
    std::string name;          // "San Francisco Intl"
    double lat{};
    double lon{};
    int elevation_ft{};
    double mag_declination{};
    std::vector<RunwayData> runways;
};

class AirportCache {
public:
    void load_from_apt_dat(const std::string& path);
    void load_from_cifp(const std::string& cifp_dir);

    std::optional<AirportInfo> get(const std::string& icao) const;
    
    // Parse APT.DAT raw text into AirportInfo structs
    static std::vector<AirportInfo> parse_apt_dat(const std::string& raw_text);

private:
    std::unordered_map<std::string, AirportInfo> cache_;
};

} // namespace zoal_briefing::apt

#endif // ZOAL_BRIEFING_AIRPORT_HPP
