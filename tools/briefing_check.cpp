// briefing_check: the SimBrief -> LLM path outside X-Plane.
//
//   briefing_check <simbrief user or id> [--save ofp.json] [--no-llm]
//   briefing_check --ofp ofp.json [--no-llm]
//
// Fetches the pilot's latest OFP (or reads one from a file), prints the flight
// context the LLM sees, then generates the three briefings with the hardcoded
// Ollama server and prints them as they would be spoken.

#include "zoal_briefing/audio/tts/sentences.hpp"
#include "zoal_briefing/llm_bridge/ollama_client.hpp"
#include "zoal_briefing/llm_bridge/briefing_prompts.hpp"
#include "zoal_briefing/util/http_client.hpp"
#include "zoal_briefing/simbrief/client.hpp"
#include "zoal_briefing/simbrief/ofp.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

using namespace zoal_briefing;

namespace {

int run(int argc, char **argv) {
  std::string pilot, save_path, ofp_path;
  bool llm = true;
  for (int i = 1; i < argc; ++i) {
    const std::string a = argv[i];
    if (a == "--save" && i + 1 < argc) {
      save_path = argv[++i];
    } else if (a == "--ofp" && i + 1 < argc) {
      ofp_path = argv[++i];
    } else if (a == "--no-llm") {
      llm = false;
    } else if (!a.empty() && a[0] != '-') {
      pilot = a;
    } else {
      std::fprintf(stderr, "unknown argument %s\n", a.c_str());
      return 2;
    }
  }
  if (pilot.empty() && ofp_path.empty()) {
    std::fprintf(stderr, "usage: briefing_check <simbrief user or id> [--save f] [--no-llm]\n"
                         "       briefing_check --ofp ofp.json [--no-llm]\n");
    return 2;
  }

  auto http = util::make_http_client();
  fp::FlightPlan plan;
  if (!ofp_path.empty()) {
    std::ifstream in(ofp_path);
    std::stringstream ss;
    ss << in.rdbuf();
    std::string error;
    auto parsed = simbrief::parse_ofp(ss.str(), error);
    if (!parsed) {
      std::fprintf(stderr, "%s: %s\n", ofp_path.c_str(), error.c_str());
      return 1;
    }
    plan = *parsed;
  } else {
    std::printf("fetching %s\n", simbrief::latest_ofp_url(pilot).c_str());
    auto result = simbrief::fetch_latest(*http, pilot);
    if (!result.plan) {
      std::fprintf(stderr, "%s\n", result.error.c_str());
      return 1;
    }
    plan = *result.plan;
    if (!save_path.empty()) {
      std::ofstream(save_path, std::ios::binary) << result.raw;
      std::printf("saved the OFP to %s\n", save_path.c_str());
    }
  }

  const std::string context = llm_bridge::flight_context(plan);
  std::printf("OFP %s: %s %s to %s\n\n--- flight context ---\n%s\n", plan.request_id.c_str(),
              plan.callsign.c_str(), plan.origin.icao.c_str(), plan.destination.icao.c_str(),
              context.c_str());
  if (!llm) return 0;

  llm_bridge::OllamaClient chat(*http, llm_bridge::OllamaConfig{});
  int failures = 0;
  for (const llm_bridge::BriefingKind kind : llm_bridge::kBriefingKinds) {
    std::printf("--- %s briefing (%s) ---\n", llm_bridge::briefing_title(kind), chat.config().model.c_str());
    std::fflush(stdout);
    const auto started = std::chrono::steady_clock::now();
    double first_sentence = -1;
    const auto on_text = [&](const std::string &so_far) {
      if (first_sentence < 0 &&
          audio::next_sentence_end(llm_bridge::clean_for_speech(so_far), 0) != std::string::npos) {
        first_sentence =
            std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
      }
    };
    const auto result = chat.complete(llm_bridge::system_prompt(),
                                      llm_bridge::user_prompt(kind, context), {}, on_text);
    const double took =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
    if (!result.ok) {
      std::printf("FAILED after %.1f s: %s\n\n", took, result.error.c_str());
      ++failures;
      continue;
    }
    const std::string spoken = llm_bridge::clean_for_speech(result.text);
    std::printf("%s\n(first sentence after %.1f s, all in %.1f s, %zu words)\n\n",
                spoken.c_str(), first_sentence, took,
                static_cast<std::size_t>(std::count(spoken.begin(), spoken.end(), ' ') + 1));
  }
  return failures == 0 ? 0 : 1;
}

} // namespace

int main(int argc, char **argv) {
  try {
    return run(argc, argv);
  } catch (const std::exception &e) {
    std::fprintf(stderr, "briefing_check: %s\n", e.what());
    return 1;
  }
}
