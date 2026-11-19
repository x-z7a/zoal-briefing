#include "check.hpp"

#include "zoal_briefing/audio/config.hpp"
#include "zoal_briefing/audio/tts/voice_pool.hpp"
#include "zoal_briefing/log.hpp"

#include <nlohmann/json.hpp>

#include <memory>
#include <set>
#include <string>
#include <vector>

using namespace zoal_briefing::audio;

namespace {

// Says which voice spoke by the clip's sample rate.
struct Tagged : Synthesizer {
  int tag;
  explicit Tagged(int t) : tag(t) {}
  std::optional<PcmClip> synthesize(const std::string &) override {
    PcmClip clip;
    clip.sample_rate = tag;
    clip.samples.assign(10, 1);
    return clip;
  }
};

int voice_of(VoicePool &pool) {
  const auto clip = pool.synthesize("Runway two eight left.");
  return clip ? clip->sample_rate : -1;
}

} // namespace

int main() {
  zoal_briefing::log::set_sink([](const std::string &) {});

  // A pool of one, or none.
  {
    VoicePool empty;
    CHECK(!empty.synthesize("x"));
    VoicePool one;
    one.add("lessac", std::make_unique<Tagged>(1));
    one.set_flight("158012345");
    one.begin_utterance();
    CHECK(voice_of(one) == 1 && one.current_name() == "lessac");
  }

  // One copilot per flight: the voice changes with a new flight, not per reply.
  {
    VoicePool pool;
    for (int i = 0; i < 5; ++i) pool.add("v" + std::to_string(i), std::make_unique<Tagged>(i));

    pool.set_flight("158012345");
    pool.begin_utterance();
    const int first = voice_of(pool);
    CHECK(first == static_cast<int>(VoicePool::voice_for("158012345", 5)));
    for (int reply = 0; reply < 20; ++reply) {
      pool.begin_utterance(); // replies within the flight
      CHECK(voice_of(pool) == first);
    }
    pool.set_flight("158012345"); // the same plan checked again
    pool.begin_utterance();
    CHECK(voice_of(pool) == first);

    // A new flight changes the voice, but only when the next reply starts.
    pool.set_flight("158099999");
    CHECK(voice_of(pool) == first); // mid-reply: unchanged
    pool.begin_utterance();
    CHECK(voice_of(pool) != first);

    // Never the previous flight's voice, over many flights; all voices used.
    std::set<int> used;
    int last = voice_of(pool);
    for (int flight = 0; flight < 200; ++flight) {
      pool.set_flight(std::to_string(160000000 + flight * 7919));
      pool.begin_utterance();
      const int v = voice_of(pool);
      CHECK(v != last);
      used.insert(v);
      last = v;
    }
    CHECK(used.size() == 5);
  }

  // The same plan after a restart: the same copilot.
  {
    VoicePool a, b;
    for (int i = 0; i < 5; ++i) {
      a.add("v" + std::to_string(i), std::make_unique<Tagged>(i));
      b.add("v" + std::to_string(i), std::make_unique<Tagged>(i));
    }
    a.set_flight("194073880");
    b.set_flight("194073880");
    a.begin_utterance();
    b.begin_utterance();
    CHECK(voice_of(a) == voice_of(b));
  }

  // The config: five voices by default, a list, or one old-style voice.
  {
    const AudioConfig defaults = audio_config_from_json(nlohmann::json::object());
    CHECK(defaults.piper_voices.size() == 5);
    CHECK(defaults.piper_voices.front() == "piper/vits-piper-en_US-lessac-medium");

    const auto list = audio_config_from_json(
        nlohmann::json::parse(R"({"audio":{"piper_voices":["piper/a","piper/b"]}})"));
    CHECK((list.piper_voices == std::vector<std::string>{"piper/a", "piper/b"}));

    const auto one = audio_config_from_json(
        nlohmann::json::parse(R"({"audio":{"piper_voice":"piper/only"}})"));
    CHECK((one.piper_voices == std::vector<std::string>{"piper/only"}));

    const auto junk =
        audio_config_from_json(nlohmann::json::parse(R"({"audio":{"piper_voices":[1, ""]}})"));
    CHECK(junk.piper_voices.size() == 5); // nothing usable: the defaults
  }

  zoal_briefing::log::reset_sink();
  return test_result("voice_pool");
}
