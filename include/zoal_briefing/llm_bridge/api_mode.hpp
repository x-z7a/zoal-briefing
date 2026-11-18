#ifndef ZOAL_BRIEFING_API_MODE_HPP
#define ZOAL_BRIEFING_API_MODE_HPP

#include <string>
#include <optional>
#include <functional>
#include <chrono>
#include <nlohmann/json.hpp>

namespace zoal_briefing::llm {

// HTTP/REST client for OpenAI-compatible, Anthropic, Ollama, or LM Studio endpoints.
class ApiMode {
public:
    ApiMode(const std::string& endpoint = "", 
            const std::string& model = "qwen3.6:35b-a3b",
            const std::string& api_key = "");
    
    // Send a chat completion request synchronously and return the assistant's response.
    // Returns nullopt on any failure (will trigger CLI fallback in LlmBridge).
    std::optional<std::string> send_completion(const std::string& system_prompt,
                                                const std::string& user_message);
    
    // Send a streaming chat completion request with a callback for each chunk.
    void send_completion_streaming(
        const std::string& system_prompt,
        const std::string& user_message,
        std::function<void(const std::string& chunk)> on_chunk,
        std::function<void(const std::string& error)> on_error);

private:
    // Helper to health-check the endpoint.
    static bool health_check(const std::string& endpoint, double timeout_sec = 2.0);
    
    // Build JSON request body for the API.
    static void build_request_body(const std::string& system_prompt,
                                    const std::string& user_message,
                                    const std::string& model,
                                    nlohmann::json& out);
    
    // Parse LLM response from various provider formats.
    std::optional<std::string> parse_response(const std::string& raw_json) const;
    
    std::string endpoint_;
    std::string model_;
    std::string api_key_;
};

} // namespace zoal_briefing::llm

#endif // ZOAL_BRIEFING_API_MODE_HPP
