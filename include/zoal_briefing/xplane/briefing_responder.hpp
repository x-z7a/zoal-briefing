#ifndef ZOAL_BRIEFING_XPLANE_BRIEFING_RESPONDER_HPP
#define ZOAL_BRIEFING_XPLANE_BRIEFING_RESPONDER_HPP

#include "zoal_briefing/audio/types.hpp"
#include "zoal_briefing/llm_bridge/briefing_generator.hpp"
#include "zoal_briefing/simbrief/plan_watcher.hpp"

#include <string>

namespace zoal_briefing::xplane {

// Which briefing a request asks for, by keyword, until the intent parser
// exists: "approach"/"landing", "descent"/"arrival", "departure"/"takeoff"/
// "taxi"/"pushback", else the pre-flight briefing.
llm_bridge::BriefingKind pick_briefing(const std::string &request);

// The voice loop's answer, wired from the SimBrief plan and its briefings:
// the briefing the request asks for (pick_briefing), STREAMING while the
// model is writing it (so it is spoken as it is written), PENDING ("stand
// by") before its first words, and a spoken reason when there is no plan or
// it failed (which retries it).
class BriefingResponder : public audio::Responder {
public:
  BriefingResponder(const simbrief::PlanWatcher &plans, llm_bridge::BriefingGenerator &briefings)
      : plans_(plans), briefings_(briefings) {}

  audio::Reply reply(const std::string &request) override;

private:
  const simbrief::PlanWatcher &plans_;
  llm_bridge::BriefingGenerator &briefings_;
};

} // namespace zoal_briefing::xplane

#endif // ZOAL_BRIEFING_XPLANE_BRIEFING_RESPONDER_HPP
