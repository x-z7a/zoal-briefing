#include "zoal_briefing/simbrief/plan_watcher.hpp"

#include "zoal_briefing/log.hpp"
#include "zoal_briefing/simbrief/client.hpp"
#include "zoal_briefing/simbrief/ofp.hpp"

#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace zoal_briefing::simbrief {

namespace {

constexpr const char *kLog = "simbrief";

std::string utc_now() {
  const std::time_t t = std::time(nullptr);
  std::tm tm{};
#ifdef _WIN32
  gmtime_s(&tm, &t);
#else
  gmtime_r(&t, &tm);
#endif
  char buf[16];
  std::strftime(buf, sizeof(buf), "%H%MZ", &tm);
  return buf;
}

} // namespace

std::string describe(const fp::FlightPlan &plan) {
  std::string s = plan.origin.icao + " to " + plan.destination.icao;
  if (!plan.callsign.empty()) s = plan.callsign + " " + s;
  if (!plan.request_id.empty()) s += " (OFP " + plan.request_id + ")";
  return s;
}

PlanWatcher::PlanWatcher(util::HttpClient &http, WatcherConfig config, NewPlan on_new_plan)
    : http_(http), on_new_plan_(std::move(on_new_plan)), cfg_(std::move(config)) {}

PlanWatcher::~PlanWatcher() { stop(); }

void PlanWatcher::start() {
  if (running_.exchange(true)) return;
  stopping_ = false;
  thread_ = std::thread([this] { run(); });
}

void PlanWatcher::stop() {
  if (!running_.exchange(false)) return;
  stopping_ = true;
  // Taking the lock orders the flag before the notify: the thread cannot be
  // between its predicate check and its wait.
  {
    std::lock_guard<std::mutex> lock(mutex_);
  }
  cv_.notify_all();
  if (thread_.joinable()) thread_.join();
}

void PlanWatcher::set_pilot(const std::string &pilot) {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (pilot == cfg_.pilot) return;
    cfg_.pilot = pilot;
    refresh_requested_ = true;
  }
  log::info(kLog, pilot.empty() ? "pilot cleared" : "pilot set to " + pilot);
  cv_.notify_all();
}

void PlanWatcher::refresh(const std::string &why) {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    refresh_requested_ = true;
  }
  log::info(kLog, "checking SimBrief: " + why);
  cv_.notify_all();
}

WatchStatus PlanWatcher::status() const {
  std::lock_guard<std::mutex> lock(mutex_);
  WatchStatus s;
  s.pilot_set = !cfg_.pilot.empty();
  s.text = text_;
  s.fetching = fetching_;
  s.checked = checked_;
  s.plan = plan_;
  return s;
}

void PlanWatcher::run() {
  load_cache();
  while (running_.load()) {
    {
      std::unique_lock<std::mutex> lock(mutex_);
      cv_.wait_for(lock, std::chrono::duration<double>(cfg_.poll_seconds),
                   [this] { return !running_.load() || refresh_requested_; });
      if (!running_.load()) break;
      refresh_requested_ = false;
    }
    poll_once();
  }
}

void PlanWatcher::poll_once() {
  std::string pilot;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    pilot = cfg_.pilot;
    if (pilot.empty()) {
      checked_ = true;
      text_ = plan_ ? describe(*plan_) + "; no SimBrief pilot set"
                    : "Enter your SimBrief username or pilot ID in Settings";
      changed();
      return;
    }
    fetching_ = true;
    changed();
  }

  FetchResult result = fetch_latest(http_, pilot, [this] { return stopping_.load(); });

  std::optional<fp::FlightPlan> new_plan;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    fetching_ = false;
    checked_ = true;
    changed();
    if (pilot != cfg_.pilot) return; // changed mid-fetch; its own check is queued

    if (!result.plan) {
      text_ = (plan_ ? describe(*plan_) + "; " : std::string()) + "SimBrief check failed at " +
              utc_now() + ": " + result.error;
      log::warn(kLog, "fetch for " + pilot + " failed: " + result.error);
      return;
    }
    if (plan_ && plan_->request_id == result.plan->request_id) {
      text_ = describe(*plan_) + ", checked " + utc_now();
      return;
    }
    plan_ = std::move(result.plan);
    text_ = describe(*plan_) + ", fetched " + utc_now();
    new_plan = plan_;
    log::info(kLog, "new OFP: " + describe(*plan_));
  }
  save_cache(result.raw);
  if (on_new_plan_) on_new_plan_(*new_plan);
}

void PlanWatcher::load_cache() {
  if (cfg_.cache_path.empty()) return;
  std::ifstream in(cfg_.cache_path, std::ios::binary);
  if (!in.is_open()) return;
  std::stringstream buffer;
  buffer << in.rdbuf();
  std::string error;
  auto plan = parse_ofp(buffer.str(), error);
  if (!plan) {
    log::warn(kLog, "ignoring cached OFP " + cfg_.cache_path + ": " + error);
    return;
  }
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (plan_) return; // a fetch got there first
    plan_ = plan;
    text_ = describe(*plan_) + " (cached)";
    changed();
  }
  log::info(kLog, "cached OFP " + describe(*plan));
  if (on_new_plan_) on_new_plan_(*plan);
}

void PlanWatcher::save_cache(const std::string &raw) {
  if (cfg_.cache_path.empty() || raw.empty()) return;
  namespace fs = std::filesystem;
  const fs::path path(cfg_.cache_path);
  std::error_code ec;
  if (path.has_parent_path()) fs::create_directories(path.parent_path(), ec);
  const fs::path tmp = path.string() + ".tmp";
  {
    std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
    if (!out.is_open()) {
      log::warn(kLog, "could not write " + tmp.string());
      return;
    }
    out << raw;
  }
  fs::rename(tmp, path, ec);
  if (ec) log::warn(kLog, "could not replace " + path.string() + ": " + ec.message());
}

} // namespace zoal_briefing::simbrief
