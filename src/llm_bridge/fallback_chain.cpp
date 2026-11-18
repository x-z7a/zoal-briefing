#include "zoal_briefing/master/llm_bridge/fallback_chain.hpp"

namespace zoal_briefing::llm {

void FallbackChain::add_step(const Step& step) {
    steps_.push_back(step);
}

std::optional<std::string> FallbackChain::run() {
    for (const auto& step : steps_) {
        auto result = step.runnable();
        if (result.has_value() && !result->empty()) {
            return result;
        }
    }
    return std::nullopt;
}

} // namespace zoal_briefing::llm
