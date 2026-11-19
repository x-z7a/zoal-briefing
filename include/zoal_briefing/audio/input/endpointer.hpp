#ifndef ZOAL_BRIEFING_AUDIO_INPUT_ENDPOINTER_HPP
#define ZOAL_BRIEFING_AUDIO_INPUT_ENDPOINTER_HPP

#include <cstddef>
#include <cstdint>

namespace zoal_briefing::audio {

struct EndpointerConfig {
  int sample_rate = 16000;
  double max_seconds = 8.0;
  double no_speech_timeout_seconds = 4.0;
  int end_silence_ms = 800;
  // Speech is this many times louder (RMS) than the noise floor...
  double speech_ratio = 3.0;
  // ...and at least this loud (RMS of int16 samples), so a silent room's tiny
  // floor does not make breathing count as speech.
  double min_speech_rms = 300.0;
  // Consecutive loud chunks that start speech.
  int start_chunks = 2;
};

// Decides when the pilot has finished the request: an energy detector over
// 80 ms chunks with an adaptive noise floor. Waits for speech, then ends after
// end_silence_ms of quiet, or at max_seconds.
class Endpointer {
public:
  enum class Status { WAITING, SPEAKING, DONE, NO_SPEECH, MAX_LENGTH };

  explicit Endpointer(EndpointerConfig config = {});

  // Starts over. ambient_rms is the room's noise level measured before the
  // request (the voice loop tracks it while idle); 0 means unknown, and a
  // quiet-room default is used. It must not come from the request itself: a
  // pilot who starts talking at once would become the "noise".
  void reset(double ambient_rms = 0.0);
  Status feed(const std::int16_t *pcm, std::size_t count);

  // RMS level of int16 samples.
  static double rms(const std::int16_t *pcm, std::size_t count);
  // Noise-floor tracking: follows quiet quickly, loud slowly.
  static double track_floor(double floor, double level);

  bool heard_speech() const { return speech_started_; }
  double elapsed_seconds() const;
  double noise_floor() const { return noise_floor_; }

private:
  EndpointerConfig cfg_;
  std::size_t samples_ = 0;
  std::size_t silence_samples_ = 0;
  int loud_run_ = 0;
  bool speech_started_ = false;
  double noise_floor_ = 0.0;
  Status status_ = Status::WAITING;
};

const char *to_string(Endpointer::Status status);

} // namespace zoal_briefing::audio

#endif // ZOAL_BRIEFING_AUDIO_INPUT_ENDPOINTER_HPP
