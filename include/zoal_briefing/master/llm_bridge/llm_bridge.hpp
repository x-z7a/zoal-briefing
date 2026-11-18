#pragma once

#include <string>
#include <functional>
#include <memory>
#include <mutex>

namespace zoal_briefing::bridge {

enum class ProviderMode {
    API_MODE, // OpenAI / Anthropic / Ollama (REST)
    CLI_MODE, // Claude / Copilot / Codex (CLI execution)
    AUTO      // Health check -> API -> CLI -> Fallback
};

struct LLMConfig {
    std::string mode = "AUTO"; // "API", "CLI", "AUTO"
    
    struct APIConfig {
        std::string provider = "Ollama";
        std::string endpoint = "http://localhost:11434/v1/chat/completions";
        std::string model = "qwen3.6:35b-a3b";
        std::string api_key = ""; // Loaded from OS keychain / env
    } api;
    
    struct CLIConfig {
        std::string provider = "Claude"; // or "codex", "custom"
        std::string command = "claude --output-format json";
        std::string prompt_template_path = "./prompts/cli_briefing.txt";
        std::vector<std::string> env_tokens = {"ANTHROPIC_API_KEY"};
    } cli;
};

class LLMRouter {
public:
    LLMRouter() = default;
    
    void load_config(const std::string& config_path);
    
    using ResultCallback = std::function<void(bool success, const std::string& briefing_text)>;
    
    void generate_briefing(const std::string& system_prompt,
                           const std::string& user_context,
                           ResultCallback cb);

private:
    LLMConfig config_;
    mutable std::mutex mtx_;
    
    // Internal routing & fallback chain
    bool is_api_available() const;
    bool is_cli_available() const;
    
    void call_api(const std::string& system_prompt, const std::string& user_context, ResultCallback cb);
    void call_cli(const std::string& system_prompt, const std::string& user_context, ResultCallback cb);
    void use_cached_fallback(const std::string& system_prompt, ResultCallback cb);
};

} // namespace zoal_briefing::bridge
