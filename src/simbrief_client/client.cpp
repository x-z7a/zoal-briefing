#include "zoal_briefing/simbrief_client/client.hpp"
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <fstream>
#include <filesystem>
#include <regex>
#include <sstream>
#include <chrono>
#include <thread>

using json = nlohmann::json;
namespace fs = std::filesystem;

namespace zoal_briefing::simbrief {

// ── Helper: curl write callback ────────────────────────────────────────────
static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* user) {
    auto* out = static_cast<std::string*>(user);
    out->append(static_cast<char*>(contents), size * nmemb);
    return size * nmemb;
}

// ── Helper: parse a SimBrief .pln file into FlightPlan ───────────────────────
static std::optional<Fp::FlightPlan> parse_pln_file(const fs::path& path) {
    std::ifstream f(path);
    if (!f.is_open()) return std::nullopt;
    
    std::string content((std::istreambuf_iterator<char>(f)),
                         std::istreambuf_iterator<char>());
    
    Fp::FlightPlan plan;
    
    // Parse basic fields from .pln format.
    auto find_section = [&](const std::string& section) -> std::string {
        auto start = content.find("[" + section);
        if (start == std::string::npos) return "";
        auto end = content.find("[", start + 1);
        if (end == std::string::npos) end = content.size();
        return content.substr(start, end - start);
    };
    
    // Extract origin / destination.
    std::string header = find_section("HEADER");
    if (!header.empty()) {
        auto orig_match = std::regex_search(header, std::regex("DEP=(\\w+)"));
        if (orig_match) { /* parse */ }
    }
    
    // Simpler approach: look for common patterns.
    if (auto dep_pos = content.find("DEP="); dep_pos != std::string::npos) {
        plan.origin = content.substr(dep_pos + 4, 4);
    }
    if (auto des_pos = content.find("DES="); des_pos != std::string::npos) {
        plan.destination = content.substr(des_pos + 4, 4);
    }
    if (auto alt_pos = content.find("ALT="); alt_pos != std::string::npos) {
        plan.alternate = content.substr(alt_pos + 4, 4);
    }
    
    // If we found nothing useful, return nullopt.
    if (plan.origin.empty() || plan.destination.empty()) {
        return std::nullopt;
    }
    
    plan.aircraft_type = "B738";  // Default, will be updated from API response.
    
    // Compute a simple hash for change detection.
    uint64_t hash = 0;
    for (char c : content) {
        hash = hash * 31 + static_cast<uint8_t>(c);
    }
    plan.plan_hash = hash;
    
    return plan;
}

// ── Helper: fetch from SimBrief API ─────────────────────────────────────────
static std::optional<Fp::FlightPlan> fetch_via_api(const std::string& base_url,
                                                     const std::string& api_key) {
    // For now, this is a stub. In production, we'd:
    // 1. POST to /api/v1/flights with pilot_id / route
    // 2. GET /api/v1/flights/{id} for status and plan data
    // SimBrief uses OAuth2 or API key auth.
    
    (void)base_url;
    (void)api_key;
    return std::nullopt;  // Stub: no actual HTTP fetch yet.
}

// ── Client Implementation ──────────────────────────────────────────────────
Client::Client(const std::string& simbrief_api_key,
               const std::string& simbrief_base_url)
    : api_key_(simbrief_api_key), base_url_(simbrief_base_url) {
    
    static bool curl_initialized = false;
    if (!curl_initialized) {
        curl_global_init(CURL_GLOBAL_DEFAULT);
        curl_initialized = true;
    }
}

void Client::start_monitoring() {
    monitoring_ = true;
    
    // Start a background thread to poll for SimBrief state changes.
    std::thread([this] {
        while (monitoring_.load()) {
            if (detect_change()) {
                auto plan = fetch_flight_plan();
                if (plan.has_value()) {
                    {
                        std::lock_guard<std::mutex> lock(fp_mutex_);
                        cached_plan_ = plan.value();
                    }
                    
                    // Trigger weather refresh for the new route.
                    trigger_weather_refresh(
                        cached_plan_->origin, 
                        cached_plan_->destination,
                        37.0,    // placeholder lat
                        -122.0); // placeholder lon
                    
                    cache_update_complete_ = true;
                }
            }
            
            std::this_thread::sleep_for(std::chrono::seconds(5));
        }
    }).detach();
}

void Client::stop_monitoring() {
    monitoring_ = false;
}

std::optional<Fp::FlightPlan> Client::fetch_flight_plan() {
    // Strategy 1: Try SimBrief API first.
    if (!api_key_.empty() && !base_url_.empty()) {
        return fetch_via_api(base_url_, api_key_);
    }
    
    // Strategy 2: File watcher on .pln files in X-Plane's Custom Airport Data dir.
    static const auto home = fs::path(std::getenv("HOME"));
    static const std::string simbrief_dir_name = "X-Plane 12";
    static const std::string custom_data_name = "Custom Airport Data";
    
    for (auto& candidate : {
        home / "Documents" / simbrief_dir_name,
        fs::path(std::getenv("XPLANE_HOME")) / simbrief_dir_name,
        fs::path("/Applications/X-Plane") / simbrief_dir_name,
    }) {
        auto candidates = custom_data_name;
        auto candidate_full = candidate / "Custom Airport Data";
        
        if (fs::exists(candidate_full)) {
            for (const auto& entry : fs::directory_iterator(candidate_full)) {
                if (entry.path().extension() == ".pln") {
                    return parse_pln_file(entry.path());
                }
            }
        }
    }
    
    return std::nullopt;
}

bool Client::has_new_plan() const {
    std::lock_guard<std::mutex> lock(fp_mutex_);
    bool new_plan = cache_update_complete_;
    if (new_plan) cache_update_complete_ = false;  // Consume the flag.
    return new_plan;
}

void Client::trigger_weather_refresh(const std::string& icao, 
                                      double lat, double lon) {
    (void)icao; (void)lat; (void)lon;
    // In production: notify WeatherSummarizer to refresh METAR/TAF/SIGMET
    // for this airport and along the route.
}

bool Client::detect_change() {
    auto plan = fetch_flight_plan();
    if (!plan.has_value()) return false;
    
    std::lock_guard<std::mutex> lock(fp_mutex_);
    if (cached_plan_.has_value() && cached_plan_->plan_hash == plan->plan_hash) {
        return false;  // No change detected.
    }
    
    cached_plan_ = std::move(plan);
    return true;
}

std::string Client::current_origin() const {
    std::lock_guard<std::mutex> lock(fp_mutex_);
    if (cached_plan_.has_value()) return cached_plan_->origin;
    return "";
}

std::string Client::current_destination() const {
    std::lock_guard<std::mutex> lock(fp_mutex_);
    if (cached_plan_.has_value()) return cached_plan_->destination;
    return "";
}

} // namespace zoal_briefing::simbrief
