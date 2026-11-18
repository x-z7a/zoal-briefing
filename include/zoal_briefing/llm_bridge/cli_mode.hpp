#ifndef ZOAL_BRIEFING_CLI_MODE_HPP
#define ZOAL_BRIEFING_CLI_MODE_HPP

#include <string>
#include <vector>
#include <optional>

namespace zoal_briefing::llm {

// Process spawn for Claude, Copilot, Codex, or custom LLM CLIs.
class CliMode {
public:
    CliMode(const std::string& command,
            const std::vector<std::string>& env_tokens,
            const std::string& prompt_template_path);
    
    // Execute the CLI and return stdout as the briefing text.
    std::optional<std::string> execute(const std::string& system_prompt,
                                        const std::string& user_message);

private:
    std::string command_;
    std::vector<std::string> env_tokens_;
    std::string prompt_template_path_;
};

} // namespace zoal_briefing::llm

#endif // ZOAL_BRIEFING_CLI_MODE_HPP
