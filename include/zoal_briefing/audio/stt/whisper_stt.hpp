#ifndef ZOAL_BRIEFING_AUDIO_STT_WHISPER_STT_HPP
#define ZOAL_BRIEFING_AUDIO_STT_WHISPER_STT_HPP

#include "zoal_briefing/audio/types.hpp"

#include <mutex>
#include <string>

struct whisper_context;

namespace zoal_briefing::audio {

// Speech to text with whisper.cpp, on the pilot's machine (Metal on macOS).
class WhisperStt final : public Transcriber {
public:
  explicit WhisperStt(std::string model_path);
  ~WhisperStt() override;

  WhisperStt(const WhisperStt &) = delete;
  WhisperStt &operator=(const WhisperStt &) = delete;

  bool load(std::string &error);

  // 16 kHz mono PCM. Trimmed text; empty when nothing was said.
  std::optional<std::string> transcribe(const std::vector<std::int16_t> &pcm16k) override;

private:
  std::string model_path_;
  whisper_context *ctx_ = nullptr;
  std::mutex mutex_;
};

} // namespace zoal_briefing::audio

#endif // ZOAL_BRIEFING_AUDIO_STT_WHISPER_STT_HPP
