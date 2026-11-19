#ifndef ZOAL_BRIEFING_AUDIO_OUTPUT_SPEAKER_HPP
#define ZOAL_BRIEFING_AUDIO_OUTPUT_SPEAKER_HPP

#include "zoal_briefing/audio/types.hpp"

#include <functional>
#include <memory>

namespace zoal_briefing::audio {

// Speech on the computer's speakers, through miniaudio: the dev tool's output
// (the plugin plays through X-Plane instead, see src/audio/output/xplane_bus.hpp).
// Plays one clip at a time; on_done fires on miniaudio's thread.
class Speaker final : public audio::AudioOutput {
public:
  Speaker();
  ~Speaker() override;

  void play(audio::PcmClip clip, std::function<void()> on_done) override;
  void stop() override;

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

} // namespace zoal_briefing::audio

#endif // ZOAL_BRIEFING_AUDIO_OUTPUT_SPEAKER_HPP
