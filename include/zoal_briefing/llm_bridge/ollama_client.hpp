#ifndef ZOAL_BRIEFING_LLM_BRIDGE_OLLAMA_CLIENT_HPP
#define ZOAL_BRIEFING_LLM_BRIDGE_OLLAMA_CLIENT_HPP

#include "zoal_briefing/util/http_client.hpp"

#include <cstddef>
#include <functional>
#include <string>

namespace zoal_briefing::llm_bridge {

// A stand-in for the LLM bridge (DESIGN.md §5.7) until it exists: one Ollama
// server, one model, /api/chat streamed. Hardcoded for now.
struct OllamaConfig {
  std::string endpoint = "http://192.168.68.54:11434";
  std::string model = "qwen3.6:35b-a3b";
  long connect_timeout_seconds = 5;
  // Covers loading the model (~12 s cold) and generating a briefing.
  long timeout_seconds = 300;
  int context_tokens = 8192;
  double temperature = 0.3;
};

// The /api/chat body: thinking off (the briefing is spoken, not reasoned
// about out loud), the model kept loaded for the next briefing.
std::string ollama_chat_request(const OllamaConfig &config, const std::string &system,
                                const std::string &user, bool stream);

// Reads an /api/chat answer as it arrives: one JSON object per line when
// streaming ({"message":{"content":"..."},"done":false}), or a single object
// when not. Errors are Ollama's own {"error": ...} or a line that is not JSON.
class OllamaStream {
public:
  void feed(const char *data, std::size_t size);
  // The last line, which may have no newline after it.
  void finish();

  const std::string &text() const { return text_; }
  const std::string &error() const { return error_; }
  bool done() const { return done_; }

private:
  void line(const std::string &line);

  std::string pending_;
  std::string text_;
  std::string error_;
  bool done_ = false;
};

class OllamaClient {
public:
  OllamaClient(util::HttpClient &http, OllamaConfig config) : http_(http), cfg_(std::move(config)) {}

  struct Result {
    bool ok = false;
    std::string text;
    std::string error;
  };

  // The answer so far, each time it grows.
  using OnText = std::function<void(const std::string &text_so_far)>;

  // Blocking: worker threads only. Streams, and calls on_text as the answer
  // grows (on this thread).
  Result complete(const std::string &system, const std::string &user,
                  std::function<bool()> cancelled = {}, const OnText &on_text = {});

  const OllamaConfig &config() const { return cfg_; }

private:
  util::HttpClient &http_;
  OllamaConfig cfg_;
};

} // namespace zoal_briefing::llm_bridge

#endif // ZOAL_BRIEFING_LLM_BRIDGE_OLLAMA_CLIENT_HPP
