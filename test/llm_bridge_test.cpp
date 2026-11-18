#include "zoal_briefing/master/llm_bridge/llm_bridge.hpp"
#include <cassert>
#include <iostream>

using zoal_briefing::llm::LlmBridge;
using zoal_briefing::llm::BridgeConfig;

int main() {
    // Test API mode configuration.
    BridgeConfig config;
    config.mode = BridgeConfig::Mode::API;
    config.api.provider = "ollama";
    config.api.endpoint = "http://localhost:11434/v1/chat/completions";
    config.api.model = "qwen3.6:35b-a3b";

    LlmBridge bridge(config);
    assert(bridge.current_mode() == BridgeConfig::Mode::API);
    std::cout << "PASS: API mode configured correctly\n";

    // Test CLI mode configuration.
    config.mode = BridgeConfig::Mode::CLI;
    config.cli.provider = "claude";
    config.cli.command = "claude --output-format json";
    LlmBridge cli_bridge(config);
    assert(cli_bridge.current_mode() == BridgeConfig::Mode::CLI);
    std::cout << "PASS: CLI mode configured correctly\n";

    // Test AUTO mode (auto-detect).
    config.mode = BridgeConfig::Mode::AUTO;
    LlmBridge auto_bridge(config);
    std::cout << "PASS: AUTO mode can be constructed\n";

    std::cout << "All LLM bridge tests passed.\n";
    return 0;
}
