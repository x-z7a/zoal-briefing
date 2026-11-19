#include "zoal_briefing/llm_bridge/briefing_generator.hpp"

#include "zoal_briefing/log.hpp"

#include <nlohmann/json.hpp>

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace zoal_briefing::llm_bridge {

using json = nlohmann::json;

namespace {

constexpr const char *kLog = "briefing";

std::size_t index_of(BriefingKind kind) { return static_cast<std::size_t>(kind); }

Briefings queued_briefings() {
  Briefings out;
  for (std::size_t i = 0; i < kBriefingKinds.size(); ++i) out[i].kind = kBriefingKinds[i];
  return out;
}

std::string route_of(const fp::FlightPlan &plan) {
  return plan.origin.icao + "-" + plan.destination.icao + " (OFP " + plan.request_id + ")";
}

std::string seconds_since(std::chrono::steady_clock::time_point t) {
  char buf[32];
  std::snprintf(buf, sizeof(buf), "%.1f s",
                std::chrono::duration<double>(std::chrono::steady_clock::now() - t).count());
  return buf;
}

} // namespace

const char *to_string(BriefingStatus status) {
  switch (status) {
  case BriefingStatus::QUEUED: return "queued";
  case BriefingStatus::GENERATING: return "generating";
  case BriefingStatus::READY: return "ready";
  case BriefingStatus::FAILED: return "failed";
  }
  return "unknown";
}

BriefingGenerator::BriefingGenerator(util::HttpClient &http, GeneratorConfig config)
    : llm_(http, config.llm), cfg_(std::move(config)), briefings_(queued_briefings()) {}

BriefingGenerator::~BriefingGenerator() { stop(); }

void BriefingGenerator::start() {
  if (running_.exchange(true)) return;
  stopping_ = false;
  log::info(kLog, "LLM: " + cfg_.llm.model + " at " + cfg_.llm.endpoint);
  thread_ = std::thread([this] { run(); });
}

void BriefingGenerator::stop() {
  if (!running_.exchange(false)) return;
  stopping_ = true;
  // Taking the lock orders the flag before the notify (no lost wakeup).
  {
    std::lock_guard<std::mutex> lock(mutex_);
  }
  cv_.notify_all();
  if (thread_.joinable()) thread_.join();
}

void BriefingGenerator::set_plan(const fp::FlightPlan &plan) {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (plan_ && plan_->request_id == plan.request_id) return;
    plan_ = plan;
    briefings_ = queued_briefings();
    ++generation_;
    load_cached_locked();
    changed();
  }
  cv_.notify_all();
}

bool BriefingGenerator::retry_failed() {
  bool any = false;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto &b : briefings_) {
      if (b.status == BriefingStatus::FAILED) {
        b.status = BriefingStatus::QUEUED;
        b.error.clear();
        any = true;
      }
    }
    if (any) changed();
  }
  if (any) cv_.notify_all();
  return any;
}

std::string BriefingGenerator::request_id() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return plan_ ? plan_->request_id : std::string();
}

Briefings BriefingGenerator::briefings() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return briefings_;
}

Briefing BriefingGenerator::briefing(BriefingKind kind) const {
  std::lock_guard<std::mutex> lock(mutex_);
  return briefings_[index_of(kind)];
}

bool BriefingGenerator::has_queued_locked() const {
  if (!plan_) return false;
  for (const auto &b : briefings_) {
    if (b.status == BriefingStatus::QUEUED) return true;
  }
  return false;
}

void BriefingGenerator::run() {
  while (running_.load()) {
    {
      std::unique_lock<std::mutex> lock(mutex_);
      cv_.wait(lock, [this] { return !running_.load() || has_queued_locked(); });
      if (!running_.load()) break;
    }
    generate_once();
  }
}

