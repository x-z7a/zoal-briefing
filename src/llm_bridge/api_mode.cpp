#include "zoal_briefing/master/llm_bridge/api_mode.hpp"

namespace zoal_briefing::llm {

ApiMode::ApiMode(const std::string& endpoint,
                 const std::string& model,
                 const std::string& api_key)
    : endpoint_(endpoint), model_(model), api_key_(api_key) {}

std::optional<std::string> ApiMode::send_completion(
    const std::string& system_prompt,
    const std::string& user_message) {
    
    // TODO: Implement HTTP POST to the LLM provider endpoint.
    // This would use curl/httplib with the appropriate API format.
    // For now, return nullopt as a placeholder.
    (void)system_prompt;
    (void)user_message;
    return std::nullopt;
}

} // namespace zoal_briefing::llm
