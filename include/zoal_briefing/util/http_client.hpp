#ifndef ZOAL_BRIEFING_UTIL_HTTP_CLIENT_HPP
#define ZOAL_BRIEFING_UTIL_HTTP_CLIENT_HPP

#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace zoal_briefing::util {

struct HttpRequest {
  std::string method = "GET";
  std::string url;
  std::vector<std::pair<std::string, std::string>> headers;
  std::string body;
  long connect_timeout_seconds = 10;
  long timeout_seconds = 30;
  // Polled while the request runs; returning true aborts it (plugin disable,
  // or the plan it was for was replaced).
  std::function<bool()> cancelled;
  // Streaming: the body is handed here as it arrives, and HttpResponse::body
  // stays empty. A client that cannot stream may instead return the whole
  // body in HttpResponse::body, so callers handle both.
  std::function<void(const char *data, std::size_t size)> on_body;
};

struct HttpResponse {
  int status = 0; // HTTP status; 0 when no response arrived
  std::string body;
  std::string error; // transport error ("connection refused", "cancelled"), else empty

  bool ok() const { return error.empty() && status >= 200 && status < 300; }
};

// Blocking HTTP, called from worker threads only. The core uses this
// interface; tests pass fakes, the plugin passes make_http_client().
class HttpClient {
public:
  virtual ~HttpClient() = default;
  virtual HttpResponse send(const HttpRequest &request) = 0;
};

// libcurl where the build found it; otherwise a client whose every request
// fails with "HTTP not available in this build". Defined in the HTTP library
// (src/util/), not the core.
std::unique_ptr<HttpClient> make_http_client();

} // namespace zoal_briefing::util

#endif // ZOAL_BRIEFING_UTIL_HTTP_CLIENT_HPP
