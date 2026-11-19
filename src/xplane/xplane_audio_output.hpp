#ifndef ZOAL_BRIEFING_XPLANE_AUDIO_OUTPUT_HPP
#define ZOAL_BRIEFING_XPLANE_AUDIO_OUTPUT_HPP

#include "zoal_briefing/audio_pipeline/audio_types.hpp"
#include "zoal_briefing/main_thread_queue.hpp"

#include <XPLMSound.h>

#include <memory>
#include <string>
#include <unordered_map>

namespace zoal_briefing::xplane {

// Plays speech through X-Plane's own sound system (FMOD), on one of its audio
// buses, so it follows the sim's volume settings. XPLMPlayPCMOnBus is
// main-thread only: play() and stop() may be called from any thread and post
// the work to the main-thread queue.
class XPlaneAudioOutput final : public audio::AudioOutput {
public:
  XPlaneAudioOutput(MainThreadQueue &queue, XPLMAudioBus bus);
  ~XPlaneAudioOutput() override;

  void play(audio::PcmClip clip, std::function<void()> on_done) override;
  void stop() override;

  // Main thread: stop everything now (plugin shutdown).
  void shutdown();

  // "ui", "com1", "com2", "pilot", "copilot", "interior"; unknown -> ui.
  static XPLMAudioBus parse_bus(const std::string &name);

private:
  struct Sound {
    XPlaneAudioOutput *owner = nullptr;
    audio::PcmClip clip;
    std::function<void()> on_done;
    FMOD_CHANNEL *channel = nullptr;
  };

  static void on_complete(void *refcon, FMOD_RESULT status);
  void start_on_main(std::unique_ptr<Sound> sound);
  void stop_on_main();
  void finish(Sound *sound);

  MainThreadQueue &queue_;
  XPLMAudioBus bus_;
  // Main thread only. Keyed by the Sound, which is X-Plane's refcon.
  std::unordered_map<Sound *, std::unique_ptr<Sound>> playing_;
  std::shared_ptr<bool> alive_ = std::make_shared<bool>(true);
};

} // namespace zoal_briefing::xplane

#endif
