#include "zoal_briefing/llm_bridge/cli_mode.hpp"

namespace zoal_briefing::llm_bridge {

CliMode::CliMode(const std::string& command,
                 const std::vector<std::string>& env_tokens,
                 const std::string& prompt_template_path)
    : command_(command), env_tokens_(env_tokens),
      prompt_template_path_(prompt_template_path) {}

std::optional<std::string> CliMode::execute(
    const std::string& system_prompt,
    const std::string& user_message) {
    
    // TODO: Spawn process with the command, inject env tokens,
    // write prompt to temp file or stdin, read stdout.
    (void)system_prompt;
    (void)user_message;
    return std::nullopt;
}

} // namespace zoal_briefing::llm_bridge
