#ifndef ZOAL_BRIEFING_LLM_BRIDGE_HPP
#define ZOAL_BRIEFING_LLM_BRIDGE_HPP

#include <string>
#include <optional>
#include <functional>

namespace zoal_briefing::llm {

// The routing modes for LLM bridge:
enum class Mode {
    API,  // HTTP/REST to OpenAI/Anthropic/Ollama/LM Studio
    CLI,  // Process spawn to Claude/Copilot/Codex via stdin/stdout
    AUTO  // Auto-detect based on config/availability
};

struct BridgeConfig {
    Mode mode = Mode::AUTO;
    
    // API mode settings
    struct ApiSettings {
        std::string provider = "ollama";       // openai, anthropic, ollama, lm_studio
        std::string endpoint;                  // e.g., http://localhost:11434/v1/chat/completions
        std::string model = "qwen3.6:35b-a3b";
        std::string api_key;                   // from OS keychain or encrypted file
    } api;

    // CLI mode settings
    struct CliSettings {
        std::string provider = "claude";      // claude, copilot, codex, custom
        std::string command;                  // e.g., "claude --output-format json"
        std::vector<std::string> env_tokens;  // env var names for API keys
        std::string prompt_template_path;     // ./prompts/cli_briefing.txt
    } cli;

    // Fallback settings
    struct FallbackSettings {
        double timeout_seconds = 5.0;
        int max_retries = 2;
    } fallback;
};

// The LLM bridge router — selects API vs CLI, routes the prompt, returns briefing text.
class LlmBridge {
public:
    explicit LlmBridge(const BridgeConfig& config);

    // Generate a structured briefing for a given input.
    using BriefingCallback = std::function<void(const std::string& result, 
                                                 const std::string& error)>;
    
    void generate(const std::string& system_prompt,
                  const std::string& user_input,
                  BriefingCallback cb);

    // Get current mode (after auto-detection).
    Mode current_mode() const;

private:
    BridgeConfig config_;
    Mode detected_mode_{Mode::AUTO};
};

} // namespace zoal_briefing::llm

#endif // ZOAL_BRIEFING_LLM_BRIDGE_HPP
