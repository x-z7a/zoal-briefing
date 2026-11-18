#include "zoal_briefing/master/state_machine/state_machine.hpp"

namespace zoal_briefing::sm {

StateMachine::StateMachine() = default;

State StateMachine::current_state() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return state_;
}

void StateMachine::transition_to(State next) {
    std::lock_guard<std::mutex> lock(mutex_);
    State prev = state_;
    if (prev != next) {
        state_ = next;
        if (callbacks_enabled_ && callback_) {
            callback_(prev, next);
        }
    }
}

void StateMachine::on_state_change(StateChangeCallback cb) {
    std::lock_guard<std::mutex> lock(mutex_);
    callback_ = std::move(cb);
}

void StateMachine::enable_callbacks(bool v) {
    std::lock_guard<std::mutex> lock(mutex_);
    callbacks_enabled_ = v;
}

} // namespace zoal_briefing::sm
