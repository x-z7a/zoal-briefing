#include "zoal_briefing/audio/input/mic.hpp"

#include "audio/miniaudio.hpp"
#include "zoal_briefing/audio/types.hpp"
#include "zoal_briefing/log.hpp"

#include <algorithm>
#include <cctype>

namespace zoal_briefing::audio {

namespace {

std::string lower(std::string s) {
  std::transform(s.begin(), s.end(), s.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return s;
}

} // namespace

struct Mic::Impl {
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

Mic::Mic() : impl_(std::make_unique<Impl>()) {}

Mic::~Mic() { stop(); }

bool Mic::start(const std::string &device_hint, OnAudio on_audio, std::string &error) {
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

void Mic::stop() {
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

bool Mic::running() const {
  return impl_->device_ready && ma_device_is_started(&impl_->device);
}

std::string Mic::device_name() const { return impl_->device_name; }

} // namespace zoal_briefing::audio
