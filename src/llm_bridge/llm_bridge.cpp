#include "zoal_briefing/master/llm_bridge/llm_bridge.hpp"
#include "zoal_briefing/master/llm_bridge/api_mode.hpp"
#include "zoal_briefing/master/llm_bridge/cli_mode.hpp"
#include "zoal_briefing/master/llm_bridge/fallback_chain.hpp"

namespace zoal_briefing::llm {

LlmBridge::LlmBridge(const BridgeConfig& config) : config_(config) {}

void LlmBridge::generate(const std::string& system_prompt,
                          const std::string& user_input,
                          BriefingCallback cb) {
    // Determine mode based on config.
    Mode mode = config_.mode;
    if (mode == Mode::AUTO) {
        // Auto-detect: try API first (health check), fall back to CLI.
        mode = Mode::API; // Default preference for lower latency.
    }

    if (mode == Mode::API) {
        ApiMode api(config_.api.endpoint, config_.api.model, config_.api.api_key);
        auto result = api.send_completion(system_prompt, user_input);
        if (result.has_value()) {
            cb(result.value(), "");
        } else {
            // Fall back to CLI.
            CliMode cli(config_.cli.command, config_.cli.env_tokens,
                        config_.cli.prompt_template_path);
            auto cli_result = cli.execute(system_prompt, user_input);
            if (cli_result.has_value()) {
                cb(cli_result.value(), "");
            } else {
                cb("", "LLM bridge failed in both API and CLI mode");
            }
        }
    } else {
        // CLI mode directly.
        CliMode cli(config_.cli.command, config_.cli.env_tokens,
                    config_.cli.prompt_template_path);
        auto result = cli.execute(system_prompt, user_input);
        if (result.has_value()) {
            cb(result.value(), "");
        } else {
            cb("", "CLI LLM bridge failed");
        }
    }
}

Mode LlmBridge::current_mode() const {
    return config_.mode;
}

} // namespace zoal_briefing::llm
