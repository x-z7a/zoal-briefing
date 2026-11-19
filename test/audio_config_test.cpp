#include "check.hpp"

#include "zoal_briefing/audio_pipeline/audio_config.hpp"
#include "zoal_briefing/audio_pipeline/audio_types.hpp"

#include <nlohmann/json.hpp>

#include <set>

using namespace zoal_briefing::audio;

int main() {
  // Defaults: the wake word is "hey jarvis".
  const AudioConfig defaults = audio_config_from_json(nlohmann::json::object());
  CHECK(defaults.wake_word == "hey jarvis");
  CHECK(defaults.acknowledgement == "Yes, captain.");
  CHECK(audio_config_from_json(nlohmann::json()).wake_word == "hey jarvis");

  // The panel's top-level wake_word wins; an empty one keeps the default.
  auto j = nlohmann::json::parse(R"({"wake_word": "hey jarvis",
      "audio": {"wake_threshold": 0.7, "acknowledgement": "Go ahead.", "output_bus": "copilot",
                "max_record_seconds": 100, "end_silence_ms": "oops"}})");
  AudioConfig c = audio_config_from_json(j);
  CHECK(c.wake_word == "hey jarvis");
  CHECK(c.wake_threshold > 0.69f && c.wake_threshold < 0.71f);
  CHECK(c.acknowledgement == "Go ahead.");
  CHECK(c.output_bus == "copilot");
  CHECK(c.max_record_seconds == 30.0); // clamped
  CHECK(c.end_silence_ms == 800);      // wrong type keeps the default
  CHECK(audio_config_from_json(nlohmann::json::parse(R"({"wake_word": ""})")).wake_word ==
        "hey jarvis");

  CHECK(wake_word_slug("hey jarvis") == "cookie_monster");
  CHECK(wake_word_slug("  hey,  jarvis! ") == "hey_jarvis");
  CHECK(phrase_from_model_file("/m/openwakeword/hey_jarvis_v0.1.onnx") == "hey jarvis");
  CHECK(phrase_from_model_file("cookie_monster.onnx") == "hey jarvis");

  // Model choice: the trained phrase model first, then the fallback.
  std::set<std::string> files = {"/m/openwakeword/hey_jarvis_v0.1.onnx"};
  auto exists = [&](const std::string &p) { return files.count(p) > 0; };
  WakeModelChoice choice = choose_wake_model(defaults, "/m", exists);
  CHECK(choice.fallback);
  CHECK(choice.path == "/m/openwakeword/hey_jarvis_v0.1.onnx");
  CHECK(choice.phrase == "hey jarvis");

  files.insert("/m/openwakeword/cookie_monster.onnx");
  choice = choose_wake_model(defaults, "/m", exists);
  CHECK(!choice.fallback);
  CHECK(choice.path == "/m/openwakeword/cookie_monster.onnx");
  CHECK(choice.phrase == "hey jarvis");

  files.clear();
  CHECK(choose_wake_model(defaults, "/m", exists).path.empty());

  // Resampling keeps duration.
  std::vector<std::int16_t> tone(22050, 1000);
  auto out = resample_linear(tone, 22050, 16000);
  CHECK(out.size() == 16000);
  CHECK(out[100] == 1000);

  return test_result("audio_config");
}
