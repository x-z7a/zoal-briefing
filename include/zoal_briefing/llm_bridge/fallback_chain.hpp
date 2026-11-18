#ifndef ZOAL_BRIEFING_FALLBACK_CHAIN_HPP
#define ZOAL_BRIEFING_FALLBACK_CHAIN_HPP

#include <string>
#include <optional>
#include <vector>
#include <functional>
#include <thread>
#include <chrono>
#include <atomic>

namespace zoal_briefing::llm {

// Fallback chain for LLM bridge failures:
//   API mode → CLI mode → cached template → voice prompt
class FallbackChain {
public:
    using Runnable = std::function<std::optional<std::string>()>;
    
    struct Step {
        std::string name;
        Runnable runnable;
    };

    void add_step(const Step& step);
    
    // Run the chain, returning the first non-empty result or nullopt if all fail.
    std::optional<std::string> run() const;
    
    // Run the chain with a timeout (returns early if any step succeeds).
    std::optional<std::string> run_with_timeout(double timeout_seconds = 5.0) const;

private:
    std::vector<Step> steps_;
    mutable bool interrupted_ = false;
};

} // namespace zoal_briefing::llm

#endif // ZOAL_BRIEFING_FALLBACK_CHAIN_HPP
