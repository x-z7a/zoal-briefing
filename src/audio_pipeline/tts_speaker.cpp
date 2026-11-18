#include "zoal_briefing/audio_pipeline/tts_speaker.hpp"

namespace zoal_briefing::audio {

TtsSpeaker::TtsSpeaker(const std::string& piper_model_dir)
    : piper_model_dir_(piper_model_dir) {}

TtsSpeaker::~TtsSpeaker() = default;

void TtsSpeaker::speak(const std::string& text) {
    // TODO: Piper generates PCM audio from text.
    // Queue the PCM into the playback system for X-Plane com mixing.
    (void)text;
}

void TtsSpeaker::queue_chunk(const std::string& chunk_text) {
    std::lock_guard<std::mutex> lock(mutex_);
    SpeechChunk chunk;
    // TODO: Piper generates PCM for chunk_text into chunk.pcm.
    (void)chunk_text;
    playback_queue_.push_back(std::move(chunk));
}

void TtsSpeaker::stop() {
    std::lock_guard<std::mutex> lock(mutex_);
    speaking_ = false;
}

bool TtsSpeaker::is_speaking() const {
    return speaking_;
}

} // namespace zoal_briefing::audio
