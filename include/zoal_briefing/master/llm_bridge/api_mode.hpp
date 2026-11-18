#ifndef ZOAL_BRIEFING_API_MODE_HPP
#define ZOAL_BRIEFING_API_MODE_HPP

#include <string>
#include <optional>

namespace zoal_briefing::llm {

// HTTP/REST client for OpenAI-compatible, Anthropic, Ollama, or LM Studio endpoints.
class ApiMode {
public:
    ApiMode(const std::string& endpoint, 
            const std::string& model,
            const std::string& api_key);
    
    // Send a chat completion request and return the assistant's response.
    std::optional<std::string> send_completion(const std::string& system_prompt,
                                                const std::string& user_message);

private:
    std::string endpoint_;
    std::string model_;
    std::string api_key_;
};

} // namespace zoal_briefing::llm

#endif // ZOAL_BRIEFING_API_MODE_HPP
