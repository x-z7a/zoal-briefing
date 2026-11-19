#include "zoal_briefing/audio_pipeline/stt_transcriber.hpp"

#include "zoal_briefing/log.hpp"

#include <whisper.h>

#include <algorithm>
#include <cctype>
#include <thread>

namespace zoal_briefing::audio {

namespace {

constexpr const char *kLog = "stt";

// Aviation words bias the decoder toward what pilots say.
constexpr const char *kPrompt =
    "Departure briefing, arrival briefing, runway two eight left, ILS approach, KSFO.";

// whisper.cpp and ggml are chatty at load; keep warnings and errors only.
void whisper_log(enum ggml_log_level level, const char *text, void *) {
  if (level != GGML_LOG_LEVEL_WARN && level != GGML_LOG_LEVEL_ERROR) return;
  std::string line(text ? text : "");
  while (!line.empty() && (line.back() == '\n' || line.back() == '\r')) line.pop_back();
  if (line.empty()) return;
  if (level == GGML_LOG_LEVEL_ERROR) {
    log::error("whisper", line);
  } else {
    log::warn("whisper", line);
  }
}

std::string trim(std::string s) {
  auto not_space = [](unsigned char c) { return !std::isspace(c); };
  s.erase(s.begin(), std::find_if(s.begin(), s.end(), not_space));
  s.erase(std::find_if(s.rbegin(), s.rend(), not_space).base(), s.end());
  return s;
}

// Whisper's annotations for non-speech, e.g. "[BLANK_AUDIO]" or "(wind)".
bool is_annotation(const std::string &s) {
  return s.size() >= 2 && ((s.front() == '[' && s.back() == ']') ||
                           (s.front() == '(' && s.back() == ')'));
}

} // namespace

SttTranscriber::SttTranscriber(std::string model_path) : model_path_(std::move(model_path)) {}

SttTranscriber::~SttTranscriber() {
  std::lock_guard<std::mutex> lock(mutex_);
  if (ctx_) whisper_free(ctx_);
  ctx_ = nullptr;
}

bool SttTranscriber::load(std::string &error) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (ctx_) return true;
  whisper_log_set(whisper_log, nullptr);
  whisper_context_params params = whisper_context_default_params();
  params.use_gpu = true;
  ctx_ = whisper_init_from_file_with_params(model_path_.c_str(), params);
  if (!ctx_) {
    error = "could not load whisper model " + model_path_;
    return false;
  }
  return true;
}

std::optional<std::string> SttTranscriber::transcribe(const std::vector<std::int16_t> &pcm16k) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (!ctx_) return std::nullopt;
  if (pcm16k.empty()) return std::string();

  std::vector<float> samples(pcm16k.size());
  std::transform(pcm16k.begin(), pcm16k.end(), samples.begin(),
                 [](std::int16_t s) { return static_cast<float>(s) / 32768.0f; });
  // whisper wants at least a second; pad short requests with silence.
  if (samples.size() < static_cast<std::size_t>(WHISPER_SAMPLE_RATE) + 1600) {
    samples.resize(static_cast<std::size_t>(WHISPER_SAMPLE_RATE) + 1600, 0.0f);
  }

  whisper_full_params params = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
  params.language = "en";
  params.translate = false;
  params.no_context = true;
  params.no_timestamps = true;
  params.single_segment = false;
  params.print_progress = false;
  params.print_realtime = false;
  params.print_special = false;
  params.print_timestamps = false;
  params.suppress_blank = true;
  params.initial_prompt = kPrompt;
  params.n_threads = static_cast<int>(std::clamp(std::thread::hardware_concurrency() / 2u, 1u, 4u));

  if (whisper_full(ctx_, params, samples.data(), static_cast<int>(samples.size())) != 0) {
    log::error(kLog, "whisper_full failed");
    return std::nullopt;
  }

  std::string text;
  const int segments = whisper_full_n_segments(ctx_);
  for (int i = 0; i < segments; ++i) {
    std::string segment = trim(whisper_full_get_segment_text(ctx_, i));
    if (segment.empty() || is_annotation(segment)) continue;
    if (!text.empty()) text += ' ';
    text += segment;
  }
  return trim(text);
}

} // namespace zoal_briefing::audio
