#ifndef ZOAL_BRIEFING_KEYWORD_ROUTER_HPP
#define ZOAL_BRIEFING_KEYWORD_ROUTER_HPP

#include <string>
#include <unordered_map>

namespace zoal_briefing::intent {

// Maps keywords to briefing types and parsing rules.
class KeywordRouter {
public:
    // Register a keyword → type mapping.
    void register_keyword(const std::string& word, int type_code);
    
    // Score the transcript against all keywords. Return the best match.
    int score(const std::string& transcript) const;

private:
    std::unordered_map<std::string, int> keyword_map_;
};

} // namespace zoal_briefing::intent

#endif // ZOAL_BRIEFING_KEYWORD_ROUTER_HPP
