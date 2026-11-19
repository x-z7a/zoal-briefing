#include "zoal_briefing/audio/wake/openwakeword.hpp"

#include "zoal_briefing/log.hpp"

#include <onnxruntime_c_api.h>

#include <algorithm>
#include <array>
#include <deque>
#include <random>
#include <vector>

namespace zoal_briefing::audio {

namespace {

constexpr const char *kLog = "wake";
constexpr std::size_t kMelBins = 32;
constexpr std::size_t kEmbeddingWindow = 76; // mel frames per embedding
constexpr std::size_t kEmbeddingSize = 96;
constexpr std::size_t kMelContext = 160 * 3; // extra samples the mel window needs
constexpr std::size_t kMaxMelFrames = 970;
constexpr std::size_t kMaxFeatures = 120;
constexpr int kWarmupPredictions = 5;

const OrtApi &ort() {
  static const OrtApi *api = OrtGetApiBase()->GetApi(ORT_API_VERSION);
  return *api;
}

// Turns an OrtStatus into a message, releasing it.
bool ok(OrtStatus *status, std::string *error = nullptr) {
  if (!status) return true;
  if (error) *error = ort().GetErrorMessage(status);
  ort().ReleaseStatus(status);
  return false;
}

struct Model {
  OrtSession *session = nullptr;
  std::string input;
  std::string output;
  std::vector<std::int64_t> input_shape;

  ~Model() {
    if (session) ort().ReleaseSession(session);
  }

  bool load(OrtEnv *env, OrtSessionOptions *options, const std::string &path, std::string &error) {
#ifdef _WIN32
    std::wstring wpath(path.begin(), path.end());
    if (!ok(ort().CreateSession(env, wpath.c_str(), options, &session), &error)) {
#else
    if (!ok(ort().CreateSession(env, path.c_str(), options, &session), &error)) {
#endif
      error = path + ": " + error;
      return false;
    }
    OrtAllocator *allocator = nullptr;
    if (!ok(ort().GetAllocatorWithDefaultOptions(&allocator), &error)) return false;
    char *name = nullptr;
    if (!ok(ort().SessionGetInputName(session, 0, allocator, &name), &error)) return false;
    input = name;
    allocator->Free(allocator, name);
    if (!ok(ort().SessionGetOutputName(session, 0, allocator, &name), &error)) return false;
    output = name;
    allocator->Free(allocator, name);

    OrtTypeInfo *type_info = nullptr;
    if (!ok(ort().SessionGetInputTypeInfo(session, 0, &type_info), &error)) return false;
    const OrtTensorTypeAndShapeInfo *tensor = nullptr;
    std::size_t dims = 0;
    bool good = ok(ort().CastTypeInfoToTensorInfo(type_info, &tensor), &error) &&
                ok(ort().GetDimensionsCount(tensor, &dims), &error);
    if (good) {
      input_shape.resize(dims);
      good = ok(ort().GetDimensions(tensor, input_shape.data(), dims), &error);
    }
    ort().ReleaseTypeInfo(type_info);
    return good;
  }

  // Runs on one float tensor; returns the first output's data and shape.
  bool run(OrtMemoryInfo *memory, std::vector<float> &in, const std::vector<std::int64_t> &shape,
           std::vector<float> &out, std::vector<std::int64_t> &out_shape, std::string &error) {
    OrtValue *input_value = nullptr;
    if (!ok(ort().CreateTensorWithDataAsOrtValue(memory, in.data(), in.size() * sizeof(float),
                                                 shape.data(), shape.size(),
                                                 ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT, &input_value),
            &error)) {
      return false;
    }
    const char *in_names[] = {input.c_str()};
    const char *out_names[] = {output.c_str()};
    OrtValue *output_value = nullptr;
    bool good = ok(ort().Run(session, nullptr, in_names, &input_value, 1, out_names, 1,
                             &output_value),
                   &error);
    ort().ReleaseValue(input_value);
    if (!good) return false;

    OrtTensorTypeAndShapeInfo *info = nullptr;
    std::size_t count = 0, dims = 0;
    float *data = nullptr;
    good = ok(ort().GetTensorTypeAndShape(output_value, &info), &error) &&
           ok(ort().GetTensorShapeElementCount(info, &count), &error) &&
           ok(ort().GetDimensionsCount(info, &dims), &error);
    if (good) {
      out_shape.resize(dims);
      good = ok(ort().GetDimensions(info, out_shape.data(), dims), &error) &&
             ok(ort().GetTensorMutableData(output_value, reinterpret_cast<void **>(&data)), &error);
    }
    if (info) ort().ReleaseTensorTypeAndShapeInfo(info);
    if (good) out.assign(data, data + count);
    ort().ReleaseValue(output_value);
    return good;
  }
};

// The ONNX Runtime objects sessions depend on. Declared first in Impl, so it is
// destroyed last, after every session.
struct OrtHandles {
  OrtEnv *env = nullptr;
  OrtSessionOptions *options = nullptr;
  OrtMemoryInfo *memory = nullptr;

