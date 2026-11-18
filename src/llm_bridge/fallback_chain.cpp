#include "zoal_briefing/master/llm_bridge/fallback_chain.hpp"

#include <iostream>
#include <thread>
#include <atomic>
#include <chrono>

namespace zoal_briefing::llm {

void FallbackChain::add_step(const Step& step) {
    steps_.push_back(step);
}

std::optional<std::string> FallbackChain::run() {
    for (const auto& step : steps_) {
        if (interrupted_) break;
        
        auto result = step.runnable();
        if (result.has_value() && !result->empty()) {
            return result;
        }
    }
    return std::nullopt;
}

std::optional<std::string> FallbackChain::run_with_timeout(
    double timeout_seconds) {
    
    interrupted_ = false;
    
    auto start = std::chrono::steady_clock::now();
    
    for (const auto& step : steps_) {
        // Check if we've exceeded the timeout.
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration<double>(now - start).count();
        if (elapsed > timeout_seconds) {
            break;  // Timeout reached.
        }
        
        auto result = step.runnable();
        if (result.has_value() && !result->empty()) {
            return result;
        }
    }
    
    return std::nullopt;
}

} // namespace zoal_briefing::llm
