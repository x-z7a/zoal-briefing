#include "zoal_briefing/master/data_types/weather.hpp"
#include <sstream>
#include <algorithm>
#include <cmath>

namespace zoal_briefing::wx {

// ── METARWorker Implementation ────────────────────────────────────────────────
METARWorker::METARWorker(const std::string& xplane_data_dir) {
    (void)xplane_data_dir;
}

void METARWorker::fetch_metar(const std::string& icao) {
    // TODO: HTTP GET to NOAA / ASOS for METAR string.
    // e.g., https://aviationweather.gov/api/data/metar?ids=KSFO&format=raw
    (void)icao;
}

std::optional<METARData> METARWorker::get_latest() const {
    return cached_metar_;
}

METARData METARWorker::parse(const std::string& metar_text) {
    // TODO: Full METAR parser supporting automated and manual reports.
    // Handles: "KSFO 172053Z 28015KT 10SM FEW020 SCT250 14/M02 A3012"
    (void)metar_text;
    return {};
}

// ── TAFParser Implementation ────────────────────────────────────────────────
std::optional<TAFData> TAFParser::parse(const std::string& taf_text) {
    if (taf_text.empty()) return std::nullopt;
    
    TAFData taf;
    taf.raw = taf_text;
    // TODO: Parse the full TAF string into periods.
    // Look for "AMD", "PROBE", "FM" change indicators.
    return taf;
}

std::optional<TAFPeriod> TAFParser::get_period(size_t index) const {
    if (index < data_.periods.size()) {
        return data_.periods[index];
    }
    return std::nullopt;
}

} // namespace zoal_briefing::wx
