#include "zoal_briefing/master/intent_parser/keyword_router.hpp"

namespace zoal_briefing::intent {

void KeywordRouter::register_keyword(const std::string& word, int type_code) {
    keywords_.push_back({word, type_code, 1});
}

int KeywordRouter::score(const std::string& transcript) const {
    // Count how many keywords appear in the transcript.
    int score = 0;
    for (const auto& entry : keywords_) {
        if (transcript.find(entry.word) != std::string::npos) {
            ++score;
        }
    }
    return score;
}

std::vector<std::string> KeywordRouter::get_matches(
    const std::string& transcript) const {
    
    std::vector<std::string> matches;
    for (const auto& entry : keywords_) {
        if (transcript.find(entry.word) != std::string::npos) {
            matches.push_back(entry.word);
        }
    }
    return matches;
}

} // namespace zoal_briefing::intent
