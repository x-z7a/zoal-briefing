#include "zoal_briefing/audio/output/speaker.hpp"

#include "audio/miniaudio.hpp"
#include "zoal_briefing/log.hpp"

#include <algorithm>
#include <atomic>
#include <cstring>
#include <mutex>

namespace zoal_briefing::audio {

using audio::PcmClip;

struct Speaker::Impl {
  std::mutex mutex; // guards device lifetime (play/stop), not the callback
  ma_device device{};
  bool device_ready = false;
  PcmClip clip;
  std::atomic<std::size_t> position{0};
  std::function<void()> on_done;
  std::atomic<bool> done_fired{true};

  void fire_done() {
    if (!done_fired.exchange(true) && on_done) on_done();
  }

  static void callback(ma_device *device, void *output, const void *, ma_uint32 frames) {
    auto *self = static_cast<Impl *>(device->pUserData);
    auto *out = static_cast<std::int16_t *>(output);
    const std::size_t pos = self->position.load();
    const std::size_t total = self->clip.samples.size();
    const std::size_t n = std::min<std::size_t>(frames, total > pos ? total - pos : 0);
    if (n > 0) std::memcpy(out, self->clip.samples.data() + pos, n * sizeof(std::int16_t));
    if (n < frames) std::memset(out + n, 0, (frames - n) * sizeof(std::int16_t));
    self->position.store(pos + n);
    if (pos + n >= total) self->fire_done();
  }

  void close() {
    if (device_ready) {
      ma_device_uninit(&device);
      device_ready = false;
    }
  }
};

Speaker::Speaker() : impl_(std::make_unique<Impl>()) {}

Speaker::~Speaker() { stop(); }

void Speaker::play(PcmClip clip, std::function<void()> on_done) {
  Impl &m = *impl_;
  std::lock_guard<std::mutex> lock(m.mutex);
  m.close();
  m.fire_done(); // a clip cut short still reports completion

  m.clip = std::move(clip);
  m.position = 0;
  m.on_done = std::move(on_done);
  m.done_fired = false;

  ma_device_config config = ma_device_config_init(ma_device_type_playback);
  config.playback.format = ma_format_s16;
  config.playback.channels = 1;
  config.sampleRate = static_cast<ma_uint32>(m.clip.sample_rate);
  config.dataCallback = Impl::callback;
  config.pUserData = &m;
  if (ma_device_init(nullptr, &config, &m.device) != MA_SUCCESS) {
    log::error("speaker", "could not open the speaker");
    m.fire_done();
    return;
  }
  m.device_ready = true;
  if (ma_device_start(&m.device) != MA_SUCCESS) {
    log::error("speaker", "could not start the speaker");
    m.close();
    m.fire_done();
  }
}

void Speaker::stop() {
  Impl &m = *impl_;
  std::lock_guard<std::mutex> lock(m.mutex);
  m.close();
  m.fire_done();
}

} // namespace zoal_briefing::audio
