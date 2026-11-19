#include "check.hpp"

#include "zoal_briefing/llm_bridge/briefing_generator.hpp"
#include "zoal_briefing/llm_bridge/briefing_prompts.hpp"
#include "zoal_briefing/llm_bridge/ollama_client.hpp"
#include "zoal_briefing/log.hpp"
#include "zoal_briefing/simbrief/ofp.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

using namespace zoal_briefing;
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

// An Ollama server that answers with the prompt's first line, or is down.
struct FakeOllama : util::HttpClient {
  std::mutex mutex;
  bool up = true;
  std::vector<std::string> prompts; // the user message of every call

  util::HttpResponse send(const util::HttpRequest &r) override {
    std::lock_guard<std::mutex> lock(mutex);
    util::HttpResponse out;
    const auto body = nlohmann::json::parse(r.body);
    const std::string user = body["messages"][1]["content"];
    prompts.push_back(user);
    if (!up) {
      out.error = "Couldn't connect to server";
      return out;
    }
    out.status = 200;
    out.body = nlohmann::json{{"message",
                               {{"role", "assistant"},
                                {"content", "**Briefing.** " + user.substr(0, user.find('\n'))}}}}
                   .dump();
    return out;
  }
};

fp::FlightPlan with_id(fp::FlightPlan plan, const std::string &id) {
  plan.request_id = id;
  return plan;
}

} // namespace

