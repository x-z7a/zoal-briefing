#ifndef ZOAL_BRIEFING_TTS_SPEAKER_HPP
#define ZOAL_BRIEFING_TTS_SPEAKER_HPP

#include <string>
#include <vector>
#include <optional>
#include <functional>
#include <mutex>

namespace zoal_briefing::audio {

// Speaks briefing text aloud via Piper TTS, mixed into X-Plane com system.
class TtsSpeaker {
public:
    // Initialize with Piper model config.
    explicit TtsSpeaker(const std::string& piper_model_dir);
    ~TtsSpeaker();

    // Generate speech audio from text and queue it for playback in X-Plane.
    void speak(const std::string& text);
    
    // Queue a chunk of text (for long briefings, split into smaller chunks).
    void queue_chunk(const std::string& chunk_text);

    // Stop current playback.
    void stop();
    
    // Check if currently speaking.
    bool is_speaking() const;

private:
    struct SpeechChunk {
        std::vector<std::int16_t> pcm;
        int sample_rate_hz = 0;
    };

    std::string piper_model_dir_;
    std::vector<SpeechChunk> playback_queue_;
    bool speaking_ = false;
    std::mutex mutex_;
};

} // namespace zoal_briefing::audio

#endif // ZOAL_BRIEFING_TTS_SPEAKER_HPP
