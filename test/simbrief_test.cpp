#include "check.hpp"

#include "zoal_briefing/log.hpp"
#include "zoal_briefing/simbrief/client.hpp"
#include "zoal_briefing/simbrief/ofp.hpp"
#include "zoal_briefing/simbrief/plan_watcher.hpp"

#include <nlohmann/json.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

using namespace zoal_briefing;

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

struct OneShotHttp : util::HttpClient {
  util::HttpResponse response;
  util::HttpRequest last;
  util::HttpResponse send(const util::HttpRequest &r) override {
    last = r;
    return response;
  }
};

} // namespace

int main() {
  log::set_sink([](const std::string &) {});
  const std::string ofp = read_fixture("simbrief_ofp.json");
  CHECK(!ofp.empty());

  // The fixture maps field by field.
  {
    std::string error;
    const auto plan = simbrief::parse_ofp(ofp, error);
    CHECK(plan.has_value());
    if (plan) {
      CHECK(plan->request_id == "158012345");
      CHECK(plan->generated_at == 1795000000);
      CHECK(plan->callsign == "SWA1234");
      CHECK(plan->aircraft_icao == "B738");
      CHECK(plan->origin.icao == "KSFO" && plan->origin.planned_runway == "28L");
      CHECK(plan->origin.elevation_ft == 13);
      CHECK(contains(plan->origin.metar, "29014KT"));
      CHECK(plan->destination.icao == "KLAX" && plan->destination.planned_runway == "24R");
      CHECK(plan->destination.transition_level_ft == 18000);
      CHECK(plan->alternates.size() == 1); // a lone object, not an array
      CHECK(!plan->alternates.empty() && plan->alternates[0].icao == "KONT");
      CHECK(!plan->alternates.empty() && plan->alternates[0].taf.empty()); // {} is empty
      CHECK(plan->sid == "SSTIK4" && plan->sid_transition.empty());
      CHECK(plan->star == "SADDE8" && plan->star_transition == "GMN");
      CHECK(plan->initial_altitude_ft == 33000);
      CHECK(plan->avg_wind_component_kt == -18);
      CHECK(plan->ete_seconds == 4380);
      CHECK(plan->fuel.units == "kgs");
      CHECK(plan->fuel.plan_ramp == 7696 && plan->fuel.trip == 4310);
      CHECK(plan->weights.tow == 63119 && plan->weights.max_ldw == 66360);
      CHECK(plan->navlog.size() == 9);
      CHECK(!plan->navlog.empty() && plan->navlog[0].procedure && plan->navlog[0].via == "SSTIK4");
    }
  }

  // Older OFPs without sid_ident/star_ident: the procedure fixes name them.
  {
    nlohmann::json j = nlohmann::json::parse(ofp);
    j["general"].erase("sid_ident");
    j["general"].erase("star_ident");
    j["alternate"] = nlohmann::json::array({j["alternate"], {{"icao_code", "KBUR"}}});
    std::string error;
    const auto plan = simbrief::parse_ofp(j.dump(), error);
    CHECK(plan && plan->sid == "SSTIK4" && plan->star == "SADDE8");
    CHECK(plan && plan->alternates.size() == 2);
  }

  // SimBrief's own errors come through.
  {
    std::string error;
    CHECK(!simbrief::parse_ofp(R"({"fetch":{"status":"Error: Unknown UserID"}})", error));
    CHECK(error == "SimBrief: Error: Unknown UserID");
    CHECK(!simbrief::parse_ofp("<html>", error));
    CHECK(!simbrief::parse_ofp(R"({"fetch":{"status":"Success"}})", error));
  }

  // Numeric is a pilot ID, anything else a username (encoded).
  CHECK(simbrief::latest_ofp_url("123456") ==
        "https://www.simbrief.com/api/xml.fetcher.php?userid=123456&json=1");
  CHECK(simbrief::latest_ofp_url(" Captain Z ") ==
        "https://www.simbrief.com/api/xml.fetcher.php?username=Captain%20Z&json=1");
  CHECK(simbrief::latest_ofp_url("  ").empty());

  // fetch_latest: SimBrief's 400 carries the reason in the body.
  {
    OneShotHttp http;
    http.response.status = 400;
    http.response.body = R"({"fetch":{"status":"Error: Unknown UserID"}})";
    const auto r = simbrief::fetch_latest(http, "nobody");
    CHECK(!r.plan);
    CHECK(contains(r.error, "Unknown UserID"));
    CHECK(contains(http.last.url, "username=nobody"));

    http.response.status = 200;
    http.response.body = ofp;
    const auto ok = simbrief::fetch_latest(http, "nobody");
    CHECK(ok.plan && ok.error.empty() && ok.raw == ofp);

    CHECK(!simbrief::fetch_latest(http, "").plan);
  }

  // PlanWatcher: a new request_id is a new plan, handed over once.
  {
    struct Servers : util::HttpClient {
      std::string body;
      int calls = 0;
      util::HttpResponse send(const util::HttpRequest &) override {
        ++calls;
        util::HttpResponse r;
        r.status = 200;
        r.body = body;
        return r;
      }
    } servers;
    servers.body = ofp;
    std::vector<std::string> handed;
    const auto on_new = [&](const fp::FlightPlan &p) { handed.push_back(p.request_id); };

    // No pilot: nothing fetched, the panel says what to do.
    {
      simbrief::PlanWatcher w(servers, {}, on_new);
      w.poll_once();
      CHECK(servers.calls == 0 && !w.status().pilot_set);
      CHECK(contains(w.status().text, "Settings"));
    }

    namespace fs = std::filesystem;
    const fs::path cache =
        fs::temp_directory_path() /
        ("zb_ofp_test_" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".json");
    simbrief::WatcherConfig cfg;
    cfg.pilot = "123456";
    cfg.cache_path = cache.string();
    {
      simbrief::PlanWatcher w(servers, cfg, on_new);
      CHECK(!w.status().checked);
      w.poll_once();
      const auto st = w.status();
      CHECK(st.checked && !st.fetching && st.plan && st.plan->request_id == "158012345");
      CHECK(contains(st.text, "SWA1234 KSFO to KLAX (OFP 158012345), fetched"));
      CHECK(handed.size() == 1);

      w.poll_once(); // the same OFP
      CHECK(handed.size() == 1 && contains(w.status().text, "checked"));

      auto j = nlohmann::json::parse(ofp);
      j["params"]["request_id"] = "158099999";
      servers.body = j.dump();
      w.poll_once();
      CHECK(handed.size() == 2 && handed.back() == "158099999");

      servers.body = R"({"fetch":{"status":"Error: Unknown UserID"}})";
      w.poll_once(); // a failed check keeps the plan
      const auto kept = w.status().plan;
      CHECK(kept && kept->request_id == "158099999");
      CHECK(contains(w.status().text, "Unknown UserID"));
    }
    // A restart starts from the cached OFP, and hands it over.
    {
      simbrief::PlanWatcher w(servers, cfg, on_new);
      w.load_cache();
      const auto kept = w.status().plan;
      CHECK(kept && kept->request_id == "158099999");
      CHECK(contains(w.status().text, "(cached)"));
      CHECK(handed.size() == 3 && handed.back() == "158099999");
    }
    std::error_code ec;
    fs::remove(cache, ec);

    // The thread checks at start.
    {
      servers.body = ofp;
      simbrief::PlanWatcher w(servers, {"123456", 120.0, ""}, on_new);
      w.start();
      for (int i = 0; i < 200 && !w.status().checked; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
      }
      CHECK(w.status().plan.has_value());
      w.stop();
    }
  }

  log::reset_sink();
  return test_result("simbrief");
}
