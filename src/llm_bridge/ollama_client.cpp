#include "zoal_briefing/llm_bridge/ollama_client.hpp"

#include <nlohmann/json.hpp>

namespace zoal_briefing::llm_bridge {

using json = nlohmann::json;

std::string ollama_chat_request(const OllamaConfig &config, const std::string &system,
                                const std::string &user, bool stream) {
  json body = {
      {"model", config.model},
      {"stream", stream},
      {"think", false},
      {"keep_alive", "30m"},
      {"messages", json::array({{{"role", "system"}, {"content", system}},
                                {{"role", "user"}, {"content", user}}})},
      {"options", {{"temperature", config.temperature}, {"num_ctx", config.context_tokens}}},
  };
  return body.dump();
}

void OllamaStream::feed(const char *data, std::size_t size) {
  pending_.append(data, size);
  std::size_t start = 0;
  for (std::size_t nl = pending_.find('\n'); nl != std::string::npos;
       nl = pending_.find('\n', start)) {
    line(pending_.substr(start, nl - start));
    start = nl + 1;
  }
  pending_.erase(0, start);
}

void OllamaStream::finish() {
  if (!pending_.empty()) line(pending_);
  pending_.clear();
}

void OllamaStream::line(const std::string &line) {
  if (line.find_first_not_of(" \t\r") == std::string::npos || !error_.empty()) return;
  const json j = json::parse(line, nullptr, false);
  if (j.is_discarded() || !j.is_object()) {
    error_ = "Ollama did not return JSON";
    return;
  }
  const auto err = j.find("error");
  if (err != j.end() && err->is_string()) {
    error_ = "Ollama: " + err->get<std::string>();
    return;
  }
  const auto message = j.find("message");
  if (message != j.end() && message->is_object()) {
    const auto content = message->find("content");
    if (content != message->end() && content->is_string()) text_ += content->get<std::string>();
  }
  const auto done = j.find("done");
  if (done != j.end() && done->is_boolean() && done->get<bool>()) done_ = true;
}

OllamaClient::Result OllamaClient::complete(const std::string &system, const std::string &user,
                                            std::function<bool()> cancelled,
                                            const OnText &on_text) {
  util::HttpRequest request;
  request.method = "POST";
  request.url = cfg_.endpoint + "/api/chat";
  request.headers = {{"Content-Type", "application/json"}};
  request.body = ollama_chat_request(cfg_, system, user, true);
  request.connect_timeout_seconds = cfg_.connect_timeout_seconds;
  request.timeout_seconds = cfg_.timeout_seconds;
  request.cancelled = std::move(cancelled);

  OllamaStream stream;
  request.on_body = [&](const char *data, std::size_t size) {
    const std::size_t before = stream.text().size();
    stream.feed(data, size);
    if (on_text && stream.text().size() != before) on_text(stream.text());
  };

  const util::HttpResponse response = http_.send(request);
  // A client that cannot stream returns the whole body instead.
  if (!response.body.empty()) {
    stream.feed(response.body.data(), response.body.size());
  }
  stream.finish();

  Result result;
  if (!response.error.empty()) {
    result.error = response.error + " (" + cfg_.endpoint + ")";
    return result;
  }
  if (!stream.error().empty()) {
    result.error = response.status == 200
                       ? stream.error()
                       : "HTTP " + std::to_string(response.status) + ": " + stream.error();
    return result;
  }
  if (response.status != 200) {
    result.error = "HTTP " + std::to_string(response.status);
    return result;
  }
  if (stream.text().find_first_not_of(" \t\r\n") == std::string::npos) {
    result.error = "Ollama returned an empty answer";
    return result;
  }
  result.text = stream.text();
  result.ok = true;
  return result;
}

} // namespace zoal_briefing::llm_bridge
