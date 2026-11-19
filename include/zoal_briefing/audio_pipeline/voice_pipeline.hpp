#ifndef ZOAL_BRIEFING_VOICE_PIPELINE_HPP
#define ZOAL_BRIEFING_VOICE_PIPELINE_HPP

#include "zoal_briefing/audio_pipeline/audio_config.hpp"
#include "zoal_briefing/audio_pipeline/audio_types.hpp"
#include "zoal_briefing/state_machine/state_machine.hpp"

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

namespace zoal_briefing::audio {

class WakeWordEngine;
class SttTranscriber;
class TtsSpeaker;
class MicCapture;
class VoiceLoop;

// The whole audio pipeline, assembled: openWakeWord, whisper.cpp, Piper, the
// microphone and the VoiceLoop. The host provides where speech is played (X-Plane
// or the speakers) and the state machine.
//
// Loading takes a second or two (the whisper model), so start() does it on a
// background thread and the host keeps running; status() says how far it got.
class VoicePipeline {
public:
  VoicePipeline(AudioOutput &output, state::StateMachine &machine);
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

private:
  void load(AudioConfig config, std::string models_dir);
  void set_status(std::string status);

  AudioOutput &output_;
  state::StateMachine &machine_;
  std::unique_ptr<WakeWordEngine> wake_;
  std::unique_ptr<SttTranscriber> stt_;
  std::unique_ptr<TtsSpeaker> tts_;
  std::unique_ptr<VoiceLoop> loop_;
  std::unique_ptr<MicCapture> mic_;
  std::thread loader_;
  std::atomic<bool> ready_{false};
  std::atomic<bool> stopping_{false};
  mutable std::mutex status_mutex_;
  std::string status_ = "stopped";
};

} // namespace zoal_briefing::audio

#endif // ZOAL_BRIEFING_VOICE_PIPELINE_HPP
