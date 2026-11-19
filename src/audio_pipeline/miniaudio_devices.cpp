#include "zoal_briefing/audio_pipeline/miniaudio_devices.hpp"

#include "zoal_briefing/log.hpp"

#define MA_NO_DECODING
#define MA_NO_ENCODING
#define MA_NO_GENERATION
#define MA_NO_RESOURCE_MANAGER
#define MA_NO_NODE_GRAPH
#define MA_NO_ENGINE
#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio.h>

#include <algorithm>
#include <atomic>
#include <cstring>
#include <mutex>

namespace zoal_briefing::audio {

namespace {

std::string lower(std::string s) {
  std::transform(s.begin(), s.end(), s.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return s;
}

} // namespace

// --- MicCapture -----------------------------------------------------------------

struct MicCapture::Impl {
  ma_context context{};
  ma_device device{};
  bool context_ready = false;
  bool device_ready = false;
  OnAudio on_audio;
  std::string device_name;

  static void callback(ma_device *device, void *, const void *input, ma_uint32 frames) {
    auto *self = static_cast<Impl *>(device->pUserData);
    if (input && self->on_audio) {
      self->on_audio(static_cast<const std::int16_t *>(input), frames);
    }
  }
};

MicCapture::MicCapture() : impl_(std::make_unique<Impl>()) {}

MicCapture::~MicCapture() { stop(); }

bool MicCapture::start(const std::string &device_hint, OnAudio on_audio, std::string &error) {
  stop();
  Impl &m = *impl_;
  if (ma_context_init(nullptr, 0, nullptr, &m.context) != MA_SUCCESS) {
    error = "could not initialize the audio system";
    return false;
  }
  m.context_ready = true;

  ma_device_info *captures = nullptr;
  ma_uint32 capture_count = 0;
  const ma_device_id *chosen = nullptr;
  m.device_name.clear();
  if (ma_context_get_devices(&m.context, nullptr, nullptr, &captures, &capture_count) ==
      MA_SUCCESS) {
    const std::string hint = lower(device_hint);
    for (ma_uint32 i = 0; i < capture_count; ++i) {
      const std::string name = captures[i].name;
      if (!hint.empty() && lower(name).find(hint) != std::string::npos) {
        chosen = &captures[i].id;
        m.device_name = name;
        break;
      }
      if (hint.empty() && captures[i].isDefault) m.device_name = name;
    }
    if (!hint.empty() && !chosen) {
      log::warn("mic", "no input device matching \"" + device_hint + "\"; using the default");
    }
  }

  ma_device_config config = ma_device_config_init(ma_device_type_capture);
  config.capture.pDeviceID = chosen;
  config.capture.format = ma_format_s16;
  config.capture.channels = 1;
  config.sampleRate = kCaptureSampleRate;
  config.periodSizeInMilliseconds = 20;
  config.dataCallback = Impl::callback;
  config.pUserData = &m;
  m.on_audio = std::move(on_audio);

  if (ma_device_init(&m.context, &config, &m.device) != MA_SUCCESS) {
    error = "could not open the microphone";
    stop();
    return false;
  }
  m.device_ready = true;
  if (m.device_name.empty()) m.device_name = m.device.capture.name;
  if (ma_device_start(&m.device) != MA_SUCCESS) {
    error = "could not start the microphone (is microphone access allowed?)";
    stop();
    return false;
  }
  return true;
}

void MicCapture::stop() {
  Impl &m = *impl_;
  if (m.device_ready) {
    ma_device_uninit(&m.device);
    m.device_ready = false;
  }
  if (m.context_ready) {
    ma_context_uninit(&m.context);
    m.context_ready = false;
  }
}

bool MicCapture::running() const {
  return impl_->device_ready && ma_device_is_started(&impl_->device);
}

std::string MicCapture::device_name() const { return impl_->device_name; }

// --- SpeakerOutput --------------------------------------------------------------

struct SpeakerOutput::Impl {
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

SpeakerOutput::SpeakerOutput() : impl_(std::make_unique<Impl>()) {}

SpeakerOutput::~SpeakerOutput() { stop(); }

void SpeakerOutput::play(PcmClip clip, std::function<void()> on_done) {
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

void SpeakerOutput::stop() {
  Impl &m = *impl_;
  std::lock_guard<std::mutex> lock(m.mutex);
  m.close();
  m.fire_done();
}

} // namespace zoal_briefing::audio
