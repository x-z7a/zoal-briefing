#pragma once

#include <string>
#include <vector>
#include <filesystem>

// Forward declaration for the GribData struct
namespace zoal_briefing::wx {
    struct GribData;
}

namespace zoal_briefing::cache {

class GribFileParser {
public:
    // Scan X-Plane's GRIB folder and parse relevant weather cells
    static bool parse_directory(const std::filesystem::path& dir_path, class wx::GribData& out);
    
private:
    // Helper to extract wind/temp from GRIB2 record (mock implementation)
    static void process_wind_record(const std::string& record_data, class wx::GribData::WindProfile& profile);
    // Helper to extract storm/rain from GRIB2 precipitable water records
    static void process_precip_record(const std::string& record_data, class wx::GribData::SevereWeather& storm);
};

} // namespace zoal_briefing::cache
