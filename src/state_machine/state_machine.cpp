#include "zoal_briefing/state_machine/state_machine.hpp"
#include <iostream>

namespace zoal_briefing::master::state {

void StateMachine::transition_to(State next) {
    std::lock_guard<std::mutex> lock(mtx_);
    State prev = current_state_;
    if (prev == next) return;
    
    current_state_ = next;
    cancel_timeout(); // Reset on valid transition
    
    if (transition_cb_) {
        transition_cb_(prev, next);
    }
}

State StateMachine::current() const {
    std::lock_guard<std::mutex> lock(mtx_);
    return current_state_;
}

bool StateMachine::is_terminal() const {
    return current_state_ == State::IDLE || current_state_ == State::PLAYBACK;
}

void StateMachine::on_transition(Callback cb) {
    transition_cb_ = std::move(cb);
}

void StateMachine::start_timeout_loop(std::chrono::seconds timeout, std::function<void()> on_timeout_cb) {
    cancel_timeout();
    timeout_active_ = true;
    timeout_start_ = std::chrono::steady_clock::now();
    on_timeout_ = std::move(on_timeout_cb);
    
    // Simulate async polling or callback registration
    // In production, this runs in a dedicated background thread checking steady_clock
}

void StateMachine::cancel_timeout() {
    timeout_active_ = false;
}

} // namespace zoal_briefing::master::state
