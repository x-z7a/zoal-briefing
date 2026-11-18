#include "zoal_briefing/audio_pipeline/stt_transcriber.hpp"

namespace zoal_briefing::audio {

SttTranscriber::SttTranscriber(const std::string& model_path)
    : model_path_(model_path) {}

SttTranscriber::~SttTranscriber() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (whisper_context_) {
        // TODO: whisper_free(whisper_context_);
        whisper_context_ = nullptr;
    }
}

bool SttTranscriber::load(std::string& error) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (whisper_context_) return true;
    if (load_failed_) {
        error = "model failed to load";
        return false;
    }
    // TODO: whisper_init_from_file_with_params.
    error = "no whisper model loaded";
    load_failed_ = true;
    return false;
}

std::optional<std::string> SttTranscriber::transcribe(
    const std::vector<std::int16_t>& pcm16) {
    
    std::lock_guard<std::mutex> lock(mutex_);
    if (!whisper_context_) return std::nullopt;
    
    // TODO: whisper_full with the audio buffer.
    (void)pcm16;
    return std::nullopt;
}

} // namespace zoal_briefing::audio
