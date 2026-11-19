#include "zoal_briefing/audio/voice_pipeline.hpp"

#include "zoal_briefing/audio/input/mic.hpp"
#include "zoal_briefing/audio/stt/whisper_stt.hpp"
#include "zoal_briefing/audio/tts/piper_tts.hpp"
#include "zoal_briefing/audio/voice_loop.hpp"
#include "zoal_briefing/audio/wake/openwakeword.hpp"
#include "zoal_briefing/log.hpp"

#include <chrono>
#include <cstdio>
#include <filesystem>

namespace zoal_briefing::audio {

namespace fs = std::filesystem;

namespace {

constexpr const char *kLog = "audio";

class Stopwatch {
public:
  std::string lap() {
    const auto now = std::chrono::steady_clock::now();
    const double s = std::chrono::duration<double>(now - last_).count();
    last_ = now;
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.2f s", s);
    return buf;
  }

private:
  std::chrono::steady_clock::time_point last_ = std::chrono::steady_clock::now();
};

bool file_exists(const std::string &path) {
  std::error_code ec;
  return fs::is_regular_file(path, ec);
}

} // namespace

VoicePipeline::VoicePipeline(AudioOutput &output, state::StateMachine &machine,
                             Responder *responder)
    : output_(output), machine_(machine), responder_(responder) {}

VoicePipeline::~VoicePipeline() { stop(); }

void VoicePipeline::start(const AudioConfig &config, const std::string &models_dir) {
  stop();
  stopping_ = false;
  set_status("loading");
  loader_ = std::thread([this, config, models_dir] { load(config, models_dir); });
}

void VoicePipeline::load(AudioConfig config, std::string models_dir) {
  Stopwatch clock;
  log::info(kLog, "loading the audio pipeline from " + models_dir);

  // Wake word: the model for the configured phrase, else the fallback.
  const WakeModelChoice choice = choose_wake_model(config, models_dir, file_exists);
  if (choice.path.empty()) {
    set_status("no wake word model in " + models_dir + "/openwakeword");
    log::error(kLog, status());
    return;
  }
  if (choice.fallback) {
    log::warn(kLog, "no model for wake word \"" + config.wake_word + "\" (expected openwakeword/" +
                        wake_word_slug(config.wake_word) + ".onnx); using \"" + choice.phrase +
                        "\" until it is trained -- see docs/wake-word-training.md");
  }
  WakeWordModels models;
  models.melspectrogram = models_dir + "/openwakeword/melspectrogram.onnx";
  models.embedding = models_dir + "/openwakeword/embedding_model.onnx";
  models.classifier = choice.path;
  models.phrase = choice.phrase;
  models.threshold = config.wake_threshold;

  std::string error;
  auto wake = std::make_unique<OpenWakeWord>();
  if (!wake->load(models, error)) {
    set_status("wake word: " + error);
    log::error(kLog, status());
    return;
  }
  log::info(kLog, "wake word \"" + choice.phrase + "\" loaded (" +
                      fs::path(choice.path).filename().string() + ", threshold " +
                      std::to_string(config.wake_threshold).substr(0, 4) + ", " + clock.lap() + ")");
  if (stopping_) return;

  auto stt = std::make_unique<WhisperStt>(models_dir + "/" + config.whisper_model);
  if (!stt->load(error)) {
    set_status("speech to text: " + error);
    log::error(kLog, status());
    return;
  }
  log::info(kLog, "whisper loaded (" + config.whisper_model + ", " + clock.lap() + ")");
  if (stopping_) return;

  auto tts = std::make_unique<PiperTts>(models_dir + "/" + config.piper_voice);
  if (!tts->load(error)) {
    set_status("text to speech: " + error);
    log::error(kLog, status());
    return;
  }
  log::info(kLog, "Piper voice loaded (" + config.piper_voice + ", " + clock.lap() + ")");
  if (stopping_) return;

  VoiceLoopConfig loop_config;
  loop_config.acknowledgement = config.acknowledgement;
  loop_config.say_again = config.say_again;
  loop_config.endpointer.max_seconds = config.max_record_seconds;
  loop_config.endpointer.no_speech_timeout_seconds = config.no_speech_timeout_seconds;
  loop_config.endpointer.end_silence_ms = config.end_silence_ms;

  wake_ = std::move(wake);
  stt_ = std::move(stt);
  tts_ = std::move(tts);
  loop_ = std::make_unique<VoiceLoop>(loop_config, *wake_, *stt_, *tts_, output_, machine_,
                                      responder_);
  loop_->start();

  mic_ = std::make_unique<Mic>();
  VoiceLoop *loop = loop_.get();
  if (!mic_->start(config.input_device,
                   [loop](const std::int16_t *pcm, std::size_t n) { loop->push_audio(pcm, n); },
                   error)) {
    set_status("microphone: " + error);
    log::error(kLog, status());
    // Without audio the loop could only time out, so stop it.
    loop_->stop();
    return;
  }
  log::info(kLog, "microphone open: " + mic_->device_name());

  set_status("listening for \"" + choice.phrase + "\"");
  ready_ = true;
  log::info(kLog, "audio pipeline ready, " + status());
}

void VoicePipeline::stop() {
  stopping_ = true;
  if (loader_.joinable()) loader_.join();
  const bool was_ready = ready_.exchange(false);
  if (mic_) mic_->stop(); // no more audio into the loop
  if (loop_) loop_->stop();
  mic_.reset();
  loop_.reset();
  tts_.reset();
  stt_.reset();
  wake_.reset();
  if (was_ready) {
    set_status("stopped");
    log::info(kLog, "audio pipeline stopped");
  }
}

std::string VoicePipeline::status() const {
  std::lock_guard<std::mutex> lock(status_mutex_);
  return status_;
}

void VoicePipeline::set_status(std::string status) {
  std::lock_guard<std::mutex> lock(status_mutex_);
  status_ = std::move(status);
}

void VoicePipeline::request_listen() {
  if (ready_ && loop_) {
    loop_->request_listen();
  } else {
    log::warn(kLog, "listen ignored: audio pipeline " + status());
  }
}

void VoicePipeline::request_cancel() {
  if (ready_ && loop_) loop_->request_cancel();
}

SpeechProgress VoicePipeline::speech_progress() const {
  return (ready_ && loop_) ? loop_->speech_progress() : SpeechProgress{};
}

std::string VoicePipeline::last_transcript() const {
  return (ready_ && loop_) ? loop_->last_transcript() : std::string();
}

} // namespace zoal_briefing::audio
