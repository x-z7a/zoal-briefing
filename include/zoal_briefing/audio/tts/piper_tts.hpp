#ifndef ZOAL_BRIEFING_AUDIO_TTS_PIPER_TTS_HPP
#define ZOAL_BRIEFING_AUDIO_TTS_PIPER_TTS_HPP

#include "zoal_briefing/audio/types.hpp"

#include <mutex>
#include <string>

struct SherpaOnnxOfflineTts;

namespace zoal_briefing::audio {

// Text to speech with a Piper (VITS) voice, run by sherpa-onnx. The voice is a
// directory as sherpa-onnx publishes it: <name>.onnx, tokens.txt and
// espeak-ng-data/.
class PiperTts final : public Synthesizer {
public:
  explicit PiperTts(std::string voice_dir);
  ~PiperTts() override;

  PiperTts(const PiperTts &) = delete;
  PiperTts &operator=(const PiperTts &) = delete;

  bool load(std::string &error);

  std::optional<PcmClip> synthesize(const std::string &text) override;

private:
  std::string voice_dir_;
  const SherpaOnnxOfflineTts *tts_ = nullptr;
  std::mutex mutex_;
};

} // namespace zoal_briefing::audio

#endif // ZOAL_BRIEFING_AUDIO_TTS_PIPER_TTS_HPP
