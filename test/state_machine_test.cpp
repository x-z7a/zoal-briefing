#include "check.hpp"

#include "zoal_briefing/log.hpp"
#include "zoal_briefing/state_machine/state_machine.hpp"

#include <string>
#include <vector>

using zoal_briefing::state::State;
using zoal_briefing::state::StateMachine;

int main() {
  std::vector<std::string> lines;
  zoal_briefing::log::set_sink([&](const std::string &line) { lines.push_back(line); });

  StateMachine sm;
  CHECK(sm.current() == State::IDLE);

  // The echo-mode path.
  CHECK(sm.transition_to(State::WAKE_DETECTED, "heard wake word"));
  CHECK(sm.transition_to(State::LISTENING, "acknowledged"));
  CHECK(sm.transition_to(State::STT_COMPLETE, "transcribed"));
  CHECK(sm.transition_to(State::PLAYBACK, "echo"));
  CHECK(sm.transition_to(State::IDLE, "done"));

  // Every change is logged with its reason.
  CHECK(lines.size() == 5);
  CHECK(!lines.empty() &&
        lines[0] == "[zoal-briefing] [state] IDLE -> WAKE_DETECTED: heard wake word");

  // Refused transitions leave the state alone and log a warning.
  lines.clear();
  CHECK(!sm.transition_to(State::PLAYBACK, "skip ahead"));
  CHECK(sm.current() == State::IDLE);
  CHECK(lines.size() == 1 && lines[0].find("WARN [state] refused IDLE -> PLAYBACK") != std::string::npos);

  // Same state: a no-op, not logged.
  lines.clear();
  CHECK(sm.transition_to(State::IDLE));
  CHECK(lines.empty());

  // Manual trigger skips the wake word; reset works from anywhere.
  CHECK(sm.transition_to(State::LISTENING, "listen command"));
  sm.reset("cancelled");
  CHECK(sm.current() == State::IDLE);

  // The full briefing path is allowed for the later components.
  CHECK(StateMachine::allowed(State::STT_COMPLETE, State::PARSING));
  CHECK(StateMachine::allowed(State::PARSING, State::DATA_READY));
  CHECK(StateMachine::allowed(State::BRIEFING_GENERATING, State::PLAYBACK));
  CHECK(!StateMachine::allowed(State::WAKE_DETECTED, State::PLAYBACK));

  // Callbacks run outside the lock, so they may transition again.
  sm.on_transition([&](State, State to, const std::string &) {
    if (to == State::WAKE_DETECTED) sm.transition_to(State::LISTENING, "from callback");
  });
  CHECK(sm.transition_to(State::WAKE_DETECTED, "wake"));
  CHECK(sm.current() == State::LISTENING);

  zoal_briefing::log::reset_sink();
  return test_result("state_machine");
}
