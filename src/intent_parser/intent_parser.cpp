#include "zoal_briefing/master/intent_parser/intent_parser.hpp"

#include <iostream>
#include <algorithm>
#include <regex>
#include <unordered_map>

namespace zoal_briefing::intent {

IntentParser::IntentParser() : icao_pattern_(R"([EK]A-Z{3})") {
    // Register all known keywords for intent parsing, mapping to the correct enum.
    keywords_ = {
        {"dispatch", BriefingType::DISPATCH},
        {"departure", BriefingType::DEPARTURE},
        {"arrival", BriefingType::ARRIVAL},
        {"landing", BriefingType::LANDING},
        {"briefing", BriefingType::DEPARTURE}, // Default to departure briefing.
        {"weather", BriefingType::WEATHER},   // New: weather-specific briefing.
        {"storm", BriefingType::WEATHER},     // Weather keyword for storms.
        {"rain", BriefingType::WEATHER},      // Weather keyword for rain.
    };
}

IntentResult IntentParser::parse(const std::string& transcript) {
    IntentResult result;
    
    std::string lower = transcript;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    
    // Find all keywords that match.
    for (const auto& [keyword, type] : keywords_) {
        if (lower.find(keyword) != std::string::npos) {
            result.keywords_found.push_back(keyword);
            if (result.type == BriefingType::UNKNOWN || 
                result.type == BriefingType::DEPARTURE) {
                result.type = type;
            }
        }
    }
    
    // Extract ICAO code from the transcript.
    std::smatch match;
    if (std::regex_search(transcript, match, icao_pattern_)) {
        result.airport = match[0].str();
    }
    
    // Set confirmed if any keyword was found.
    result.confirmed = !result.keywords_found.empty();
    
    return result;
}

bool IntentParser::is_valid_request(const std::string& transcript) const {
    for (const auto& [keyword, _] : keywords_) {
        std::string lower_k = keyword;
        std::transform(lower_k.begin(), lower_k.end(), lower_k.begin(), ::tolower);
        if (transcript.find(lower_k) != std::string::npos || 
            transcript.find(keyword) != std::string::npos) {
            return true;
        }
    }
    return false;
}

std::optional<std::string> IntentParser::extract_icao(const std::string& transcript) {
    std::smatch match;
    if (std::regex_search(transcript, match, icao_pattern_)) {
        return match[0].str();
    }
    return std::nullopt;
}

} // namespace zoal_briefing::intent
