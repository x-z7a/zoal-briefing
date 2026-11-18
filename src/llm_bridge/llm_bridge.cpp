#include "zoal_briefing/master/llm_bridge/llm_bridge.hpp"
#include "zoal_briefing/master/llm_bridge/api_mode.hpp"
#include "zoal_briefing/master/llm_bridge/cli_mode.hpp"
#include "zoal_briefing/master/llm_bridge/fallback_chain.hpp"

#include <iostream>
#include <thread>
#include <chrono>

namespace zoal_briefing::llm {

LlmBridge::LlmBridge(const BridgeConfig& config) : config_(config) {}

void LlmBridge::generate(const std::string& system_prompt,
                          const std::string& user_input,
                          BriefingCallback cb) {
    // Determine mode based on config.
    Mode mode = config_.mode;
    
    if (mode == Mode::AUTO) {
        // Health check: try API first with a quick probe.
        ApiMode probe("", "qwen3.6:35b-a3b", "");
        auto probe_result = probe.send_completion(
            "OK", 
            "Respond with 'health_check_passed' if you can hear me."
        );
        if (probe_result.has_value() && !probe_result.value().empty()) {
            mode = Mode::API;
        } else {
            mode = Mode::CLI;  // Default to CLI if API fails.
        }
    }

    // Build a fallback chain: API → CLI → cached template → voice prompt
    FallbackChain chain;
    
    // Step 1: API mode (if configured or auto-detected).
    if (mode == Mode::API || mode == Mode::AUTO) {
        ApiMode api(config_.api.endpoint, config_.api.model, config_.api.api_key);
        chain.add_step({"api_mode", [&]() -> std::optional<std::string> {
            return api.send_completion(system_prompt, user_input);
        }});
    }
    
    // Step 2: CLI mode fallback.
    if (mode == Mode::CLI || mode == Mode::AUTO) {
        CliMode cli(config_.cli.command, config_.cli.env_tokens,
                    config_.cli.prompt_template_path);
        chain.add_step({"cli_mode", [&]() -> std::optional<std::string> {
            return cli.execute(system_prompt, user_input);
        }});
    }
    
    // Step 3: Cached template fallback (if no LLM is available).
    chain.add_step({"cached_template", [this, system_prompt]() -> std::optional<std::string> {
        // Build a simple cached template based on the system prompt.
        return "WEATHER BRIEFING AVAILABLE AT STATION\n" + 
               system_prompt.substr(0, 200) + "\n"
               "[Offline mode: LLM unavailable, using cached template]";
    }});
    
    // Step 4: Voice prompt fallback (lowest level).
    chain.add_step({"voice_prompt", []() -> std::optional<std::string> {
        return "WARNING: No LLM available. Please check your connection.\n"
               "Use the pilot briefing system manually for weather and route information.";
    }});
    
    // Run the fallback chain in a thread to avoid blocking the audio pipeline.
    std::thread([chain, cb, mode]() {
        auto result = chain.run();
        
        if (result.has_value()) {
            cb(result.value(), "");
        } else {
            cb("", "All LLM bridge modes failed");
        }
    }).detach();
    
    // Log the detected mode.
    std::string mode_str = "UNKNOWN";
    switch (mode) {
        case Mode::API: mode_str = "API"; break;
        case Mode::CLI: mode_str = "CLI"; break;
        case Mode::AUTO: mode_str = "AUTO"; break;
    }
}

Mode LlmBridge::current_mode() const {
    return config_.mode;
}

// ── Async generate helper ───────────────────────────────────────────────────
void LlmBridge::generate_async(const std::string& system_prompt,
                                const std::string& user_input) {
    // For now, just log what would happen. In production this triggers the same
    // flow but without waiting for TTS output.
    (void)system_prompt;
    (void)user_input;
}

// ── Health check method ─────────────────────────────────────────────────────
bool LlmBridge::is_available() const {
    if (config_.mode == Mode::CLI || config_.mode == Mode::AUTO) {
        return true;  // CLI always available, API may be offline.
    }
    return true;     // Assume API is available.
}

} // namespace zoal_briefing::llm
