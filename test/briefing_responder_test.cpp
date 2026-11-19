#include "check.hpp"

#include "zoal_briefing/llm_bridge/briefing_generator.hpp"
#include "zoal_briefing/log.hpp"
#include "zoal_briefing/simbrief/plan_watcher.hpp"
#include "zoal_briefing/xplane/briefing_responder.hpp"

#include <nlohmann/json.hpp>

#include <fstream>
#include <sstream>
#include <string>
#include <vector>

using namespace zoal_briefing;
using audio::Reply;
using llm_bridge::BriefingGenerator;
using llm_bridge::BriefingKind;
using llm_bridge::BriefingStatus;

namespace {

std::string read_fixture(const char *name) {
  std::ifstream in(std::string(ZB_FIXTURES_DIR) + "/" + name);
  std::stringstream ss;
  ss << in.rdbuf();
  return ss.str();
}

bool contains(const std::string &haystack, const std::string &needle) {
  return haystack.find(needle) != std::string::npos;
}

// SimBrief and Ollama, by URL.
struct FakeServers : util::HttpClient {
  std::string ofp;
  bool llm_up = true;
  util::HttpResponse send(const util::HttpRequest &r) override {
    util::HttpResponse out;
    if (contains(r.url, "simbrief.com")) {
      out.status = 200;
      out.body = ofp;
    } else if (!llm_up) {
      out.error = "Couldn't connect to server";
    } else {
      out.status = 200;
      out.body = R"({"message":{"role":"assistant","content":"Runway two eight left."}})";
    }
    return out;
  }
};

// The plugin's wiring, without threads.
struct Rig {
  FakeServers http;
  BriefingGenerator briefings{http, {}};
  simbrief::PlanWatcher plans;
  xplane::BriefingResponder responder{plans, briefings};

  explicit Rig(const std::string &pilot)
      : plans(http, {pilot, 120.0, ""},
              [this](const fp::FlightPlan &p) { briefings.set_plan(p); }) {}
};

} // namespace

int main() {
  log::set_sink([](const std::string &) {});
  const std::string ofp = read_fixture("simbrief_ofp.json");

  // No SimBrief pilot: say what to do instead of waiting.
  {
    Rig r("");
    const Reply reply = r.responder.reply("anything");
    CHECK(reply.status == Reply::Status::READY && contains(reply.text, "SimBrief username"));
  }

  // The briefing a request asks for, by keyword.
  using xplane::pick_briefing;
  CHECK(pick_briefing("Give me the approach briefing.") == BriefingKind::APPROACH);
  CHECK(pick_briefing("Landing briefing for two four right") == BriefingKind::APPROACH);
  CHECK(pick_briefing("Descent briefing") == BriefingKind::DESCENT);
  CHECK(pick_briefing("arrival please") == BriefingKind::DESCENT);
  CHECK(pick_briefing("Pre-departure briefing") == BriefingKind::DEPARTURE);
  CHECK(pick_briefing("Before takeoff") == BriefingKind::DEPARTURE);
  CHECK(pick_briefing("taxi briefing") == BriefingKind::DEPARTURE);
  CHECK(pick_briefing("Pre-flight briefing") == BriefingKind::PREFLIGHT);
  CHECK(pick_briefing("Brief me.") == BriefingKind::PREFLIGHT); // anything else

  // Not checked yet, then generating, then the pre-flight briefing.
  {
    Rig r("123456");
    r.http.ofp = ofp;
    Reply reply = r.responder.reply("x");
    CHECK(reply.status == Reply::Status::PENDING && contains(reply.text, "Checking SimBrief"));

    r.plans.poll_once();
    reply = r.responder.reply("x");
    CHECK(reply.status == Reply::Status::PENDING && contains(reply.text, "still being prepared"));

    CHECK(r.briefings.generate_once()); // pre-flight: first in order
    reply = r.responder.reply("brief me");
    CHECK(reply.status == Reply::Status::READY && reply.text == "Runway two eight left.");
    // The descent briefing is not done yet, and is named in the stand-by.
    reply = r.responder.reply("descent briefing please");
    CHECK(reply.status == Reply::Status::PENDING &&
          reply.text == "Stand by, captain. The descent and arrival briefing is still being "
                        "prepared.");
  }

  // SimBrief does not know the pilot.
  {
    Rig r("nobody");
    r.http.ofp = R"({"fetch":{"status":"Error: Unknown UserID"}})";
    r.plans.poll_once();
    const Reply reply = r.responder.reply("x");
    CHECK(reply.status == Reply::Status::READY &&
          contains(reply.text, "could not get your flight plan"));
  }

  // The briefing failed: the pilot hears so, and asking retries it.
  {
    Rig r("123456");
    r.http.ofp = ofp;
    r.http.llm_up = false;
    r.plans.poll_once();
    CHECK(r.briefings.generate_once());
    const Reply reply = r.responder.reply("x");
    CHECK(reply.status == Reply::Status::READY &&
          contains(reply.text, "the pre-flight briefing could not be prepared"));
    CHECK(r.briefings.briefing(BriefingKind::PREFLIGHT).status == BriefingStatus::QUEUED);
  }

  // A briefing being written is STREAMING, its text so far.
  {
    struct Streaming : util::HttpClient {
      xplane::BriefingResponder *responder = nullptr;
      std::vector<Reply> seen;
      util::HttpResponse send(const util::HttpRequest &r) override {
        util::HttpResponse out;
        out.status = 200;
        if (contains(r.url, "simbrief.com")) {
          out.body = read_fixture("simbrief_ofp.json");
          return out;
        }
        const std::string line =
            R"({"message":{"content":"Good morning. Callsign SWA one two"},"done":false})"
            "\n";
        r.on_body(line.data(), line.size());
        seen.push_back(responder->reply("x"));
        return out;
      }
    } http;
    BriefingGenerator briefings(http, {});
    simbrief::PlanWatcher plans(http, {"123456", 120.0, ""},
                                [&](const fp::FlightPlan &p) { briefings.set_plan(p); });
    xplane::BriefingResponder responder(plans, briefings);
    http.responder = &responder;
    plans.poll_once();
    CHECK(briefings.generate_once());
    CHECK(http.seen.size() == 1);
    if (!http.seen.empty()) {
      CHECK(http.seen[0].status == Reply::Status::STREAMING);
      CHECK(http.seen[0].text == "Good morning. Callsign SWA one two");
    }
  }

  // A newer plan than the generator has: wait for its briefing, not the old one.
  {
    Rig r("123456");
    r.http.ofp = ofp;
    r.plans.poll_once();
    CHECK(r.briefings.generate_once());
    auto j = nlohmann::json::parse(ofp);
    j["params"]["request_id"] = "158099999";
    r.http.ofp = j.dump();
    r.plans.poll_once();
    CHECK(r.responder.reply("x").status == Reply::Status::PENDING);
  }

  log::reset_sink();
  return test_result("briefing_responder");
}
