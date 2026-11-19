#include "audio/output/xplane_bus.hpp"

#include "zoal_briefing/log.hpp"

#include <vector>

namespace zoal_briefing::audio {

XPlaneBus::XPlaneBus(MainThreadQueue &queue, XPLMAudioBus bus)
    : queue_(queue), bus_(bus) {}

XPlaneBus::~XPlaneBus() {
  *alive_ = false;
  shutdown();
}

XPLMAudioBus XPlaneBus::parse_bus(const std::string &name) {
  if (name == "com1") return xplm_AudioRadioCom1;
  if (name == "com2") return xplm_AudioRadioCom2;
  if (name == "pilot") return xplm_AudioRadioPilot;
  if (name == "copilot") return xplm_AudioRadioCopilot;
  if (name == "interior") return xplm_AudioInterior;
  return xplm_AudioUI;
}

void XPlaneBus::play(audio::PcmClip clip, std::function<void()> on_done) {
  auto sound = std::make_unique<Sound>();
  sound->owner = this;
  sound->clip = std::move(clip);
  sound->on_done = std::move(on_done);
  auto alive = alive_;
  // std::function needs a copyable callable; hand the Sound over as a raw pointer.
  Sound *raw = sound.release();
  queue_.post([this, alive, raw] {
    std::unique_ptr<Sound> owned(raw);
    if (!*alive) {
      if (owned->on_done) owned->on_done();
      return;
    }
    start_on_main(std::move(owned));
  });
}

void XPlaneBus::stop() {
  auto alive = alive_;
  queue_.post([this, alive] {
    if (*alive) stop_on_main();
  });
}

void XPlaneBus::shutdown() { stop_on_main(); }

void XPlaneBus::start_on_main(std::unique_ptr<Sound> sound) {
  // One voice at a time: a new clip replaces what is playing.
  stop_on_main();

  Sound *key = sound.get();
  playing_.emplace(key, std::move(sound));
  auto &pcm = key->clip.samples;
  key->channel = XPLMPlayPCMOnBus(pcm.data(), static_cast<int>(pcm.size() * sizeof(std::int16_t)),
                                  FMOD_SOUND_FORMAT_PCM16, key->clip.sample_rate, 1, 0, bus_,
                                  &XPlaneBus::on_complete, key);
  if (!key->channel) {
    // On failure X-Plane may already have called on_complete with the error.
    if (playing_.count(key)) {
      log::error("xplane-bus", "XPLMPlayPCMOnBus failed");
      finish(key);
    }
  }
}

void XPlaneBus::stop_on_main() {
  std::vector<Sound *> sounds;
  for (auto &entry : playing_) sounds.push_back(entry.first);
  for (Sound *s : sounds) {
    if (!playing_.count(s)) continue;
    if (s->channel) {
      XPLMStopAudio(s->channel); // calls on_complete, which finishes it
    }
    if (playing_.count(s)) finish(s);
  }
}

void XPlaneBus::on_complete(void *refcon, FMOD_RESULT status) {
  auto *sound = static_cast<Sound *>(refcon);
  XPlaneBus *owner = sound->owner;
  if (!owner->playing_.count(sound)) return;
  if (status != 0) {
    log::warn("xplane-bus", "playback ended with FMOD status " + std::to_string(status));
  }
  owner->finish(sound);
}

void XPlaneBus::finish(Sound *sound) {
  auto it = playing_.find(sound);
  if (it == playing_.end()) return;
  std::unique_ptr<Sound> owned = std::move(it->second);
  playing_.erase(it);
  if (owned->on_done) owned->on_done();
}

} // namespace zoal_briefing::audio
