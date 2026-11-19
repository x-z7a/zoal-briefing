// voice_check: exercise the audio pipeline outside X-Plane.
//
//   voice_check selftest <models_dir>      no microphone needed: Piper speaks,
//                                          openWakeWord and whisper listen
//   voice_check live <models_dir> [config] the real loop on mic + speakers
//   voice_check say <models_dir> <text>    speak text on the speakers

#include "zoal_briefing/audio_pipeline/audio_config.hpp"
#include "zoal_briefing/audio_pipeline/miniaudio_devices.hpp"
#include "zoal_briefing/audio_pipeline/stt_transcriber.hpp"
#include "zoal_briefing/audio_pipeline/tts_speaker.hpp"
#include "zoal_briefing/audio_pipeline/voice_loop.hpp"
#include "zoal_briefing/audio_pipeline/voice_pipeline.hpp"
#include "zoal_briefing/audio_pipeline/wake_word_engine.hpp"
#include "zoal_briefing/log.hpp"
#include "zoal_briefing/state_machine/state_machine.hpp"

#include <nlohmann/json.hpp>

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>

using namespace zoal_briefing;
using namespace zoal_briefing::audio;
namespace fs = std::filesystem;

namespace {

std::atomic<bool> g_quit{false};

bool exists(const std::string &p) {
  std::error_code ec;
  return fs::is_regular_file(p, ec);
}

std::vector<std::int16_t> to_16k_with_padding(const PcmClip &clip) {
  auto pcm = resample_linear(clip.samples, clip.sample_rate, kCaptureSampleRate);
  std::vector<std::int16_t> out(kCaptureSampleRate, 0); // 1 s silence before
  out.insert(out.end(), pcm.begin(), pcm.end());
  out.insert(out.end(), kCaptureSampleRate, 0); // and after
  return out;
}

// Feeds audio through the wake engine; returns the peak score.
float peak_wake_score(WakeWordEngine &wake, const std::vector<std::int16_t> &pcm, bool &fired) {
  wake.reset();
  fired = false;
  for (std::size_t i = 0; i + kChunkSamples <= pcm.size(); i += kChunkSamples) {
    if (wake.feed(pcm.data() + i, kChunkSamples)) fired = true;
  }
  return wake.peak_score();
}

int selftest(const std::string &models_dir) {
  int failures = 0;
  auto check = [&](bool ok, const std::string &what) {
    std::printf("%s  %s\n", ok ? "PASS" : "FAIL", what.c_str());
    if (!ok) ++failures;
  };

  AudioConfig config;
  std::string error;

  TtsSpeaker tts(models_dir + "/" + config.piper_voice);
  const bool tts_ok = tts.load(error);
  check(tts_ok, "Piper voice loads" + (tts_ok ? "" : ": " + error));
  if (!tts_ok) return 1;

  auto ack = tts.synthesize(config.acknowledgement);
  check(ack && ack->seconds() > 0.3, "TTS speaks \"" + config.acknowledgement + "\"" +
                                         (ack ? " (" + std::to_string(ack->seconds()).substr(0, 4) + " s)" : ""));

  // Wake word: every model present in openwakeword/, against its own phrase
  // and against a sentence that is not it.
  const std::string oww = models_dir + "/openwakeword";
  std::vector<std::string> classifiers;
  std::error_code ec;
  for (const auto &e : fs::directory_iterator(oww, ec)) {
    const auto name = e.path().filename().string();
    if (e.path().extension() == ".onnx" && name != "melspectrogram.onnx" &&
        name != "embedding_model.onnx") {
      classifiers.push_back(e.path().string());
    }
  }
  check(!classifiers.empty(), "wake word models present in " + oww);

  const auto negative = tts.synthesize("Good morning. The weather today is clear and calm.");
  for (const auto &path : classifiers) {
    WakeWordModels models;
    models.melspectrogram = oww + "/melspectrogram.onnx";
    models.embedding = oww + "/embedding_model.onnx";
    models.classifier = path;
    models.phrase = phrase_from_model_file(path);
    models.threshold = config.wake_threshold;
    WakeWordEngine wake;
    const bool loaded = wake.load(models, error);
    check(loaded, "openWakeWord loads " + fs::path(path).filename().string() +
                      (loaded ? "" : ": " + error));
    if (!loaded) continue;

    auto spoken = tts.synthesize(models.phrase + ".");
    if (!spoken) {
      check(false, "TTS speaks \"" + models.phrase + "\"");
      continue;
    }
    bool fired = false;
    const auto start = std::chrono::steady_clock::now();
    const auto pcm = to_16k_with_padding(*spoken);
    const float peak = peak_wake_score(wake, pcm, fired);
    const double ms_per_chunk =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count() /
        static_cast<double>(pcm.size() / kChunkSamples);
    char line[160];
    std::snprintf(line, sizeof(line), "wake \"%s\" fires on the spoken phrase (peak %.2f, %.2f ms per 80 ms chunk)",
                  models.phrase.c_str(), static_cast<double>(peak), ms_per_chunk);
    check(fired, line);

    if (negative) {
      const float neg = peak_wake_score(wake, to_16k_with_padding(*negative), fired);
      std::snprintf(line, sizeof(line), "wake \"%s\" stays quiet on other speech (peak %.2f)",
                    models.phrase.c_str(), static_cast<double>(neg));
      check(!fired, line);
    }
  }

  // Speech to text.
  SttTranscriber stt(models_dir + "/" + config.whisper_model);
  const bool stt_ok = stt.load(error);
  check(stt_ok, "whisper loads" + (stt_ok ? "" : ": " + error));
  if (stt_ok) {
    const std::string said = "Departure briefing for runway two eight left.";
    auto request = tts.synthesize(said);
    if (request) {
      const auto start = std::chrono::steady_clock::now();
      auto text = stt.transcribe(to_16k_with_padding(*request));
      const double took = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
      std::string lower = text.value_or("");
      for (auto &c : lower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
      char line[256];
      std::snprintf(line, sizeof(line), "whisper hears \"%s\" as \"%s\" (%.2f s)", said.c_str(),
                    text.value_or("<failed>").c_str(), took);
      check(lower.find("departure") != std::string::npos && lower.find("runway") != std::string::npos,
            line);
    }
  }

  // The whole loop with the real engines: the wake phrase, then a request,
  // as if spoken into the microphone. Playback "finishes" at once.
  if (stt_ok && !classifiers.empty()) {
    struct InstantOutput : AudioOutput {
      std::vector<double> seconds;
      void play(PcmClip clip, std::function<void()> on_done) override {
        seconds.push_back(clip.seconds());
        on_done();
      }
      void stop() override {}
    } output;

    AudioConfig loop_config;
    const auto choice = choose_wake_model(loop_config, models_dir, exists);
    WakeWordModels models;
    models.melspectrogram = oww + "/melspectrogram.onnx";
    models.embedding = oww + "/embedding_model.onnx";
    models.classifier = choice.path;
    models.phrase = choice.phrase;
    models.threshold = loop_config.wake_threshold;
    WakeWordEngine wake;
    wake.load(models, error);

    state::StateMachine machine;
    std::vector<std::string> seen;
    machine.on_transition([&](state::State, state::State to, const std::string &) {
      seen.push_back(state::to_string(to));
    });
    VoiceLoop loop({}, wake, stt, tts, output, machine);

    auto feed = [&](const std::vector<std::int16_t> &pcm) {
      for (std::size_t i = 0; i + kChunkSamples <= pcm.size(); i += kChunkSamples) {
        loop.push_audio(pcm.data() + i, kChunkSamples);
        loop.step();
        loop.step(); // lets the loop act on a finished playback
      }
    };
    std::printf("---- loop: \"%s\", then a request ----\n", choice.phrase.c_str());
    auto phrase = tts.synthesize(choice.phrase + ".");
    auto request = tts.synthesize("Arrival briefing for San Francisco.");
    if (phrase && request) {
      feed(to_16k_with_padding(*phrase));
      feed(to_16k_with_padding(*request));
      std::vector<std::int16_t> quiet(kCaptureSampleRate * 2, 0);
      feed(quiet);
    }
    std::string path;
    for (const auto &s : seen) path += (path.empty() ? "" : " -> ") + s;
    std::printf("----\n");
    const std::string heard = loop.last_transcript();
    check(seen.size() >= 5 && seen[0] == "WAKE_DETECTED" && seen[1] == "LISTENING" &&
              seen[2] == "STT_COMPLETE" && seen[3] == "PLAYBACK" && seen[4] == "IDLE",
          "loop walks IDLE -> " + path);
    check(heard.find("rrival") != std::string::npos, "loop echoes \"" + heard + "\"");
  }

  std::printf("%s\n", failures == 0 ? "selftest passed" : "selftest FAILED");
  return failures == 0 ? 0 : 1;
}

AudioConfig read_config(const std::string &path) {
  if (path.empty()) return {};
  std::ifstream f(path);
  if (!f) {
    log::warn("voice_check", "no config at " + path + "; using defaults");
    return {};
  }
  return audio_config_from_json(nlohmann::json::parse(f, nullptr, false));
}

int live(const std::string &models_dir, const std::string &config_path) {
  std::signal(SIGINT, [](int) { g_quit = true; });
  state::StateMachine machine;
  SpeakerOutput speaker;
  VoicePipeline pipeline(speaker, machine);
  pipeline.start(read_config(config_path), models_dir);

  std::printf("Live: say the wake word. Type 'l' + Enter to listen without it, 'q' to quit.\n");
  std::thread input([&] {
    std::string line;
    while (!g_quit && std::getline(std::cin, line)) {
      if (line == "q") g_quit = true;
      if (line == "l") pipeline.request_listen();
      if (line == "c") pipeline.request_cancel();
    }
    g_quit = true;
  });
  while (!g_quit) std::this_thread::sleep_for(std::chrono::milliseconds(100));
  pipeline.stop();
  input.detach();
  return 0;
}

int say(const std::string &models_dir, const std::string &text) {
  AudioConfig config;
  std::string error;
  TtsSpeaker tts(models_dir + "/" + config.piper_voice);
  if (!tts.load(error)) {
    std::fprintf(stderr, "%s\n", error.c_str());
    return 1;
  }
  auto clip = tts.synthesize(text);
  if (!clip) return 1;
  SpeakerOutput speaker;
  std::atomic<bool> done{false};
  speaker.play(*clip, [&] { done = true; });
  while (!done) std::this_thread::sleep_for(std::chrono::milliseconds(20));
  return 0;
}

} // namespace

int main(int argc, char **argv) {
  if (argc < 3) {
    std::fprintf(stderr,
                 "usage: %s selftest <models_dir>\n"
                 "       %s live <models_dir> [zoal-briefing.json]\n"
                 "       %s say <models_dir> <text>\n",
                 argv[0], argv[0], argv[0]);
    return 2;
  }
  const std::string mode = argv[1];
  const std::string models_dir = argv[2];
  if (mode == "selftest") return selftest(models_dir);
  if (mode == "live") return live(models_dir, argc > 3 ? argv[3] : "");
  if (mode == "say" && argc > 3) return say(models_dir, argv[3]);
  std::fprintf(stderr, "unknown mode %s\n", mode.c_str());
  return 2;
}
