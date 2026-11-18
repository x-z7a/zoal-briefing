#ifndef ZOAL_BRIEFING_INTENT_PARSER_HPP
#define ZOAL_BRIEFING_INTENT_PARSER_HPP

#include <string>
#include <optional>
#include <unordered_set>

namespace zoal_briefing::intent {

enum class BriefingType {
    DISPATCH,
    DEPARTURE,
    ARRIVAL,
    LANDING,
    UNKNOWN
};

struct IntentResult {
    BriefingType type{BriefingType::UNKNOWN};
    std::optional<std::string> airport;  // ICAO or aircraft position
    std::optional<std::string> runway;
    std::optional<BriefingType> approach_type; // ILS, VOR, RNAV
    bool confirmed = false;
};

class IntentParser {
public:
    IntentParser();

    // Parse a spoken transcript into an intent.
    IntentResult parse(const std::string& transcript);
    
    // Check if the transcript is a valid briefing request.
    bool is_valid_request(const std::string& transcript) const;

private:
    std::unordered_set<std::string> keywords_;
};

} // namespace zoal_briefing::intent

#endif // ZOAL_BRIEFING_INTENT_PARSER_HPP
