#include "zoal_briefing/util/http_client.hpp"

// The build found no libcurl (Windows, for now): every request fails, so the
// plugin still loads and says why it has no flight plan.

namespace zoal_briefing::util {

namespace {

class NoHttpClient : public HttpClient {
public:
  HttpResponse send(const HttpRequest &) override {
    HttpResponse response;
    response.error = "HTTP not available in this build (libcurl not found)";
    return response;
  }
};

} // namespace

std::unique_ptr<HttpClient> make_http_client() { return std::make_unique<NoHttpClient>(); }

} // namespace zoal_briefing::util
