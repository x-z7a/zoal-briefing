#include "zoal_briefing/master/llm_bridge/llm_bridge.hpp"
#include <iostream>
#include <fstream>
#include <nlohmann/json.hpp> // Conceptual dependency, standard in modern C++ plugins

namespace zoal_briefing::bridge {

void LLMRouter::load_config(const std::string& config_path) {
    std::lock_guard<std::mutex> lock(mtx_);
    // In production, parse briefingmaster.json using nlohmann/json
    // For scaffolding, we rely on defaults or a simple reader
    std::ifstream file(config_path);
    if (file.is_open()) {
        // json j; file >> j; config_.api.model = j["llm_bridge"]["api"]["model"].get<std::string>();
    }
}

bool LLMRouter::is_api_available() const {
    // Health check ping to endpoint + auth validation
    // Returns true if HTTP 200 OK and API key valid
    return true; // Stubbed
}

bool LLMRouter::is_cli_available() const {
    // Check if CLI binary exists in PATH and can execute --help
    std::string test_cmd = config_.cli.command + " --help";
    return (std::system(test_cmd.c_str()) == 0);
}

void LLMRouter::call_api(const std::string& sys_prompt, const std::string& ctx, ResultCallback cb) {
    // Construct JSON payload for OpenAI/Ollama compatible API
    // curl/httplib POST to config_.api.endpoint
    // Parse response -> extract choices[0].message.content
    std::cout << "[API] Calling " << config_.api.model << " at " << config_.api.endpoint << std::endl;
    cb(true, "Briefing via API: " + ctx);
}

void LLMRouter::call_cli(const std::string& sys_prompt, const std::string& ctx, ResultCallback cb) {
    // Write prompt to temp file with secure permissions
    // Spawn process using config_.cli.command, read stdin/stdout
    // Extract JSON/Text output
    std::cout << "[CLI] Executing " << config_.cli.provider << std::endl;
    cb(true, "Briefing via CLI: " + ctx);
}

void LLMRouter::use_cached_fallback(const std::string& sys_prompt, ResultCallback cb) {
    // Fallback to hardcoded or disk-cached template based on briefing phase
    // Ensures pilot gets *something* even if LLM completely fails
    std::cout << "[FALLBACK] Using cached weather/template briefing" << std::endl;
    cb(true, "Briefing (Offline Fallback): Check METAR/TAF manually.");
}

void LLMRouter::generate_briefing(const std::string& sys_prompt, const std::string& ctx, ResultCallback cb) {
    // Fallback Chain: Auto Mode -> API -> CLI -> Cache
    if (config_.mode == "API" || config_.mode == "AUTO") {
        if (is_api_available()) {
            call_api(sys_prompt, ctx, cb);
            return;
        } else {
            std::cerr << "[WARN] API unavailable, switching to CLI fallback..." << std::endl;
        }
    }

    if (config_.mode == "CLI" || config_.mode == "AUTO") {
        if (is_cli_available()) {
            call_cli(sys_prompt, ctx, cb);
            return;
        } else {
            std::cerr << "[WARN] CLI unavailable, switching to offline cache..." << std::endl;
        }
    }

    // Ultimate fallback: ensures briefing is never completely silent if LLM fails
    use_cached_fallback(sys_prompt, cb);
}

} // namespace zoal_briefing::bridge
