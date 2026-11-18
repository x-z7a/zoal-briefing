#include "zoal_briefing/master/audio_pipeline/wake_word_engine.hpp"

namespace zoal_briefing::audio {

WakeWordEngine::WakeWordEngine(const std::string& model_path)
    : model_path_(model_path) {}

WakeWordEngine::~WakeWordEngine() = default;

bool WakeWordEngine::detect(const std::vector<std::int16_t>& pcm16) {
    // TODO: Pass audio buffer to openwakeword inference engine.
    // Return true if "Cookie Monster" wake word is detected.
    (void)pcm16;
    return false;
}

void WakeWordEngine::set_callback(WakeCallback cb) {
    std::lock_guard<std::mutex> lock(mutex_);
    callback_ = std::move(cb);
}

} // namespace zoal_briefing::audio
