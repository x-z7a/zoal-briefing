#ifndef ZOAL_BRIEFING_AUDIO_CONFIG_HPP
#define ZOAL_BRIEFING_AUDIO_CONFIG_HPP

#include <functional>
#include <string>

#include <nlohmann/json_fwd.hpp>

namespace zoal_briefing::audio {

inline constexpr const char *kDefaultWakeWord = "cookie monster";

// Audio settings, read from zoal-briefing.json: the top-level "wake_word"
// (the panel edits it) and the "audio" object. Every field has a default, so
// a missing file or key is fine.
struct AudioConfig {
  std::string wake_word = kDefaultWakeWord;
  // openWakeWord score (0..1) that counts as a detection.
  float wake_threshold = 0.5f;
  // Used while no model for wake_word exists yet (see docs/wake-word-training.md).
  std::string fallback_wake_model = "hey_jarvis_v0.1.onnx";

  std::string acknowledgement = "Yes, captain.";
  std::string say_again = "Say again.";

  // Model locations, relative to models_dir. An empty models_dir means the
  // host decides (the plugin uses <plugin>/models).
  std::string models_dir;
  std::string whisper_model = "whisper/ggml-small.en-q5_1.bin";
  std::string piper_voice = "piper/vits-piper-en_US-lessac-medium";

  // Capture device: a substring of its name; empty is the system default.
  std::string input_device;
  // X-Plane audio bus for speech: ui, com1, com2, pilot, copilot, interior.
  std::string output_bus = "ui";

  // End of the spoken request.
  double max_record_seconds = 8.0;
  double no_speech_timeout_seconds = 4.0;
  int end_silence_ms = 800;
};

AudioConfig audio_config_from_json(const nlohmann::json &root);

// "Cookie Monster" -> "cookie_monster", the model file's stem.
std::string wake_word_slug(const std::string &phrase);
// "hey_jarvis_v0.1.onnx" -> "hey jarvis".
std::string phrase_from_model_file(const std::string &file_name);

struct WakeModelChoice {
  std::string path;   // empty when none was found
  std::string phrase; // what the chosen model listens for
  bool fallback = false;
};

// <models_dir>/openwakeword/<slug>.onnx (or <slug>_v*.onnx), else the
// fallback model. `exists` is injectable for tests.
WakeModelChoice choose_wake_model(const AudioConfig &config, const std::string &models_dir,
                                  const std::function<bool(const std::string &)> &exists);

} // namespace zoal_briefing::audio

#endif // ZOAL_BRIEFING_AUDIO_CONFIG_HPP