bool BriefingGenerator::generate_once() {
  fp::FlightPlan plan;
  BriefingKind kind = BriefingKind::DEPARTURE;
  std::uint64_t generation = 0;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!plan_) return false;
    Briefing *next = nullptr;
    for (auto &b : briefings_) {
      if (b.status == BriefingStatus::QUEUED) {
        next = &b;
        break;
      }
    }
    if (!next) return false;
    next->status = BriefingStatus::GENERATING;
    kind = next->kind;
    plan = *plan_;
    generation = generation_.load();
    changed();
  }

  log::info(kLog, std::string("generating the ") + to_string(kind) + " briefing for " +
                      route_of(plan) + " with " + cfg_.llm.model);
  const auto started = std::chrono::steady_clock::now();
  // The answer so far is the briefing's text while it is GENERATING, so the
  // panel shows it and the voice can speak it as it is written.
  const auto on_text = [this, kind, generation](const std::string &so_far) {
    const std::string text = clean_for_speech(so_far);
    std::lock_guard<std::mutex> lock(mutex_);
    if (generation_ != generation) return;
    Briefing &b = briefings_[index_of(kind)];
    if (b.status != BriefingStatus::GENERATING || b.text == text) return;
    b.text = text;
    changed();
  };
  const OllamaClient::Result result = llm_.complete(
      system_prompt(), user_prompt(kind, flight_context(plan)),
      [this, generation] { return stopping_.load() || generation_ != generation; }, on_text);
  const std::string took = seconds_since(started);

  bool ready = false;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (generation_ != generation) {
      log::info(kLog, std::string("dropped the ") + to_string(kind) +
                          " briefing: a newer plan replaced it");
      return true;
    }
    Briefing &b = briefings_[index_of(kind)];
    if (stopping_.load() && !result.ok) {
      b.status = BriefingStatus::QUEUED; // stopped mid-request: not a failure
      b.text.clear();
      return true;
    }
    const std::string text = result.ok ? clean_for_speech(result.text) : std::string();
    if (!text.empty()) {
      b.status = BriefingStatus::READY;
      b.text = text;
      b.error.clear();
      ready = true;
      log::info(kLog, std::string(to_string(kind)) + " briefing ready (" + took + ", " +
                          std::to_string(text.size()) + " chars)");
    } else {
      b.status = BriefingStatus::FAILED;
      b.text.clear(); // a half-written briefing is not one
      b.error = result.ok ? "the model's answer was empty" : result.error;
      log::error(kLog,
                 std::string(to_string(kind)) + " briefing failed after " + took + ": " + b.error);
    }
    changed();
  }
  if (ready) save_cache();
  return true;
}

// --- on disk: {"request_id", "prompt_version", "briefings": {"preflight": ...}} --

void BriefingGenerator::load_cached_locked() {
  if (cfg_.cache_path.empty() || !plan_) return;
  std::ifstream in(cfg_.cache_path, std::ios::binary);
  if (!in.is_open()) return;
  std::stringstream buffer;
  buffer << in.rdbuf();
  const json cache = json::parse(buffer.str(), nullptr, false);
  if (!cache.is_object()) return;
  const auto id = cache.find("request_id");
  if (id == cache.end() || !id->is_string() || id->get<std::string>() != plan_->request_id) return;
  // Briefings from older prompts cover different ground: generate them again.
  const auto version = cache.find("prompt_version");
  if (version == cache.end() || !version->is_number_integer() ||
      version->get<int>() != kPromptVersion) {
    return;
  }
  const auto texts = cache.find("briefings");
  if (texts == cache.end() || !texts->is_object()) return;
  int ready = 0;
  for (auto &b : briefings_) {
    const auto it = texts->find(to_string(b.kind));
    if (it != texts->end() && it->is_string() && !it->get<std::string>().empty()) {
      b.text = it->get<std::string>();
      b.status = BriefingStatus::READY;
      ++ready;
    }
  }
  if (ready > 0) {
    log::info(kLog, std::to_string(ready) + " cached briefing(s) for " + route_of(*plan_));
  }
}

void BriefingGenerator::save_cache() {
  if (cfg_.cache_path.empty()) return;
  json cache = json::object();
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!plan_) return;
    cache["request_id"] = plan_->request_id;
    cache["prompt_version"] = kPromptVersion;
    json texts = json::object();
    for (const auto &b : briefings_) {
      if (b.status == BriefingStatus::READY) texts[to_string(b.kind)] = b.text;
    }
    cache["briefings"] = std::move(texts);
  }

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
    out << cache.dump(2);
  }
  fs::rename(tmp, path, ec);
  if (ec) log::warn(kLog, "could not replace " + path.string() + ": " + ec.message());
}

} // namespace zoal_briefing::llm_bridge
