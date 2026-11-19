#ifndef ZOAL_BRIEFING_SIMBRIEF_PLAN_WATCHER_HPP
#define ZOAL_BRIEFING_SIMBRIEF_PLAN_WATCHER_HPP

#include "zoal_briefing/data_types/flight_plan.hpp"
#include "zoal_briefing/util/http_client.hpp"

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <thread>

namespace zoal_briefing::simbrief {

struct WatcherConfig {
  // SimBrief username or numeric pilot ID; empty until the pilot sets it.
  std::string pilot;
  // How often to look for a new OFP. refresh() checks at once.
  double poll_seconds = 120.0;
  // The last OFP as received, so a restart starts from it. Empty: not kept.
  std::string cache_path;
};

struct WatchStatus {
  bool pilot_set = false;
  std::string text; // one line for the panel: the plan, or why there is none
  bool fetching = false;
  bool checked = false; // a SimBrief check has finished since start
  std::optional<fp::FlightPlan> plan;
};

// Keeps the pilot's latest SimBrief OFP. A worker thread checks SimBrief
// every poll_seconds and on refresh(); an OFP with a new request_id replaces
// the plan and is handed to on_new_plan (on that thread, outside the lock).
// The cached OFP loaded at start is handed over the same way.
class PlanWatcher {
public:
  using NewPlan = std::function<void(const fp::FlightPlan &)>;

  PlanWatcher(util::HttpClient &http, WatcherConfig config, NewPlan on_new_plan);
  ~PlanWatcher();

  PlanWatcher(const PlanWatcher &) = delete;
  PlanWatcher &operator=(const PlanWatcher &) = delete;

  void start();
  void stop();

  // A changed pilot checks SimBrief at once.
  void set_pilot(const std::string &pilot);
  void refresh(const std::string &why);

  WatchStatus status() const;
  // Changes whenever status() does.
  std::uint64_t version() const { return version_.load(); }

  // The thread's steps, public so tests can run them without it.
  void poll_once();
  void load_cache();

private:
  void run();
  void save_cache(const std::string &raw);
  void changed() { ++version_; }

  util::HttpClient &http_;
  NewPlan on_new_plan_;

  mutable std::mutex mutex_;
  std::condition_variable cv_;
  WatcherConfig cfg_;
  bool refresh_requested_ = true;
  bool fetching_ = false;
  bool checked_ = false;
  std::string text_ = "not checked yet";
  std::optional<fp::FlightPlan> plan_;

  std::atomic<bool> running_{false};
  std::atomic<bool> stopping_{false};
  std::atomic<std::uint64_t> version_{1};
  std::thread thread_;
};

// "SWA1234 KSFO to KLAX (OFP 158012345)", for logs and the panel.
std::string describe(const fp::FlightPlan &plan);

} // namespace zoal_briefing::simbrief

#endif // ZOAL_BRIEFING_SIMBRIEF_PLAN_WATCHER_HPP
