#ifndef ZOAL_BRIEFING_AUDIO_TYPES_HPP
#define ZOAL_BRIEFING_AUDIO_TYPES_HPP

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace zoal_briefing::audio {

// Every model in the pipeline listens at 16 kHz mono (openWakeWord, whisper).
inline constexpr int kCaptureSampleRate = 16000;
// openWakeWord's frame: 80 ms at 16 kHz.
inline constexpr std::size_t kChunkSamples = 1280;

// Mono 16-bit PCM at a given rate.
struct PcmClip {
  std::vector<std::int16_t> samples;
  int sample_rate = 0;

  double seconds() const {
    return sample_rate > 0 ? static_cast<double>(samples.size()) / sample_rate : 0.0;
  }
};

// Linear-interpolation resampler, good enough for speech (TTS output into the
// 16 kHz models in the self-test).
std::vector<std::int16_t> resample_linear(const std::vector<std::int16_t> &in, int from_hz,
                                          int to_hz);

// --- the pipeline's seams ------------------------------------------------------
// VoiceLoop talks only to these, so it is tested with fakes and the heavy
// engines (ONNX Runtime, whisper.cpp, sherpa-onnx) stay out of the core.

// Streaming wake-word detector. feed() takes 16 kHz PCM of any length and
// returns the score when the wake word was detected in it.
class WakeDetector {
public:
  virtual ~WakeDetector() = default;
  virtual std::optional<float> feed(const std::int16_t *pcm, std::size_t count) = 0;
  // Forget buffered audio (after we spoke, so our own voice is not heard).
  virtual void reset() = 0;
  // The phrase the loaded model listens for, for logs ("hey jarvis").
  virtual std::string phrase() const = 0;
};

class Transcriber {
public:
  virtual ~Transcriber() = default;
  // 16 kHz mono. nullopt on failure; an empty string when nothing was said.
  virtual std::optional<std::string> transcribe(const std::vector<std::int16_t> &pcm16k) = 0;
};

class Synthesizer {
public:
  virtual ~Synthesizer() = default;
  virtual std::optional<PcmClip> synthesize(const std::string &text) = 0;
};

// What the loop says back to a transcribed request. Without one, the loop
// echoes the transcript.
struct Reply {
  enum class Status {
    READY,     // the whole answer: speak text
    STREAMING, // the answer so far, still being written: speak its whole
               // sentences and ask again for the rest
    PENDING,   // nothing yet: speak text (a "stand by") once, then ask again
  };
  Status status = Status::READY;
  std::string text;
};

class Responder {
public:
  virtual ~Responder() = default;
  // Called on the voice worker thread, first with a fresh request and then
  // again with the same request while the answer is PENDING or STREAMING.
  // A STREAMING text only grows; a later text that does not continue it is
  // spoken after what was already said. Must not block.
  virtual Reply reply(const std::string &request) = 0;
};

// Plays a clip. on_done may be called from any thread, exactly once per
// clip that started, when it finishes or is stopped.
class AudioOutput {
public:
  virtual ~AudioOutput() = default;
  virtual void play(PcmClip clip, std::function<void()> on_done) = 0;
  virtual void stop() = 0;
};

} // namespace zoal_briefing::audio

#endif // ZOAL_BRIEFING_AUDIO_TYPES_HPP
