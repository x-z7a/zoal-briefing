#ifndef ZOAL_BRIEFING_AUDIO_TTS_VOICE_POOL_HPP
#define ZOAL_BRIEFING_AUDIO_TTS_VOICE_POOL_HPP

#include "zoal_briefing/audio/types.hpp"

#include <atomic>
#include <cstddef>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace zoal_briefing::audio {

// Several voices behind one Synthesizer, one per flight: there is one copilot
// in the cockpit, and a new flight plan is a new flight. The voice follows
// from the flight's id (SimBrief's request_id), so the same plan after a
// restart brings back the same copilot; a new flight that would get the
// previous flight's voice gets the next one instead.
//
// set_flight() may be called from any thread; the change takes effect at the
// next utterance (begin_utterance(), on the voice thread), never mid-reply.
class VoicePool final : public Synthesizer {
public:
  void add(std::string name, std::unique_ptr<Synthesizer> voice);
  std::size_t size() const { return voices_.size(); }
  // The voice the current utterance is spoken in.
  const std::string &current_name() const;

  void set_flight(const std::string &flight_id);

  void begin_utterance() override;
  std::optional<PcmClip> synthesize(const std::string &text) override;

  // The voice a flight id maps to, before avoiding the previous flight's.
  static std::size_t voice_for(const std::string &flight_id, std::size_t voices);

private:
  struct Voice {
    std::string name;
    std::unique_ptr<Synthesizer> synth;
  };
  std::vector<Voice> voices_;
  std::size_t current_ = 0; // voice thread only

  std::mutex mutex_;
  std::string flight_;
  std::size_t flight_voice_ = 0;
  std::atomic<bool> changed_{false};
};

} // namespace zoal_briefing::audio

#endif // ZOAL_BRIEFING_AUDIO_TTS_VOICE_POOL_HPP
