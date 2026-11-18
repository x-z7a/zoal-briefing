#ifndef ZOAL_BRIEFING_FALLBACK_CHAIN_HPP
#define ZOAL_BRIEFING_FALLBACK_CHAIN_HPP

#include <string>
#include <optional>
#include <vector>

namespace zoal_briefing::llm {

// Fallback chain for LLM bridge failures:
//   API mode → CLI mode → cached template → voice prompt
class FallbackChain {
public:
    // Try each step in sequence until one succeeds.
    struct Step {
        std::string name;
        std::function<std::optional<std::string>()> runnable;
    };

    void add_step(const Step& step);
    
    // Run the chain, returning the first non-empty result or nullopt if all fail.
    std::optional<std::string> run();

private:
    std::vector<Step> steps_;
};

} // namespace zoal_briefing::llm

#endif // ZOAL_BRIEFING_FALLBACK_CHAIN_HPP