  ~OrtHandles() {
    if (memory) ort().ReleaseMemoryInfo(memory);
    if (options) ort().ReleaseSessionOptions(options);
    if (env) ort().ReleaseEnv(env);
  }
};

} // namespace

struct OpenWakeWord::Impl {
  OrtHandles handles;
  OrtSessionOptions *&options = handles.options;
  OrtMemoryInfo *&memory = handles.memory;
  Model mel, embedding, classifier;
  std::size_t feature_frames = 16;
  WakeWordModels models;
  bool loaded = false;

  std::vector<std::int16_t> pending;   // samples not yet a whole chunk
  std::deque<std::int16_t> raw;        // recent samples, for the mel window's context
  std::deque<std::array<float, kMelBins>> mel_frames;
  std::deque<std::array<float, kEmbeddingSize>> features;
  int predictions = 0;
  float last_score = 0.0f;
  float peak_score = 0.0f;
  bool logged_run_error = false;

  // One 80 ms chunk through the whole pipeline. Returns the score, or nullopt
  // while warming up or on error.
  std::optional<float> process_chunk(const std::int16_t *chunk, bool predict) {
    raw.insert(raw.end(), chunk, chunk + kChunkSamples);
    while (raw.size() > kChunkSamples + kMelContext) raw.pop_front();
    if (!append_mel()) return std::nullopt;
    if (!append_feature()) return std::nullopt;
    if (!predict) return std::nullopt;

    std::vector<float> in;
    in.reserve(feature_frames * kEmbeddingSize);
    for (std::size_t i = features.size() - feature_frames; i < features.size(); ++i) {
      in.insert(in.end(), features[i].begin(), features[i].end());
    }
    std::vector<float> out;
    std::vector<std::int64_t> out_shape;
    std::string error;
    if (!classifier.run(memory, in,
                        {1, static_cast<std::int64_t>(feature_frames),
                         static_cast<std::int64_t>(kEmbeddingSize)},
                        out, out_shape, error) ||
        out.empty()) {
      report(error);
      return std::nullopt;
    }
    float score = out[0];
    if (predictions < kWarmupPredictions) {
      ++predictions;
      score = 0.0f;
    }
    last_score = score;
    peak_score = std::max(peak_score, score);
    return score;
  }

  bool append_mel() {
    std::vector<float> in(raw.begin(), raw.end()); // int16 values as float, unscaled
    std::vector<float> out;
    std::vector<std::int64_t> shape;
    std::string error;
    if (!mel.run(memory, in, {1, static_cast<std::int64_t>(in.size())}, out, shape, error)) {
      report(error);
      return false;
    }
    // Output [1, 1, frames, 32].
    const std::size_t frames = out.size() / kMelBins;
    for (std::size_t f = 0; f < frames; ++f) {
      std::array<float, kMelBins> frame{};
      for (std::size_t b = 0; b < kMelBins; ++b) frame[b] = out[f * kMelBins + b] / 10.0f + 2.0f;
      mel_frames.push_back(frame);
    }
    while (mel_frames.size() > kMaxMelFrames) mel_frames.pop_front();
    return true;
  }

  bool append_feature() {
    if (mel_frames.size() < kEmbeddingWindow) return false;
    std::vector<float> in;
    in.reserve(kEmbeddingWindow * kMelBins);
    for (std::size_t i = mel_frames.size() - kEmbeddingWindow; i < mel_frames.size(); ++i) {
      in.insert(in.end(), mel_frames[i].begin(), mel_frames[i].end());
    }
    std::vector<float> out;
    std::vector<std::int64_t> shape;
    std::string error;
    if (!embedding.run(memory, in,
                       {1, static_cast<std::int64_t>(kEmbeddingWindow),
                        static_cast<std::int64_t>(kMelBins), 1},
                       out, shape, error) ||
        out.size() < kEmbeddingSize) {
      report(error);
      return false;
    }
    std::array<float, kEmbeddingSize> feature{};
    std::copy(out.begin(), out.begin() + kEmbeddingSize, feature.begin());
    features.push_back(feature);
    while (features.size() > kMaxFeatures) features.pop_front();
    return true;
  }

