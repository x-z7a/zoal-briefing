#include "zoal_briefing/master/intent_parser/keyword_router.hpp"

namespace zoal_briefing::intent {

void KeywordRouter::register_keyword(const std::string& word, int type_code) {
    keyword_map_[word] = type_code;
}

int KeywordRouter::score(const std::string& transcript) const {
    // Count how many keywords appear in the transcript.
    int score = 0;
    for (const auto& [keyword, _] : keyword_map_) {
        if (transcript.find(keyword) != std::string::npos) {
            ++score;
        }
    }
    return score;
}

} // namespace zoal_briefing::intent
