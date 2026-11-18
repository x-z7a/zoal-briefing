#include "zoal_briefing/master/intent_parser/intent_parser.hpp"

namespace zoal_briefing::intent {

IntentParser::IntentParser() {
    // Register all known keywords for intent parsing.
    keywords_ = {
        {"dispatch", static_cast<int>(BriefingType::DISPATCH)},
        {"departure", static_cast<int>(BriefingType::DEPARTURE)},
        {"arrival", static_cast<int>(BriefingType::ARRIVAL)},
        {"landing", static_cast<int>(BriefingType::LANDING)},
        {"briefing", 0},
        {"runway", 0},
        {"approach", 0},
        {"sid", 0},
        {"star", 0},
        {"weather", 0}
    };
}

IntentResult IntentParser::parse(const std::string& transcript) {
    IntentResult result;
    
    // Check each keyword against the transcript.
    for (const auto& [keyword, type_code] : keywords_) {
        if (transcript.find(keyword) != std::string::npos) {
            result.type = static_cast<BriefingType>(type_code);
            result.confirmed = true;
        }
    }
    
    return result;
}

bool IntentParser::is_valid_request(const std::string& transcript) const {
    // Check if the transcript contains any briefing-related keywords.
    for (const auto& [keyword, _] : keywords_) {
        if (transcript.find(keyword) != std::string::npos) {
            return true;
        }
    }
    return false;
}

} // namespace zoal_briefing::intent
