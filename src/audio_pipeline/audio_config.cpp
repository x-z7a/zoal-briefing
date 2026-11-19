#include "zoal_briefing/audio_pipeline/audio_config.hpp"

#include "zoal_briefing/audio_pipeline/audio_types.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <nlohmann/json.hpp>

namespace zoal_briefing::audio {

namespace {

template <typename T> void read(const nlohmann::json &obj, const char *key, T &out) {
  auto it = obj.find(key);
  if (it == obj.end() || it->is_null()) return;
  try {
    out = it->get<T>();
  } catch (const nlohmann::json::exception &) {
    // Wrong type: keep the default.
  }
}

void read_text(const nlohmann::json &obj, const char *key, std::string &out) {
  auto it = obj.find(key);
  if (it != obj.end() && it->is_string() && !it->get<std::string>().empty()) {
    out = it->get<std::string>();
  }
}

} // namespace

AudioConfig audio_config_from_json(const nlohmann::json &root) {
  AudioConfig c;
  if (!root.is_object()) return c;

  auto it = root.find("audio");
  const nlohmann::json empty = nlohmann::json::object();
  const auto &a = (it != root.end() && it->is_object()) ? *it : empty;
  read_text(a, "wake_word", c.wake_word);
  read_text(root, "wake_word", c.wake_word); // the panel's key wins
  read(a, "wake_threshold", c.wake_threshold);
  read_text(a, "fallback_wake_model", c.fallback_wake_model);
  read_text(a, "acknowledgement", c.acknowledgement);
  read_text(a, "say_again", c.say_again);
  read_text(a, "models_dir", c.models_dir);
  read_text(a, "whisper_model", c.whisper_model);
  read_text(a, "piper_voice", c.piper_voice);
  read(a, "input_device", c.input_device);
  read_text(a, "output_bus", c.output_bus);
  read(a, "max_record_seconds", c.max_record_seconds);
  read(a, "no_speech_timeout_seconds", c.no_speech_timeout_seconds);
  read(a, "end_silence_ms", c.end_silence_ms);

  c.wake_threshold = std::clamp(c.wake_threshold, 0.05f, 0.99f);
  c.max_record_seconds = std::clamp(c.max_record_seconds, 1.0, 30.0);
  c.no_speech_timeout_seconds = std::clamp(c.no_speech_timeout_seconds, 1.0, c.max_record_seconds);
  c.end_silence_ms = std::clamp(c.end_silence_ms, 200, 3000);
  return c;
}

std::string wake_word_slug(const std::string &phrase) {
  std::string slug;
  bool pending_sep = false;
  for (char c : phrase) {
    const auto ch = static_cast<unsigned char>(c);
    if (std::isalnum(ch)) {
      if (pending_sep && !slug.empty()) slug += '_';
      pending_sep = false;
      slug += static_cast<char>(std::tolower(ch));
    } else {
      pending_sep = true;
    }
  }
  return slug;
}

std::string phrase_from_model_file(const std::string &file_name) {
  std::string stem = file_name;
  if (auto slash = stem.find_last_of("/\\"); slash != std::string::npos) stem = stem.substr(slash + 1);
  if (auto dot = stem.rfind(".onnx"); dot != std::string::npos) stem = stem.substr(0, dot);
  if (auto ver = stem.find("_v"); ver != std::string::npos && ver + 2 < stem.size() &&
                                    std::isdigit(static_cast<unsigned char>(stem[ver + 2]))) {
    stem = stem.substr(0, ver);
  }
  std::replace(stem.begin(), stem.end(), '_', ' ');
  return stem;
}

WakeModelChoice choose_wake_model(const AudioConfig &config, const std::string &models_dir,
                                  const std::function<bool(const std::string &)> &exists) {
  const std::string dir = models_dir + "/openwakeword/";
  const std::string slug = wake_word_slug(config.wake_word);
  if (!slug.empty()) {
    for (const std::string &name : {slug + ".onnx", slug + "_v0.1.onnx"}) {
      if (exists(dir + name)) return {dir + name, config.wake_word, false};
    }
  }
  if (!config.fallback_wake_model.empty() && exists(dir + config.fallback_wake_model)) {
    return {dir + config.fallback_wake_model, phrase_from_model_file(config.fallback_wake_model),
            true};
  }
  return {};
}

std::vector<std::int16_t> resample_linear(const std::vector<std::int16_t> &in, int from_hz,
                                          int to_hz) {
  if (in.empty() || from_hz <= 0 || to_hz <= 0 || from_hz == to_hz) return in;
  const double ratio = static_cast<double>(from_hz) / to_hz;
  const auto out_len = static_cast<std::size_t>(static_cast<double>(in.size()) / ratio);
  std::vector<std::int16_t> out(out_len);
  for (std::size_t i = 0; i < out_len; ++i) {
    const double pos = static_cast<double>(i) * ratio;
    const auto i0 = static_cast<std::size_t>(pos);
    const std::size_t i1 = std::min(i0 + 1, in.size() - 1);
    const double frac = pos - static_cast<double>(i0);
    const double v = in[i0] * (1.0 - frac) + in[i1] * frac;
    out[i] = static_cast<std::int16_t>(std::lround(v));
  }
  return out;
}

} // namespace zoal_briefing::audio
