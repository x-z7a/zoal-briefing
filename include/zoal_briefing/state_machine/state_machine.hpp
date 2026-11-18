#pragma once

#include <atomic>
#include <mutex>
#include <string>
#include <chrono>
#include <functional>

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

class StateMachine {
public:
    StateMachine() = default;
    
    void transition_to(State next);
    State current() const;
    bool is_terminal() const;
    
    using Callback = std::function<void(State from, State to)>;
    void on_transition(Callback cb);
    
    // Timeout/Recovery
    void start_timeout_loop(std::chrono::seconds timeout_ms, std::function<void()> on_timeout);
    void cancel_timeout();

private:
    mutable std::mutex mtx_;
    State current_state_ = State::IDLE;
    Callback transition_cb_;
    
    // Timeout logic
    bool timeout_active_ = false;
    std::chrono::steady_clock::time_point timeout_start_;
    std::function<void()> on_timeout_;
};

} // namespace zoal_briefing::state
