#ifndef ZOAL_BRIEFING_WAKE_WORD_ENGINE_HPP
#define ZOAL_BRIEFING_WAKE_WORD_ENGINE_HPP

#include <string>
#include <vector>
#include <functional>
#include <mutex>

namespace zoal_briefing::audio {

// Detects the "Cookie Monster" wake word from raw audio using openwakeword.
class WakeWordEngine {
public:
    // Initialize with openwakeword model files.
    explicit WakeWordEngine(const std::string& model_path);
    ~WakeWordEngine();

    // Process a buffer of PCM audio (16-bit, 16kHz). Returns true when wake word is detected.
    bool detect(const std::vector<std::int16_t>& pcm16);
    
    // Set the callback fired on wake word detection.
    using WakeCallback = std::function<void()>;
    void set_callback(WakeCallback cb);

private:
    std::string model_path_;
    WakeCallback callback_;
    std::mutex mutex_;
};

} // namespace zoal_briefing::audio

#endif // ZOAL_BRIEFING_WAKE_WORD_ENGINE_HPP
