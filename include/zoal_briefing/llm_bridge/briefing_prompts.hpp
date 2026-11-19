#ifndef ZOAL_BRIEFING_LLM_BRIDGE_BRIEFING_PROMPTS_HPP
#define ZOAL_BRIEFING_LLM_BRIDGE_BRIEFING_PROMPTS_HPP

#include "zoal_briefing/data_types/flight_plan.hpp"

#include <array>
#include <string>

namespace zoal_briefing::llm_bridge {

// The briefings generated for every new flight plan, in this order: the
// order they are given in, from the dispatch desk to the gate at the other end.
enum class BriefingKind {
  PREFLIGHT, // dispatch / gate: route, weather, fuel, aircraft status, crew
  DEPARTURE, // cockpit before pushback: taxi, clearance and SID, takeoff emergencies
  DESCENT,   // before top of descent: weather, STAR and profile, fuel, arrival threats
  APPROACH,  // terminal area: approach setup, minima, landing, missed approach, taxi-in
};
inline constexpr std::array<BriefingKind, 4> kBriefingKinds = {
    BriefingKind::PREFLIGHT, BriefingKind::DEPARTURE, BriefingKind::DESCENT,
    BriefingKind::APPROACH};

// Bumped whenever the prompts change what a briefing covers, so briefings
// cached from older prompts are generated again.
inline constexpr int kPromptVersion = 2;

// "preflight", "departure", "descent", "approach": keys in logs and the cache.
const char *to_string(BriefingKind kind);
// "Pre-flight", "Pre-departure", ...: headings on the panel, and how the
// voice names a briefing.
const char *briefing_title(BriefingKind kind);

// The plan as a compact, labelled text block for the LLM. Items the OFP does
// not have are left out, and the prompt tells the model not to invent them.
std::string flight_context(const fp::FlightPlan &plan);

// Shared by every briefing: who is speaking, and how to write for speech.
std::string system_prompt();
std::string user_prompt(BriefingKind kind, const std::string &context);

// The model's answer made safe for text-to-speech: drops <think> blocks and
// markdown (headings, bullets, emphasis) and joins lines into paragraphs.
std::string clean_for_speech(const std::string &text);

} // namespace zoal_briefing::llm_bridge

#endif // ZOAL_BRIEFING_LLM_BRIDGE_BRIEFING_PROMPTS_HPP
