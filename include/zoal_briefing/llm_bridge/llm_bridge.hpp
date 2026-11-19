#pragma once

#include <string>
#include <functional>
#include <memory>
#include <mutex>
#include <vector>
#include <optional>

namespace zoal_briefing::llm_bridge {

enum class Mode {
    API,
    CLI,
    AUTO
};

struct BridgeConfig {
    Mode mode = Mode::AUTO;
    
    struct APIConfig {
        std::string endpoint = "http://localhost:11434/v1/chat/completions";
        std::string model = "qwen3.6:35b-a3b";
        std::string api_key = "";
    } api;
    
    struct CLIConfig {
        std::string command = "claude --output-format json";
        std::string prompt_template_path = "./prompts/cli_briefing.txt";
        std::vector<std::string> env_tokens = {"ANTHROPIC_API_KEY"};
    } cli;
};

using BriefingCallback = std::function<void(const std::string& result, const std::string& error)>;

class LlmBridge {
public:
    LlmBridge(const BridgeConfig& config);
    
    // Synchronous generate with callback (runs asynchronously in background).
    void generate(const std::string& system_prompt,
                  const std::string& user_input,
                  BriefingCallback cb);
    
    // Pure async variant (no callback, runs fire-and-forget).
    void generate_async(const std::string& system_prompt,
                        const std::string& user_input);
    
    // Query the configured mode.
    Mode current_mode() const;
    
    // Check if at least one backend is available.
    bool is_available() const;

private:
    BridgeConfig config_;
    mutable std::mutex mtx_;
};

} // namespace zoal_briefing::llm_bridge
