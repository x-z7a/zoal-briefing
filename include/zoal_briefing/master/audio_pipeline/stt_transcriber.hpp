#ifndef ZOAL_BRIEFING_STT_TRANSCRIBER_HPP
#define ZOAL_BRIEFING_STT_TRANSCRIBER_HPP

#include <string>
#include <vector>
#include <optional>
#include <mutex>

namespace zoal_briefing::audio {

// Transcribes audio to text using whisper.cpp (local, offline-capable).
class SttTranscriber {
public:
    explicit SttTranscriber(const std::string& model_path);
    ~SttTranscriber();

    // Load the Whisper model. Returns false on failure.
    bool load(std::string& error);
    
    // Transcribe a buffer of PCM audio (16-bit, 16kHz).
    std::optional<std::string> transcribe(const std::vector<std::int16_t>& pcm16);

private:
    std::string model_path_;
    void* whisper_context_ = nullptr;
    bool load_failed_ = false;
    std::mutex mutex_;
};

} // namespace zoal_briefing::audio

#endif // ZOAL_BRIEFING_STT_TRANSCRIBER_HPP
