#ifndef ZOAL_BRIEFING_STATE_MACHINE_HPP
#define ZOAL_BRIEFING_STATE_MACHINE_HPP

#include <string>
#include <functional>
#include <optional>
#include <mutex>
#include <atomic>
#include <cstdint>

namespace zoal_briefing::sm {

// State transitions for BriefingMaster:
// IDLE → WAKE_DETECTED → LISTENING → STT_COMPLETE → PARSING → DATA_READY 
//      → LLM_ROUTING → BRIEFING_GENERATING → PLAYBACK → IDLE
enum class State {
    IDLE,               // Waiting for wake word
    WAKE_DETECTED,      // Wake word recognized, about to listen
    LISTENING,          // Capturing audio from mic
    STT_COMPLETE,       // Transcription done, awaiting parse
    PARSING,            // Parsing intent/keywords
    DATA_READY,         // Cache data loaded, ready for LLM
    LLM_ROUTING,        // Determining API vs CLI mode
    BRIEFING_GENERATING,// LLM generating briefing
    PLAYBACK,           // Speaking the briefing to pilot
};

// Human-readable state names for logging/UI.
inline const char* state_to_string(State s) {
    switch (s) {
        case State::IDLE:            return "IDLE";
        case State::WAKE_DETECTED:   return "WAKE_DETECTED";
        case State::LISTENING:       return "LISTENING";
        case State::STT_COMPLETE:    return "STT_COMPLETE";
        case State::PARSING:         return "PARSING";
        case State::DATA_READY:      return "DATA_READY";
        case State::LLM_ROUTING:     return "LLM_ROUTING";
        case State::BRIEFING_GENERATING: return "BRIEFING_GENERATING";
        case State::PLAYBACK:        return "PLAYBACK";
    }
    return "UNKNOWN";
}

using StateChangeCallback = std::function<void(State from, State to)>;

class StateMachine {
public:
    StateMachine();

    // Current state (thread-safe).
    State current_state() const;

    // Transition to a new state. Calls callbacks if enabled.
    void transition_to(State next);

    // Register a callback for state transitions.
    void on_state_change(StateChangeCallback cb);

    // Disable/enable callback notifications.
    void enable_callbacks(bool v);

private:
    State state_{State::IDLE};
    std::atomic<bool> callbacks_enabled_{true};
    StateChangeCallback callback_;
    mutable std::mutex mutex_;
};

} // namespace zoal_briefing::sm

#endif // ZOAL_BRIEFING_STATE_MACHINE_HPP
