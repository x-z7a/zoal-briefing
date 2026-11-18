#include "zoal_briefing/state_machine/state_machine.hpp"
#include <cassert>
#include <iostream>

using zoal_briefing::state::StateMachine;
using zoal_briefing::state::State;

int main() {
    StateMachine sm;
    
    // Test default state.
    assert(sm.current_state() == State::IDLE);
    std::cout << "PASS: Default state is IDLE\n";

    // Test transition.
    sm.transition_to(State::LISTENING);
    assert(sm.current_state() == State::LISTENING);
    std::cout << "PASS: Can transition to LISTENING\n";

    // Test callbacks.
    bool callback_called = false;
    State from_state, to_state;
    sm.on_state_change([&](State f, State t) {
        from_state = f;
        to_state = t;
        callback_called = true;
    });

    sm.transition_to(State::STT_COMPLETE);
    assert(callback_called);
    assert(from_state == State::LISTENING);
    assert(to_state == State::STT_COMPLETE);
    std::cout << "PASS: Callback fires on transition\n";

    // Test disabled callbacks.
    sm.enable_callbacks(false);
    callback_called = false;
    sm.transition_to(State::IDLE);
    assert(!callback_called);
    std::cout << "PASS: Disabled callbacks prevent notification\n";

    std::cout << "All state machine tests passed.\n";
    return 0;
}
