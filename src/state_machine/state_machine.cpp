#include "zoal_briefing/state_machine/state_machine.hpp"

#include "zoal_briefing/log.hpp"

namespace zoal_briefing::state {

const char *to_string(State state) {
  switch (state) {
  case State::IDLE: return "IDLE";
  case State::WAKE_DETECTED: return "WAKE_DETECTED";
  case State::LISTENING: return "LISTENING";
  case State::STT_COMPLETE: return "STT_COMPLETE";
  case State::PARSING: return "PARSING";
  case State::DATA_READY: return "DATA_READY";
  case State::LLM_ROUTING: return "LLM_ROUTING";
  case State::BRIEFING_GENERATING: return "BRIEFING_GENERATING";
  case State::PLAYBACK: return "PLAYBACK";
  case State::STATE_COUNT: break;
  }
  return "UNKNOWN";
}

bool StateMachine::allowed(State from, State to) {
  if (to == State::IDLE) return true;
  switch (from) {
  case State::IDLE:
    return to == State::WAKE_DETECTED || to == State::LISTENING;
  case State::WAKE_DETECTED:
    return to == State::LISTENING;
  case State::LISTENING:
    return to == State::STT_COMPLETE;
  case State::STT_COMPLETE:
    return to == State::PARSING || to == State::PLAYBACK || to == State::BRIEFING_GENERATING;
  case State::PARSING:
    return to == State::DATA_READY;
  case State::DATA_READY:
    return to == State::LLM_ROUTING;
  case State::LLM_ROUTING:
    return to == State::BRIEFING_GENERATING;
  case State::BRIEFING_GENERATING:
    return to == State::PLAYBACK;
  case State::PLAYBACK:
  case State::STATE_COUNT:
    break;
  }
  return false;
}

bool StateMachine::transition_to(State next, const std::string &reason) {
  State prev;
  Callback cb;
  {
    std::lock_guard<std::mutex> lock(mtx_);
    prev = current_state_;
    if (prev == next) return true;
    if (!allowed(prev, next)) {
      log::warn("state", std::string("refused ") + to_string(prev) + " -> " + to_string(next) +
                             (reason.empty() ? "" : ": " + reason));
      return false;
    }
    current_state_ = next;
    entered_ = std::chrono::steady_clock::now();
    cb = transition_cb_;
  }

  log::info("state", std::string(to_string(prev)) + " -> " + to_string(next) +
                         (reason.empty() ? "" : ": " + reason));
  if (cb) cb(prev, next, reason);
  return true;
}

void StateMachine::reset(const std::string &reason) { transition_to(State::IDLE, reason); }

State StateMachine::current() const {
  std::lock_guard<std::mutex> lock(mtx_);
  return current_state_;
}

std::chrono::steady_clock::duration StateMachine::time_in_state() const {
  std::lock_guard<std::mutex> lock(mtx_);
  return std::chrono::steady_clock::now() - entered_;
}

void StateMachine::on_transition(Callback cb) {
  std::lock_guard<std::mutex> lock(mtx_);
  transition_cb_ = std::move(cb);
}

} // namespace zoal_briefing::state
