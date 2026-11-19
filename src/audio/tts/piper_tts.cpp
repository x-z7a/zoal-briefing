#include "zoal_briefing/audio/tts/piper_tts.hpp"

#include <sherpa-onnx/c-api/c-api.h>

#include <algorithm>
#include <cmath>
#include <filesystem>

namespace zoal_briefing::audio {

namespace fs = std::filesystem;

PiperTts::PiperTts(std::string voice_dir) : voice_dir_(std::move(voice_dir)) {}

PiperTts::~PiperTts() {
  std::lock_guard<std::mutex> lock(mutex_);
  if (tts_) SherpaOnnxDestroyOfflineTts(tts_);
  tts_ = nullptr;
}

bool PiperTts::load(std::string &error) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (tts_) return true;

  std::error_code ec;
  std::string model;
  for (const auto &entry : fs::directory_iterator(voice_dir_, ec)) {
    if (entry.path().extension() == ".onnx") {
      model = entry.path().string();
      break;
    }
  }
  const std::string tokens = (fs::path(voice_dir_) / "tokens.txt").string();
  const std::string data_dir = (fs::path(voice_dir_) / "espeak-ng-data").string();
  if (model.empty() || !fs::exists(tokens, ec) || !fs::is_directory(data_dir, ec)) {
    error = "no Piper voice (model .onnx, tokens.txt, espeak-ng-data) in " + voice_dir_;
    return false;
  }

  SherpaOnnxOfflineTtsConfig config{};
  config.model.vits.model = model.c_str();
  config.model.vits.tokens = tokens.c_str();
  config.model.vits.data_dir = data_dir.c_str();
  config.model.vits.noise_scale = 0.667f;
  config.model.vits.noise_scale_w = 0.8f;
  config.model.vits.length_scale = 1.0f;
  config.model.num_threads = 2;
  config.model.debug = 0;
  config.model.provider = "cpu";
  config.max_num_sentences = 1;
  config.silence_scale = 0.2f;

  tts_ = SherpaOnnxCreateOfflineTts(&config);
  if (!tts_) {
    error = "sherpa-onnx could not load the Piper voice " + model;
    return false;
  }
  return true;
}

std::optional<PcmClip> PiperTts::synthesize(const std::string &text) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (!tts_ || text.empty()) return std::nullopt;

  SherpaOnnxGenerationConfig gen{};
  gen.silence_scale = 0.2f;
  gen.speed = 1.0f;
  gen.sid = 0;
  const SherpaOnnxGeneratedAudio *audio =
      SherpaOnnxOfflineTtsGenerateWithConfig(tts_, text.c_str(), &gen, nullptr, nullptr);
  if (!audio) return std::nullopt;

  PcmClip clip;
  clip.sample_rate = audio->sample_rate;
  clip.samples.resize(static_cast<std::size_t>(std::max(audio->n, 0)));
  for (std::size_t i = 0; i < clip.samples.size(); ++i) {
    const float v = std::clamp(audio->samples[i], -1.0f, 1.0f);
    clip.samples[i] = static_cast<std::int16_t>(std::lround(v * 32767.0f));
  }
  SherpaOnnxDestroyOfflineTtsGeneratedAudio(audio);
  if (clip.samples.empty()) return std::nullopt;
  return clip;
}

} // namespace zoal_briefing::audio
