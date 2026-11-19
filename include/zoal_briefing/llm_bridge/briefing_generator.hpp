#ifndef ZOAL_BRIEFING_LLM_BRIDGE_BRIEFING_GENERATOR_HPP
#define ZOAL_BRIEFING_LLM_BRIDGE_BRIEFING_GENERATOR_HPP

#include "zoal_briefing/data_types/flight_plan.hpp"
#include "zoal_briefing/llm_bridge/briefing_prompts.hpp"
#include "zoal_briefing/llm_bridge/ollama_client.hpp"
#include "zoal_briefing/util/http_client.hpp"

#include <array>
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <thread>

namespace zoal_briefing::llm_bridge {

enum class BriefingStatus { QUEUED, GENERATING, READY, FAILED };
const char *to_string(BriefingStatus status);

struct Briefing {
  BriefingKind kind = BriefingKind::DEPARTURE;
  BriefingStatus status = BriefingStatus::QUEUED;
  // READY: what is spoken. GENERATING: what has been written so far (cleaned
  // for speech), growing as the model streams.
  std::string text;
  std::string error; // FAILED: why
};

using Briefings = std::array<Briefing, kBriefingKinds.size()>;

struct GeneratorConfig {
  OllamaConfig llm;
  // Finished briefings and the OFP they are for, so a restart with the same
  // plan does not regenerate them. Empty: not kept.
  std::string cache_path;
};

// Generates the briefings for one flight plan on a worker thread, one at a
// time in kBriefingKinds order. A new plan (set_plan) queues them all again
// and cancels the request in flight for the old one.
class BriefingGenerator {
public:
  BriefingGenerator(util::HttpClient &http, GeneratorConfig config);
  ~BriefingGenerator();

  BriefingGenerator(const BriefingGenerator &) = delete;
  BriefingGenerator &operator=(const BriefingGenerator &) = delete;

  void start();
  void stop();

  // The same request_id as now is ignored. Briefings cached for this
  // request_id are READY at once; the rest are queued.
  void set_plan(const fp::FlightPlan &plan);
  // Queue the FAILED briefings again. Returns whether there were any.
  bool retry_failed();

  // The current plan's request_id; empty before set_plan.
  std::string request_id() const;
  Briefings briefings() const;
  Briefing briefing(BriefingKind kind) const;
  // Changes whenever briefings() does.
  std::uint64_t version() const { return version_.load(); }

  // Generates the next queued briefing; false when there was none. What the
  // thread runs, public so tests can run it without the thread.
  bool generate_once();

private:
  void run();
  bool has_queued_locked() const;
  void load_cached_locked();
  void save_cache();
  void changed() { ++version_; }

  OllamaClient llm_;
  GeneratorConfig cfg_;

  mutable std::mutex mutex_;
  std::condition_variable cv_;
  std::optional<fp::FlightPlan> plan_;
  Briefings briefings_;

  std::atomic<bool> running_{false};
  std::atomic<bool> stopping_{false};
  // Bumped by every new plan; a briefing for an older one is discarded.
  std::atomic<std::uint64_t> generation_{0};
  std::atomic<std::uint64_t> version_{1};
  std::thread thread_;
};

} // namespace zoal_briefing::llm_bridge

#endif // ZOAL_BRIEFING_LLM_BRIDGE_BRIEFING_GENERATOR_HPP
