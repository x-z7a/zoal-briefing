#ifndef ZOAL_BRIEFING_MINIAUDIO_DEVICES_HPP
#define ZOAL_BRIEFING_MINIAUDIO_DEVICES_HPP

#include "zoal_briefing/audio_pipeline/audio_types.hpp"

#include <functional>
#include <memory>
#include <string>

namespace zoal_briefing::audio {

// The microphone, opened with miniaudio at 16 kHz mono 16-bit (miniaudio
// converts from whatever the device runs at). Samples go to `on_audio` on
// miniaudio's audio thread, which must not block.
class MicCapture {
public:
  using OnAudio = std::function<void(const std::int16_t *pcm, std::size_t count)>;

  MicCapture();
  ~MicCapture();

  // device_hint: a substring of the device name; empty for the default.
  bool start(const std::string &device_hint, OnAudio on_audio, std::string &error);
  void stop();
  bool running() const;
  std::string device_name() const;

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

// Speaker output for the dev tool (the plugin plays through X-Plane instead).
// Plays one clip at a time; on_done fires on miniaudio's thread.
class SpeakerOutput final : public AudioOutput {
public:
  SpeakerOutput();
  ~SpeakerOutput() override;

  void play(PcmClip clip, std::function<void()> on_done) override;
  void stop() override;

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

} // namespace zoal_briefing::audio

#endif // ZOAL_BRIEFING_MINIAUDIO_DEVICES_HPP