int main() {
  log::set_sink([](const std::string &) {});
  const std::string ofp = read_fixture("simbrief_ofp.json");
  std::string parse_error;
  const auto parsed = simbrief::parse_ofp(ofp, parse_error);
  CHECK(parsed.has_value());
  if (!parsed) return test_result("briefing_generator");
  const fp::FlightPlan &plan = *parsed;

  // The context block has what the briefings need, and nothing for {} fields.
  {
    std::string error;
    const auto plan = simbrief::parse_ofp(ofp, error);
    const std::string ctx = plan ? llm_bridge::flight_context(*plan) : std::string();
    CHECK(contains(ctx, "Callsign: SWA1234"));
    CHECK(contains(ctx, "SID: SSTIK4\n"));
    CHECK(contains(ctx, "STAR: SADDE8 transition GMN"));
    CHECK(contains(ctx, "Initial altitude: FL330"));
    CHECK(contains(ctx, "Planned runway: 28L"));
    CHECK(contains(ctx, "Transition level: FL180"));
    CHECK(contains(ctx, "Block fuel: 7700 kg (7.7 tonnes)"));
    CHECK(contains(ctx, "Takeoff weight: 63100 kg (63.1 tonnes), max 79000 kg (79.0 tonnes)"));
    CHECK(contains(ctx, "Top of descent: 144 nm from destination"));
    CHECK(contains(ctx, "Departure fixes: TRUKN (6000 ft), SSTIK (16000 ft), TOC (FL330)"));
    CHECK(contains(ctx, "Average wind component: 18 kt headwind"));
    CHECK(contains(ctx, "Estimated time enroute: 1 h 13 min"));
    // Worked out for the model: diversion fuel and the holding it leaves.
    CHECK(contains(ctx, "Minimum diversion fuel (alternate plus final reserve): 2500 kg (2.5 tonnes)"));
    CHECK(contains(ctx, "Fuel above minimum diversion fuel at landing: 700 kg, about 17 minutes"));
    CHECK(contains(ctx, "Strongest wind en route: 290 degrees at 34 kt near TOC at FL330"));
    CHECK(!contains(ctx, "TAF: \n"));
    CHECK(!contains(ctx, "Step climbs")); // "KSFO/0330" is no step
    // The destination's own navlog row is not an arrival fix.
    CHECK(contains(ctx, "Arrival fixes: GMN (FL240), SADDE (12000 ft), SMO (7000 ft)\n"));
    CHECK(contains(llm_bridge::user_prompt(llm_bridge::BriefingKind::DESCENT, ctx), "descent and arrival briefing"));
  }

  // Speech cleanup.
  {
    using llm_bridge::clean_for_speech;
    CHECK(clean_for_speech("<think>hmm</think>\n## Departure\n\n**Runway** two eight left.\n- "
                           "Wind calm.\n\n1. Climb to flight level three three zero.") ==
          "Departure\n\nRunway two eight left. Wind calm.\n\nClimb to flight level three three "
          "zero.");
    CHECK(clean_for_speech("  \n").empty());
  }

  // Ollama request, and its answer read as it streams.
  {
    llm_bridge::OllamaConfig cfg;
    const auto body =
        nlohmann::json::parse(llm_bridge::ollama_chat_request(cfg, "sys", "usr", true));
    CHECK(body["model"] == "qwen3.6:35b-a3b");
    CHECK(body["stream"] == true && body["think"] == false);
    CHECK(body["messages"].size() == 2 && body["messages"][0]["role"] == "system");
    CHECK(body["messages"][1]["content"] == "usr");

    // Lines split anywhere across chunks; the last one without a newline.
    const std::string ndjson =
        "{\"message\":{\"content\":\"Runway \"},\"done\":false}\n"
        "{\"message\":{\"content\":\"two eight\"},\"done\":false}\n"
        "{\"message\":{\"content\":\" left.\"},\"done\":true}";
    llm_bridge::OllamaStream stream;
    std::vector<std::string> seen;
    for (std::size_t i = 0; i < ndjson.size(); i += 7) {
      const std::size_t n = std::min<std::size_t>(7, ndjson.size() - i);
      stream.feed(ndjson.data() + i, n);
      if (seen.empty() || seen.back() != stream.text()) seen.push_back(stream.text());
    }
    stream.finish();
    CHECK(stream.text() == "Runway two eight left." && stream.done() && stream.error().empty());
    CHECK(seen.size() >= 3); // it grew as it came in

    // Not streaming: one object.
    llm_bridge::OllamaStream one;
    const std::string single = R"({"message":{"role":"assistant","content":"Wind calm."},"done":true})";
    one.feed(single.data(), single.size());
    one.finish();
    CHECK(one.text() == "Wind calm.");

    llm_bridge::OllamaStream bad;
    const std::string err = "{\"error\":\"model not found\"}\n";
    bad.feed(err.data(), err.size());
    CHECK(bad.error() == "Ollama: model not found");
  }

  // A streamed briefing is the briefing's text while it is generated.
  {
    struct StreamingOllama : util::HttpClient {
      BriefingGenerator *g = nullptr;
      std::vector<std::string> partial; // the departure text between chunks
      util::HttpResponse send(const util::HttpRequest &r) override {
        for (const char *word : {"Cleared ", "for takeoff. ", "Runway ", "two eight left."}) {
          const std::string line =
              nlohmann::json{{"message", {{"content", word}}}, {"done", false}}.dump() + "\n";
          r.on_body(line.data(), line.size());
          partial.push_back(g->briefing(BriefingKind::PREFLIGHT).text);
        }
        util::HttpResponse out;
        out.status = 200;
        return out;
      }
    } llm;
    BriefingGenerator g(llm, {});
    llm.g = &g;
    g.set_plan(plan);
    CHECK(g.generate_once());
    CHECK(llm.partial.size() == 4);
    if (llm.partial.size() == 4) {
      CHECK(llm.partial[0] == "Cleared");
      CHECK(llm.partial[1] == "Cleared for takeoff.");
      CHECK(llm.partial[3] == "Cleared for takeoff. Runway two eight left.");
    }
    const auto b = g.briefing(BriefingKind::PREFLIGHT);
    CHECK(b.status == BriefingStatus::READY && b.text == "Cleared for takeoff. Runway two eight left.");
  }

  // A plan queues the three briefings, generated in order.
  {
    FakeOllama llm;
    BriefingGenerator g(llm, {});
    CHECK(!g.generate_once()); // no plan yet
    g.set_plan(plan);
    CHECK(g.request_id() == "158012345");
    for (const auto &b : g.briefings()) CHECK(b.status == BriefingStatus::QUEUED);

    CHECK(g.generate_once());
    const auto preflight = g.briefing(BriefingKind::PREFLIGHT);
    CHECK(preflight.status == BriefingStatus::READY);
    CHECK(contains(preflight.text, "Briefing. Give the pre-flight briefing"));
    CHECK(!contains(preflight.text, "**")); // cleaned for speech
    CHECK(g.briefing(BriefingKind::DEPARTURE).status == BriefingStatus::QUEUED);
    CHECK(g.generate_once());
    CHECK(g.generate_once());
    CHECK(g.generate_once());
    CHECK(!g.generate_once());
    CHECK(llm.prompts.size() == 4);
    if (llm.prompts.size() == 4) {
      CHECK(contains(llm.prompts[0], "pre-flight briefing"));
      CHECK(contains(llm.prompts[0], "MEL"));
      CHECK(contains(llm.prompts[1], "pre-departure briefing"));
      CHECK(contains(llm.prompts[1], "before V1 we reject"));
      CHECK(contains(llm.prompts[2], "descent and arrival briefing"));
      CHECK(contains(llm.prompts[2], "minimum diversion fuel"));
      CHECK(contains(llm.prompts[3], "approach and landing briefing"));
      CHECK(contains(llm.prompts[3], "Planned runway: 24R"));
    }

    // The same plan again changes nothing; a new one queues everything.
    g.set_plan(plan);
    CHECK(g.briefing(BriefingKind::APPROACH).status == BriefingStatus::READY);
    g.set_plan(with_id(plan, "158099999"));
    CHECK(g.request_id() == "158099999");
    CHECK(g.briefing(BriefingKind::PREFLIGHT).status == BriefingStatus::QUEUED);
  }

  // The LLM is down: FAILED with the reason, and retry_failed queues it again.
  {
    FakeOllama llm;
    llm.up = false;
    BriefingGenerator g(llm, {});
    g.set_plan(plan);
    CHECK(g.generate_once());
    const auto failed = g.briefing(BriefingKind::PREFLIGHT);
    CHECK(failed.status == BriefingStatus::FAILED && contains(failed.error, "Couldn't connect"));
    CHECK(g.retry_failed());
    CHECK(!g.retry_failed());
    CHECK(g.briefing(BriefingKind::PREFLIGHT).status == BriefingStatus::QUEUED);
    llm.up = true;
    CHECK(g.generate_once());
    CHECK(g.briefing(BriefingKind::PREFLIGHT).status == BriefingStatus::READY);
  }

  // Finished briefings survive a restart for the same OFP, not for another.
  {
    namespace fs = std::filesystem;
    const fs::path cache =
        fs::temp_directory_path() /
        ("zb_briefings_test_" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".json");
    llm_bridge::GeneratorConfig cfg;
    cfg.cache_path = cache.string();
    FakeOllama llm;
    {
      BriefingGenerator g(llm, cfg);
      g.set_plan(plan);
      CHECK(g.generate_once()); // pre-flight only
    }
    llm.prompts.clear();
    {
      BriefingGenerator g(llm, cfg);
      g.set_plan(plan);
      CHECK(g.briefing(BriefingKind::PREFLIGHT).status == BriefingStatus::READY);
      CHECK(g.briefing(BriefingKind::DEPARTURE).status == BriefingStatus::QUEUED);
      CHECK(g.generate_once());
      CHECK(llm.prompts.size() == 1 && contains(llm.prompts[0], "pre-departure briefing"));
    }
    {
      BriefingGenerator g(llm, cfg);
      g.set_plan(with_id(plan, "999"));
      CHECK(g.briefing(BriefingKind::PREFLIGHT).status == BriefingStatus::QUEUED);
    }
    // A cache written by older prompts is ignored.
    {
      std::ofstream(cache) << R"({"request_id":"158012345","prompt_version":1,)"
                              R"("briefings":{"preflight":"old"}})";
      BriefingGenerator g(llm, cfg);
      g.set_plan(plan);
      CHECK(g.briefing(BriefingKind::PREFLIGHT).status == BriefingStatus::QUEUED);
    }
    std::error_code ec;
    fs::remove(cache, ec);
  }

  // The thread does the same on its own.
  {
    FakeOllama llm;
    BriefingGenerator g(llm, {});
    g.start();
    g.set_plan(plan);
    for (int i = 0; i < 200 && g.briefing(BriefingKind::APPROACH).status != BriefingStatus::READY;
         ++i) {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    CHECK(g.briefing(BriefingKind::APPROACH).status == BriefingStatus::READY);
    g.stop();
  }

  log::reset_sink();
  return test_result("briefing_generator");
}
