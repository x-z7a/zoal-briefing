#ifndef ZOAL_BRIEFING_TTS_SPEAKER_HPP
#define ZOAL_BRIEFING_TTS_SPEAKER_HPP

#include "zoal_briefing/audio_pipeline/audio_types.hpp"

#include <mutex>
#include <string>

struct SherpaOnnxOfflineTts;

namespace zoal_briefing::audio {

// Text to speech with a Piper (VITS) voice, run by sherpa-onnx. The voice is a
// directory as sherpa-onnx publishes it: <name>.onnx, tokens.txt and
// espeak-ng-data/.
class TtsSpeaker final : public Synthesizer {
public:
  explicit TtsSpeaker(std::string voice_dir);
  ~TtsSpeaker() override;

  TtsSpeaker(const TtsSpeaker &) = delete;
  TtsSpeaker &operator=(const TtsSpeaker &) = delete;

  bool load(std::string &error);

  std::optional<PcmClip> synthesize(const std::string &text) override;

private:
  std::string voice_dir_;
  const SherpaOnnxOfflineTts *tts_ = nullptr;
  std::mutex mutex_;
};

} // namespace zoal_briefing::audio

#endif // ZOAL_BRIEFING_TTS_SPEAKER_HPP
