#ifndef ZOAL_BRIEFING_WAKE_WORD_ENGINE_HPP
#define ZOAL_BRIEFING_WAKE_WORD_ENGINE_HPP

#include "zoal_briefing/audio_pipeline/audio_types.hpp"

#include <memory>
#include <string>

namespace zoal_briefing::audio {

struct WakeWordModels {
  std::string melspectrogram; // openWakeWord's shared front end
  std::string embedding;      // openWakeWord's shared speech embedding
  std::string classifier;     // the wake word itself, e.g. cookie_monster.onnx
  std::string phrase;         // what the classifier listens for, for logs
  float threshold = 0.5f;
};

// openWakeWord, run on ONNX Runtime. A C++ port of its streaming pipeline:
//
//   80 ms of 16 kHz audio -> melspectrogram.onnx -> 8 frames x 32 mel bins
//   (scaled x/10 + 2) -> embedding_model.onnx over the last 76 frames -> one
//   96-d feature -> <wake>.onnx over the last 16 features -> score 0..1.
//
// The first 5 scores after load or reset are ignored, as in openWakeWord, so
// a freshly reset buffer cannot fire.
class WakeWordEngine final : public WakeDetector {
public:
  WakeWordEngine();
  ~WakeWordEngine() override;

  bool load(const WakeWordModels &models, std::string &error);
  bool loaded() const;

  std::optional<float> feed(const std::int16_t *pcm, std::size_t count) override;
  void reset() override;
  std::string phrase() const override;

  // The last score computed (for diagnostics and the self-test).
  float last_score() const;
  // Highest score since the last reset.
  float peak_score() const;

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

} // namespace zoal_briefing::audio

#endif // ZOAL_BRIEFING_WAKE_WORD_ENGINE_HPP
