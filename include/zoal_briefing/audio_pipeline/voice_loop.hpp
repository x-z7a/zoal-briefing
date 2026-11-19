#ifndef ZOAL_BRIEFING_VOICE_LOOP_HPP
#define ZOAL_BRIEFING_VOICE_LOOP_HPP

#include "zoal_briefing/audio_pipeline/audio_types.hpp"
#include "zoal_briefing/audio_pipeline/endpointer.hpp"
#include "zoal_briefing/audio_pipeline/ring_buffer.hpp"
#include "zoal_briefing/state_machine/state_machine.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace zoal_briefing::audio {

struct VoiceLoopConfig {
  std::string acknowledgement = "Yes, captain.";
  std::string say_again = "Say again.";
  EndpointerConfig endpointer;
  // Extra time past a clip's length before playback counts as stuck.
  double playback_grace_seconds = 3.0;
};

// The audio pipeline's conductor, in echo mode for now:
//
//   IDLE           wake detector hears the wake word
//   WAKE_DETECTED  speaks the acknowledgement ("Yes, captain.")
//   LISTENING      records the request until the pilot stops talking
//   STT_COMPLETE   whisper transcribes it
//   PLAYBACK       speaks the transcript back
//   IDLE           listens for the wake word again
//
// Microphone audio arrives through push_audio() (the capture callback); one
// worker thread does everything else, so the engines are only ever used from
// that thread. While we are speaking, the microphone is ignored, so the
// plugin never hears itself.
class VoiceLoop {
public:
  VoiceLoop(VoiceLoopConfig config, WakeDetector &wake, Transcriber &stt, Synthesizer &tts,
            AudioOutput &output, state::StateMachine &machine);
  ~VoiceLoop();

  VoiceLoop(const VoiceLoop &) = delete;
  VoiceLoop &operator=(const VoiceLoop &) = delete;

  void start();
  void stop();
  bool running() const { return running_.load(); }

  // 16 kHz mono. Called from the capture thread only (single producer).
  void push_audio(const std::int16_t *pcm, std::size_t count);

  // Skip the wake word and start recording (the listen command).
  void request_listen();
  // Stop speaking or recording and go back to IDLE.
  void request_cancel();

  std::string last_transcript() const;

  // Runs one step of the worker loop: what the thread does, exposed so tests
  // can drive the loop deterministically without the thread.
  void step();

private:
  void run();
  void on_idle_chunk(const std::int16_t *chunk);
  void on_listening_chunk(const std::int16_t *chunk);
  void begin_listening(const std::string &reason);
  void finish_listening(Endpointer::Status status);
  // Synthesizes and plays text. Returns false if nothing could be played.
  bool speak(const std::string &text);
  void back_to_idle(const std::string &reason);

  VoiceLoopConfig cfg_;
  WakeDetector &wake_;
  Transcriber &stt_;
  Synthesizer &tts_;
  AudioOutput &output_;
  state::StateMachine &machine_;

  SampleRing ring_;
  Endpointer endpointer_;
  std::vector<std::int16_t> recording_;
  // The room's noise level, measured while idle, for the endpointer.
  double ambient_rms_ = 0.0;

  std::atomic<bool> running_{false};
  std::thread worker_;
  std::atomic<bool> listen_requested_{false};
  std::atomic<bool> cancel_requested_{false};

  // Playback completion: each clip gets a token; the output's on_done stores
  // it here. Shared so a late callback after destruction is harmless.
  std::shared_ptr<std::atomic<std::uint64_t>> finished_token_ =
      std::make_shared<std::atomic<std::uint64_t>>(0);
  std::uint64_t playing_token_ = 0;
  std::chrono::steady_clock::time_point playback_deadline_{};

  mutable std::mutex transcript_mutex_;
  std::string last_transcript_;
};

} // namespace zoal_briefing::audio

#endif // ZOAL_BRIEFING_VOICE_LOOP_HPP
