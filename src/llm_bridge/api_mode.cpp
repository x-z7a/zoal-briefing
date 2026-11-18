#include "zoal_briefing/master/llm_bridge/api_mode.hpp"
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <sstream>
#include <chrono>
#include <thread>

using json = nlohmann::json;

namespace zoal_briefing::llm {

// ── Helper: curl write callback to string buffer ────────────────────────────
static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* user) {
    auto* out = static_cast<std::string*>(user);
    out->append(static_cast<char*>(contents), size * nmemb);
    return size * nmemb;
}

// ── Helper: health check the API endpoint ───────────────────────────────────
static bool health_check(const std::string& endpoint, double timeout_sec = 2.0) {
    CURL* curl = curl_easy_init();
    if (!curl) return false;
    
    std::string dummy;
    curl_easy_setopt(curl, CURLOPT_URL, (endpoint + "/health").c_str());
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout_sec);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &dummy);
    curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);  // HEAD request only
    
    auto res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    
    if (res == CURLE_OK || res == CURLE_UNSUPPORTED_PROTOCOL) {
        return true;  // /health may not exist on all providers, but connection is good
    }
    return false;
}

// ── Helper: build the Ollama-compatible JSON request ────────────────────────
static json build_request(const std::string& system_prompt,
                           const std::string& user_message,
                           const std::string& model,
                           const std::string& api_key) {
    json req;
    req["model"] = model.empty() ? "qwen3.6:35b-a3b" : model;
    req["stream"] = false;  // Non-streaming for simplicity
    
    json messages = json::array();
    
    if (!system_prompt.empty()) {
        json sys_msg;
        sys_msg["role"] = "system";
        sys_msg["content"] = system_prompt;
        messages.push_back(sys_msg);
    }
    
    json usr_msg;
    usr_msg["role"] = "user";
    usr_msg["content"] = user_message;
    messages.push_back(usr_msg);
    
    req["messages"] = messages;
    return req;
}

// ── Helper: extract assistant text from LLM JSON response ───────────────────
static std::optional<std::string> parse_response(const std::string& raw_json) {
    if (raw_json.empty()) return std::nullopt;
    
    try {
        auto j = json::parse(raw_json, nullptr, false);
        
        // OpenAI / Ollama format: {"choices":[{"message":{"content":"..."}}]}
        if (j.contains("choices") && !j["choices"].empty()) {
            const auto& choices = j["choices"];
            if (choices[0].contains("message") && 
                choices[0]["message"].contains("content")) {
                return choices[0]["message"]["content"].get<std::string>();
            }
        }
        
        // Anthropic format: {"content":[{"text":"..."}]}
        if (j.contains("content") && j["content"].is_array()) {
            for (const auto& c : j["content"]) {
                if (c.contains("text")) return c["text"].get<std::string>();
            }
        }
        
        // Fallback: search for first "content" or "text" field recursively
        std::function<std::optional<std::string>(const json&)> find_text;
        find_text = [&](const json& obj) -> std::optional<std::string> {
            if (obj.contains("content") && obj["content"].is_string())
                return obj["content"].get<std::string>();
            if (obj.contains("text") && obj["text"].is_string())
                return obj["text"].get<std::string>();
            return std::nullopt;
        };
        return find_text(j);
        
    } catch (const std::exception&) {
        return std::nullopt;  // JSON parse failed
    }
}

// ── ApiMode Implementation ─────────────────────────────────────────────────
ApiMode::ApiMode(const std::string& endpoint,
                 const std::string& model,
                 const std::string& api_key)
    : endpoint_(endpoint), model_(model), api_key_(api_key) {
    
    // Initialize libcurl globally (thread-safe in init_once).
    static bool initialized = false;
    if (!initialized) {
        curl_global_init(CURL_GLOBAL_DEFAULT);
        initialized = true;
    }
}

std::optional<std::string> ApiMode::send_completion(
    const std::string& system_prompt,
    const std::string& user_message) {
    
    // Health check first (only if this is a new endpoint).
    if (!health_check(endpoint_, 2.0)) {
        return std::nullopt;  // Will trigger CLI fallback in LlmBridge
    }
    
    CURL* curl = curl_easy_init();
    if (!curl) return std::nullopt;
    
    auto req_json = build_request(system_prompt, user_message, model_, api_key_);
    std::string body_str = req_json.dump();
    std::string response_body;
    
    // Build request headers.
    curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    if (!api_key_.empty()) {
        auto auth_hdr = "Authorization: Bearer " + api_key_;
        headers = curl_slist_append(headers, auth_hdr.c_str());
    }
    
    // Configure the HTTP POST request.
    struct curl_slist* final_headers = headers;
    
    curl_easy_setopt(curl, CURLOPT_URL, endpoint_.c_str());
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body_str.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(body_str.size()));
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30.0);       // Max 30s for generation
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_body);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, final_headers);
    
    auto res = curl_easy_perform(curl);
    
    // Cleanup.
    curl_slist_free_all(final_headers);
    curl_easy_cleanup(curl);
    
    if (res != CURLE_OK) {
        return std::nullopt;  // HTTP failure → CLI fallback
    }
    
    // Parse the LLM response.
    return parse_response(response_body);
}

} // namespace zoal_briefing::llm
