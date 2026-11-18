#ifndef ZOAL_BRIEFING_KEYWORD_ROUTER_HPP
#define ZOAL_BRIEFING_KEYWORD_ROUTER_HPP

#include <string>
#include <unordered_map>
#include <optional>
#include <vector>

namespace zoal_briefing::intent {

// Maps keywords to briefing types and parsing rules.
class KeywordRouter {
public:
    // Register a keyword → type mapping.
    void register_keyword(const std::string& word, int type_code);
    
    // Score the transcript against all keywords. Return the best match.
    int score(const std::string& transcript) const;

    // Get all matching keywords for a transcript.
    std::vector<std::string> get_matches(const std::string& transcript) const;

private:
    struct KeywordEntry {
        std::string word;
        int type_code;
        int weight;  // For scoring (weather keywords may have higher weight).
    };
    std::vector<KeywordEntry> keywords_;
};

} // namespace zoal_briefing::intent

#endif // ZOAL_BRIEFING_KEYWORD_ROUTER_HPP