  void report(const std::string &error) {
    if (logged_run_error) return;
    logged_run_error = true;
    log::error(kLog, "inference failed: " + error);
  }

  // Fresh buffers, primed as openWakeWord does: mel frames of ones, then
  // features from 4 s of low noise so the classifier has full input at once.
  void reset_buffers() {
    pending.clear();
    raw.clear();
    mel_frames.clear();
    features.clear();
    std::array<float, kMelBins> ones{};
    ones.fill(1.0f);
    for (std::size_t i = 0; i < kEmbeddingWindow; ++i) mel_frames.push_back(ones);

    std::mt19937 rng(1234);
    std::uniform_int_distribution<int> noise(-1000, 1000);
    std::array<std::int16_t, kChunkSamples> chunk{};
    const std::size_t chunks = 4 * kCaptureSampleRate / kChunkSamples;
    for (std::size_t c = 0; c < chunks; ++c) {
      for (auto &s : chunk) s = static_cast<std::int16_t>(noise(rng));
      process_chunk(chunk.data(), false);
    }
    raw.clear();
    predictions = 0;
    last_score = 0.0f;
    peak_score = 0.0f;
  }
};

OpenWakeWord::OpenWakeWord() : impl_(std::make_unique<Impl>()) {}

OpenWakeWord::~OpenWakeWord() = default;

bool OpenWakeWord::load(const WakeWordModels &models, std::string &error) {
  Impl &m = *impl_;
  if (!ok(ort().CreateEnv(ORT_LOGGING_LEVEL_ERROR, "zoal-briefing-wake", &m.handles.env), &error) ||
      !ok(ort().CreateSessionOptions(&m.options), &error) ||
      !ok(ort().SetIntraOpNumThreads(m.options, 1), &error) ||
      !ok(ort().SetInterOpNumThreads(m.options, 1), &error) ||
      !ok(ort().SetSessionGraphOptimizationLevel(m.options, ORT_ENABLE_ALL), &error) ||
      !ok(ort().CreateCpuMemoryInfo(OrtArenaAllocator, OrtMemTypeDefault, &m.memory), &error)) {
    return false;
  }
  if (!m.mel.load(m.handles.env, m.options, models.melspectrogram, error) ||
      !m.embedding.load(m.handles.env, m.options, models.embedding, error) ||
      !m.classifier.load(m.handles.env, m.options, models.classifier, error)) {
    return false;
  }
  // Classifier input is [1, frames, 96]; most models use 16 frames.
  if (m.classifier.input_shape.size() == 3 && m.classifier.input_shape[1] > 0) {
    m.feature_frames = static_cast<std::size_t>(m.classifier.input_shape[1]);
  }
  m.models = models;
  m.reset_buffers();
  m.loaded = true;
  return true;
}

bool OpenWakeWord::loaded() const { return impl_->loaded; }

std::optional<float> OpenWakeWord::feed(const std::int16_t *pcm, std::size_t count) {
  Impl &m = *impl_;
  if (!m.loaded) return std::nullopt;
  m.pending.insert(m.pending.end(), pcm, pcm + count);
  std::optional<float> detected;
  std::size_t offset = 0;
  while (m.pending.size() - offset >= kChunkSamples) {
    const auto score = m.process_chunk(m.pending.data() + offset, true);
    offset += kChunkSamples;
    if (score && *score >= m.models.threshold && (!detected || *score > *detected)) {
      detected = score;
    }
  }
  m.pending.erase(m.pending.begin(), m.pending.begin() + static_cast<std::ptrdiff_t>(offset));
  return detected;
}

void OpenWakeWord::reset() {
  if (impl_->loaded) impl_->reset_buffers();
}

std::string OpenWakeWord::phrase() const { return impl_->models.phrase; }

float OpenWakeWord::last_score() const { return impl_->last_score; }

float OpenWakeWord::peak_score() const { return impl_->peak_score; }

} // namespace zoal_briefing::audio
