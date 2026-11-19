#ifndef ZOAL_BRIEFING_AUDIO_VOICE_PIPELINE_HPP
#define ZOAL_BRIEFING_AUDIO_VOICE_PIPELINE_HPP

#include "zoal_briefing/audio/config.hpp"
#include "zoal_briefing/audio/types.hpp"
#include "zoal_briefing/audio/voice_loop.hpp"
#include "zoal_briefing/state_machine/state_machine.hpp"

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

namespace zoal_briefing::audio {

class OpenWakeWord;
class WhisperStt;
class PiperTts;
class Mic;

// The whole audio pipeline, assembled: openWakeWord, whisper.cpp, Piper, the
// microphone and the VoiceLoop. The host provides where speech is played (X-Plane
// or the speakers), the state machine and, optionally, what to answer a
// request with (without a Responder the loop echoes the transcript).
//
// Loading takes a second or two (the whisper model), so start() does it on a
// background thread and the host keeps running; status() says how far it got.
class VoicePipeline {
public:
  VoicePipeline(AudioOutput &output, state::StateMachine &machine,
                Responder *responder = nullptr);
  ~VoicePipeline();

  VoicePipeline(const VoicePipeline &) = delete;
  VoicePipeline &operator=(const VoicePipeline &) = delete;

  // models_dir holds openwakeword/, whisper/ and piper/.
  void start(const AudioConfig &config, const std::string &models_dir);
  void stop();

  bool ready() const { return ready_.load(); }
  // "loading", "listening for \"hey jarvis\"", or the error that stopped it.
  std::string status() const;

  void request_listen();
  void request_cancel();
  std::string last_transcript() const;
  // What is being said now, for the panel.
  SpeechProgress speech_progress() const;

private:
  void load(AudioConfig config, std::string models_dir);
  void set_status(std::string status);

  AudioOutput &output_;
  state::StateMachine &machine_;
  Responder *responder_;
  std::unique_ptr<OpenWakeWord> wake_;
  std::unique_ptr<WhisperStt> stt_;
  std::unique_ptr<PiperTts> tts_;
  std::unique_ptr<VoiceLoop> loop_;
  std::unique_ptr<Mic> mic_;
  std::thread loader_;
  std::atomic<bool> ready_{false};
  std::atomic<bool> stopping_{false};
  mutable std::mutex status_mutex_;
  std::string status_ = "stopped";
};

} // namespace zoal_briefing::audio

#endif // ZOAL_BRIEFING_AUDIO_VOICE_PIPELINE_HPP
