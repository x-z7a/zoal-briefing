#include "zoal_briefing/util/http_client.hpp"

#include <curl/curl.h>

#include <mutex>

namespace zoal_briefing::util {

namespace {

struct Transfer {
  const HttpRequest *request;
  HttpResponse *response;
};

size_t on_body(char *data, size_t size, size_t count, void *user) {
  const auto *t = static_cast<Transfer *>(user);
  if (t->request->on_body) {
    t->request->on_body(data, size * count);
  } else {
    t->response->body.append(data, size * count);
  }
  return size * count;
}

int on_progress(void *user, curl_off_t, curl_off_t, curl_off_t, curl_off_t) {
  const auto *request = static_cast<const HttpRequest *>(user);
  return (request->cancelled && request->cancelled()) ? 1 : 0;
}

class CurlHttpClient : public HttpClient {
public:
  HttpResponse send(const HttpRequest &request) override {
    HttpResponse response;
    CURL *curl = curl_easy_init();
    if (!curl) {
      response.error = "curl_easy_init failed";
      return response;
    }

    curl_slist *headers = nullptr;
    for (const auto &[name, value] : request.headers) {
      std::string line = name;
      line += ": ";
      line += value;
      headers = curl_slist_append(headers, line.c_str());
    }

    curl_easy_setopt(curl, CURLOPT_URL, request.url.c_str());
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L); // worker threads
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 5L);
    curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, ""); // gzip: SimBrief OFPs are large
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "zoal-briefing/1.0");
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, request.connect_timeout_seconds);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, request.timeout_seconds);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, on_body);
    Transfer transfer{&request, &response};
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &transfer);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, on_progress);
    curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &request);
    if (headers) curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    if (request.method == "POST") {
      curl_easy_setopt(curl, CURLOPT_POST, 1L);
      curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request.body.c_str());
      curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE_LARGE,
                       static_cast<curl_off_t>(request.body.size()));
    } else if (request.method != "GET") {
      curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, request.method.c_str());
    }

    const CURLcode rc = curl_easy_perform(curl);
    if (rc == CURLE_ABORTED_BY_CALLBACK) {
      response.error = "cancelled";
    } else if (rc != CURLE_OK) {
      response.error = curl_easy_strerror(rc);
    }
    long status = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
    response.status = static_cast<int>(status);

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    return response;
  }
};

} // namespace

std::unique_ptr<HttpClient> make_http_client() {
  static std::once_flag init;
  std::call_once(init, [] { curl_global_init(CURL_GLOBAL_DEFAULT); });
  return std::make_unique<CurlHttpClient>();
}

} // namespace zoal_briefing::util
