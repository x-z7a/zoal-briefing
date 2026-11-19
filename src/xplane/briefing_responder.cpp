#include "zoal_briefing/xplane/briefing_responder.hpp"

#include <algorithm>
#include <cctype>
#include <initializer_list>

namespace zoal_briefing::xplane {

using audio::Reply;
using llm_bridge::BriefingKind;
using llm_bridge::BriefingStatus;

namespace {

bool mentions(const std::string &text, std::initializer_list<const char *> words) {
  return std::any_of(words.begin(), words.end(),
                     [&](const char *w) { return text.find(w) != std::string::npos; });
}

// "Pre-departure" in a sentence, for "the pre-departure briefing".
std::string lower_title(BriefingKind kind) {
  std::string t = llm_bridge::briefing_title(kind);
  if (!t.empty()) t[0] = static_cast<char>(std::tolower(static_cast<unsigned char>(t[0])));
  return t;
}

} // namespace

BriefingKind pick_briefing(const std::string &request) {
  std::string text = request;
  std::transform(text.begin(), text.end(), text.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  if (mentions(text, {"approach", "landing"})) return BriefingKind::APPROACH;
  if (mentions(text, {"descent", "descend", "arrival"})) return BriefingKind::DESCENT;
  if (mentions(text, {"departure", "takeoff", "take off", "take-off", "taxi", "pushback",
                      "push back"})) {
    return BriefingKind::DEPARTURE;
  }
  return BriefingKind::PREFLIGHT;
}

Reply BriefingResponder::reply(const std::string &request) {
  const simbrief::WatchStatus plan = plans_.status();
  // The generator gets the plan from the watcher's callback; until it has
  // this one, its briefings are for nothing (or an older plan).
  const bool generator_has_plan = plan.plan && !plan.plan->request_id.empty() &&
                                  briefings_.request_id() == plan.plan->request_id;

  if (!plan.plan) {
    if (!plan.pilot_set) {
      return {Reply::Status::READY, "There is no flight plan yet, captain. Enter your SimBrief "
                                    "username in the briefing settings."};
    }
    if (plan.fetching || !plan.checked) {
      return {Reply::Status::PENDING, "Stand by, captain. Checking SimBrief for your flight plan."};
    }
    return {Reply::Status::READY, "Sorry, captain, I could not get your flight plan from "
                                  "SimBrief. Check the username in the settings."};
  }

  const BriefingKind kind = pick_briefing(request);
  const std::string name = "the " + lower_title(kind) + " briefing";
  const llm_bridge::Briefing b = briefings_.briefing(kind);
  if (generator_has_plan && b.status == BriefingStatus::READY) {
    return {Reply::Status::READY, b.text};
  }
  if (generator_has_plan && b.status == BriefingStatus::GENERATING && !b.text.empty()) {
    return {Reply::Status::STREAMING, b.text}; // spoken as it is written
  }
  if (generator_has_plan && b.status == BriefingStatus::FAILED) {
    briefings_.retry_failed();
    return {Reply::Status::READY, "Sorry, captain, " + name +
                                      " could not be prepared. I am trying again; ask me in "
                                      "a minute."};
  }
  // Briefings are generated in order, so a later one may wait on the others.
  return {Reply::Status::PENDING, "Stand by, captain. The " + lower_title(kind) +
                                      " briefing is still being prepared."};
}

} // namespace zoal_briefing::xplane
