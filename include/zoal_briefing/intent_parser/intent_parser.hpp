#ifndef ZOAL_BRIEFING_INTENT_PARSER_HPP
#define ZOAL_BRIEFING_INTENT_PARSER_HPP

#include <string>
#include <optional>
#include <unordered_set>
#include <vector>
#include <regex>

namespace zoal_briefing::intent {

enum class BriefingType {
    DISPATCH,
    DEPARTURE,
    ARRIVAL,
    LANDING,
    WEATHER,           // Weather-specific briefing (new for storm/rain priority)
    UNKNOWN
};

struct IntentResult {
    BriefingType type{BriefingType::UNKNOWN};
    std::optional<std::string> airport;  // ICAO or aircraft position
    std::optional<std::string> runway;
    std::vector<std::string> keywords_found; // For debugging/logging.
    bool confirmed = false;
};

class IntentParser {
public:
    IntentParser();

    // Parse a spoken transcript into an intent.
    // Looks for briefing type keywords, ICAO codes, and runway numbers.
    IntentResult parse(const std::string& transcript);
    
    // Check if the transcript is a valid briefing request.
    bool is_valid_request(const std::string& transcript) const;
    
    // Extract an ICAO code from the transcript (e.g., "KJFK", "KSFO").
    std::optional<std::string> extract_icao(const std::string& transcript);

private:
    std::unordered_map<std::string, BriefingType> keywords_;
    std::regex icao_pattern_;  // KXXX or EGLL pattern.
};

} // namespace zoal_briefing::intent

#endif // ZOAL_BRIEFING_INTENT_PARSER_HPP
