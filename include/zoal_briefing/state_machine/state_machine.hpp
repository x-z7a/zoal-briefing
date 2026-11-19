#pragma once

#include <chrono>
#include <functional>
#include <mutex>
#include <string>

namespace zoal_briefing::state {

enum class State {
  IDLE,
  WAKE_DETECTED,
  LISTENING,
  STT_COMPLETE,
  PARSING,
  DATA_READY,
  LLM_ROUTING,
  BRIEFING_GENERATING,
  PLAYBACK,
  STATE_COUNT // Sentinel for iteration/bounds
};

const char *to_string(State state);

// The briefing workflow's state. Every change is checked against the
// transition table and logged as "[state] FROM -> TO: reason"; a change the
// table does not allow is refused and logged.
//
// Allowed:
//   IDLE -> WAKE_DETECTED            wake word heard
//   IDLE -> LISTENING                manual trigger (no wake word)
//   WAKE_DETECTED -> LISTENING       acknowledgement played
//   LISTENING -> STT_COMPLETE        request transcribed
//   STT_COMPLETE -> PARSING          briefing flow (later components)
//   STT_COMPLETE -> PLAYBACK         echo mode: speak the transcript back
//   PARSING -> DATA_READY -> LLM_ROUTING -> BRIEFING_GENERATING -> PLAYBACK
//   PLAYBACK -> IDLE                 done speaking
//   any -> IDLE                      reset, timeout or error
class StateMachine {
public:
  using Callback = std::function<void(State from, State to, const std::string &reason)>;

  // Returns false (and logs) when the transition is not allowed. A transition
  // to the current state is a no-op that returns true.
  bool transition_to(State next, const std::string &reason = {});

  // Back to IDLE from anywhere.
  void reset(const std::string &reason);

  State current() const;
  std::chrono::steady_clock::duration time_in_state() const;

  // Called after every accepted transition, outside the lock, so it may
  // transition again.
  void on_transition(Callback cb);

  static bool allowed(State from, State to);

private:
  mutable std::mutex mtx_;
  State current_state_ = State::IDLE;
  std::chrono::steady_clock::time_point entered_ = std::chrono::steady_clock::now();
  Callback transition_cb_;
};

} // namespace zoal_briefing::state
