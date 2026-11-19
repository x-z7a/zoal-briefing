#include "zoal_briefing/simbrief/client.hpp"

#include "zoal_briefing/simbrief/ofp.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>

namespace zoal_briefing::simbrief {

namespace {

std::string url_encode(const std::string &s) {
  std::string out;
  for (const char ch : s) {
    const auto c = static_cast<unsigned char>(ch);
    if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
      out += static_cast<char>(c);
    } else {
      char buf[4];
      std::snprintf(buf, sizeof(buf), "%%%02X", c);
      out += buf;
    }
  }
  return out;
}

std::string trim(const std::string &s) {
  const auto first = s.find_first_not_of(" \t\r\n");
  if (first == std::string::npos) return {};
  const auto last = s.find_last_not_of(" \t\r\n");
  return s.substr(first, last - first + 1);
}

} // namespace

std::string latest_ofp_url(const std::string &pilot) {
  const std::string id = trim(pilot);
  if (id.empty()) return {};
  const bool numeric =
      std::all_of(id.begin(), id.end(), [](unsigned char c) { return std::isdigit(c) != 0; });
  return std::string("https://www.simbrief.com/api/xml.fetcher.php?") +
         (numeric ? "userid=" : "username=") + url_encode(id) + "&json=1";
}

FetchResult fetch_latest(util::HttpClient &http, const std::string &pilot,
                         std::function<bool()> cancelled) {
  FetchResult result;
  util::HttpRequest request;
  request.url = latest_ofp_url(pilot);
  if (request.url.empty()) {
    result.error = "no SimBrief username or pilot ID set";
    return result;
  }
  request.timeout_seconds = 30;
  request.cancelled = std::move(cancelled);

  util::HttpResponse response = http.send(request);
  if (!response.error.empty()) {
    result.error = response.error;
    return result;
  }
  // SimBrief answers errors (unknown user, no OFP yet) with a 400 whose JSON
  // carries the reason in fetch.status, so parse the body whatever the status.
  std::string parse_error;
  result.plan = parse_ofp(response.body, parse_error);
  if (!result.plan) {
    // SimBrief's own reason says more than the status code.
    const bool from_simbrief = parse_error.rfind("SimBrief: ", 0) == 0;
    result.error = (from_simbrief || (response.status >= 200 && response.status < 300))
                       ? parse_error
                       : "HTTP " + std::to_string(response.status) + ": " + parse_error;
    return result;
  }
  result.raw = std::move(response.body);
  return result;
}

} // namespace zoal_briefing::simbrief
