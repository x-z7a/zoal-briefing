#include "zoal_briefing/audio/input/endpointer.hpp"

#include <algorithm>
#include <cmath>

namespace zoal_briefing::audio {

double Endpointer::rms(const std::int16_t *pcm, std::size_t count) {
  if (count == 0) return 0.0;
  double sum = 0.0;
  for (std::size_t i = 0; i < count; ++i) {
    const double v = pcm[i];
    sum += v * v;
  }
  return std::sqrt(sum / static_cast<double>(count));
}

double Endpointer::track_floor(double floor, double level) {
  if (floor <= 0.0) return std::max(level, 1.0);
  const double next = level < floor ? 0.7 * floor + 0.3 * level : 0.98 * floor + 0.02 * level;
  return std::max(next, 1.0);
}

const char *to_string(Endpointer::Status status) {
  switch (status) {
  case Endpointer::Status::WAITING: return "waiting";
  case Endpointer::Status::SPEAKING: return "speaking";
  case Endpointer::Status::DONE: return "done";
  case Endpointer::Status::NO_SPEECH: return "no speech";
  case Endpointer::Status::MAX_LENGTH: return "max length";
  }
  return "?";
}

Endpointer::Endpointer(EndpointerConfig config) : cfg_(config) { reset(); }

void Endpointer::reset(double ambient_rms) {
  samples_ = 0;
  silence_samples_ = 0;
  loud_run_ = 0;
  speech_started_ = false;
  noise_floor_ = ambient_rms > 0.0 ? ambient_rms : cfg_.min_speech_rms / cfg_.speech_ratio;
  status_ = Status::WAITING;
}

double Endpointer::elapsed_seconds() const {
  return static_cast<double>(samples_) / cfg_.sample_rate;
}

Endpointer::Status Endpointer::feed(const std::int16_t *pcm, std::size_t count) {
  if (status_ == Status::DONE || status_ == Status::NO_SPEECH || status_ == Status::MAX_LENGTH) {
    return status_;
  }
  samples_ += count;
  const double level = rms(pcm, count);

  // Once speech starts the floor only follows quiet, so speech cannot raise it.
  if (!speech_started_ || level < noise_floor_) noise_floor_ = track_floor(noise_floor_, level);

  const bool loud = level >= std::max(cfg_.min_speech_rms, noise_floor_ * cfg_.speech_ratio);

  if (!speech_started_) {
    loud_run_ = loud ? loud_run_ + 1 : 0;
    if (loud_run_ >= cfg_.start_chunks) {
      speech_started_ = true;
      silence_samples_ = 0;
      status_ = Status::SPEAKING;
    } else if (elapsed_seconds() >= cfg_.no_speech_timeout_seconds) {
      status_ = Status::NO_SPEECH;
    }
  } else {
    silence_samples_ = loud ? 0 : silence_samples_ + count;
    const std::size_t end_samples =
        static_cast<std::size_t>(cfg_.end_silence_ms) * static_cast<std::size_t>(cfg_.sample_rate) / 1000u;
    if (silence_samples_ >= end_samples) status_ = Status::DONE;
  }

  if ((status_ == Status::WAITING || status_ == Status::SPEAKING) &&
      elapsed_seconds() >= cfg_.max_seconds) {
    status_ = speech_started_ ? Status::MAX_LENGTH : Status::NO_SPEECH;
  }
  return status_;
}

} // namespace zoal_briefing::audio
