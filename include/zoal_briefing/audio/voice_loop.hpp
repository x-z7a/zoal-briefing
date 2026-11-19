#ifndef ZOAL_BRIEFING_AUDIO_VOICE_LOOP_HPP
#define ZOAL_BRIEFING_AUDIO_VOICE_LOOP_HPP

#include "zoal_briefing/audio/types.hpp"
#include "zoal_briefing/audio/input/endpointer.hpp"
#include "zoal_briefing/audio/input/sample_ring.hpp"
#include "zoal_briefing/state_machine/state_machine.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace zoal_briefing::audio {

struct VoiceLoopConfig {
  std::string acknowledgement = "Yes, captain.";
  std::string say_again = "Say again.";
  // Said when a PENDING reply never became ready, or a STREAMING one stalled.
  std::string gave_up = "Sorry, captain, the briefing is not ready. Ask me again in a minute.";
  // How long to wait on a PENDING reply, or for a STREAMING one to grow.
  double max_wait_seconds = 300.0;
  EndpointerConfig endpointer;
  // Extra time past a clip's length before playback counts as stuck.
  double playback_grace_seconds = 3.0;
};

// What is being said, for the panel: the sentences already spoken and the one
// playing now.
struct SpeechProgress {
  std::string spoken;
  std::string current;
};

// The audio pipeline's conductor:
//
//   IDLE                 wake detector hears the wake word
//   WAKE_DETECTED        speaks the acknowledgement ("Yes, captain.")
//   LISTENING            records the request until the pilot stops talking
//   STT_COMPLETE         whisper transcribes it and asks the Responder
//   BRIEFING_GENERATING  nothing to say yet: says "stand by" once and waits
//   PLAYBACK             speaks the reply (with no Responder: the transcript);
//                        a STREAMING reply is spoken as it is written
//   IDLE                 listens for the wake word again
//
// Everything is spoken a sentence at a time: the first sentence plays as soon
// as it is synthesized, and the next one is synthesized while it plays.
//
// Microphone audio arrives through push_audio() (the capture callback); one
// worker thread does everything else, so the engines are only ever used from
// that thread. While we are speaking, the microphone is ignored, so the
// plugin never hears itself.
class VoiceLoop {
public:
  VoiceLoop(VoiceLoopConfig config, WakeDetector &wake, Transcriber &stt, Synthesizer &tts,
            AudioOutput &output, state::StateMachine &machine, Responder *responder = nullptr);
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
  SpeechProgress speech_progress() const;

  // Runs one step of the worker loop: what the thread does, exposed so tests
  // can drive the loop deterministically without the thread.
  void step();

private:
  void run();
  void on_idle_chunk(const std::int16_t *chunk);
  void on_listening_chunk(const std::int16_t *chunk);
  void begin_listening(const std::string &reason);
  void finish_listening(Endpointer::Status status);
  void on_waiting();
  void on_speaking();
  // Enters PLAYBACK and says text; to IDLE when there is nothing to play.
  void speak_reply(const std::string &text, const std::string &reason);
  void begin_streaming(const std::string &text, const std::string &reason);
  void back_to_idle(const std::string &reason);
  void reset_wait();

  // --- speech, a sentence at a time ---
  enum class Speech { PLAYING, WAITING_FOR_TEXT, DONE };
  // Starts saying text (dropping whatever was being said); more may follow
  // through continue_saying() unless complete.
  void say(const std::string &text, bool complete);
  // The reply so far, again: whatever it adds to what we have is said next.
  // Returns whether it added anything.
  bool continue_saying(const std::string &text, bool complete);
  void split_sentences();
  // Plays the next sentence when the last one finished, and synthesizes the
  // one after while it plays.
  Speech pump();
  bool synthesize_next();
  void play_next();
  // A clip we started has not finished yet (and is not past its deadline).
  bool clip_playing() const;

  VoiceLoopConfig cfg_;
  WakeDetector &wake_;
  Transcriber &stt_;
  Synthesizer &tts_;
  AudioOutput &output_;
  state::StateMachine &machine_;
  Responder *responder_;

  // The request whose reply is PENDING or STREAMING, and until when we wait
  // for it (to start, or to grow).
  std::string pending_request_;
  std::chrono::steady_clock::time_point wait_deadline_{};
  std::chrono::steady_clock::time_point next_poll_{};

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
  std::uint64_t stuck_token_ = 0; // the last clip reported as stuck
  std::chrono::steady_clock::time_point playback_deadline_{};

  struct Utterance {
    std::string text;
    bool complete = true;
    // Where the replies being continued start in text (after a reply that
    // did not continue the one before, they are appended).
    std::size_t reply_offset = 0;
    std::size_t split = 0; // text before this is in sentences or spoken
    std::deque<std::string> sentences;
    std::optional<PcmClip> next_clip;
    std::string next_sentence;
  };
  Utterance utterance_;
  mutable std::mutex progress_mutex_;
  SpeechProgress progress_;

  mutable std::mutex transcript_mutex_;
  std::string last_transcript_;
};

} // namespace zoal_briefing::audio

#endif // ZOAL_BRIEFING_AUDIO_VOICE_LOOP_HPP
