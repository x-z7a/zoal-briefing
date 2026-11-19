#include "zoal_briefing/audio/tts/voice_pool.hpp"

#include "zoal_briefing/log.hpp"

#include <cstdint>

namespace zoal_briefing::audio {

void VoicePool::add(std::string name, std::unique_ptr<Synthesizer> voice) {
  voices_.push_back({std::move(name), std::move(voice)});
}

const std::string &VoicePool::current_name() const {
  static const std::string kNone;
  return voices_.empty() ? kNone : voices_[current_].name;
}

std::size_t VoicePool::voice_for(const std::string &flight_id, std::size_t voices) {
  if (voices == 0) return 0;
  // FNV-1a: the same id picks the same voice on every run and platform.
  std::uint64_t h = 14695981039346656037ull;
  for (const unsigned char c : flight_id) {
    h ^= c;
    h *= 1099511628211ull;
  }
  return static_cast<std::size_t>(h % voices);
}

void VoicePool::set_flight(const std::string &flight_id) {
  if (voices_.size() < 2 || flight_id.empty()) return;
  std::lock_guard<std::mutex> lock(mutex_);
  if (flight_id == flight_) return;
  std::size_t voice = voice_for(flight_id, voices_.size());
  // A new flight, a new copilot: not the one from the flight before.
  if (!flight_.empty() && voice == flight_voice_) voice = (voice + 1) % voices_.size();
  flight_ = flight_id;
  flight_voice_ = voice;
  changed_ = true;
}

void VoicePool::begin_utterance() {
  if (!changed_.exchange(false)) return;
  std::lock_guard<std::mutex> lock(mutex_);
  current_ = flight_voice_;
  log::info("voice", "flight " + flight_ + ": the copilot is the " + voices_[current_].name +
                         " voice");
}

std::optional<PcmClip> VoicePool::synthesize(const std::string &text) {
  if (voices_.empty()) return std::nullopt;
  return voices_[current_].synth->synthesize(text);
}

} // namespace zoal_briefing::audio
