#include "check.hpp"

#include "zoal_briefing/audio_pipeline/endpointer.hpp"

#include <cmath>
#include <random>
#include <vector>

using zoal_briefing::audio::Endpointer;
using zoal_briefing::audio::EndpointerConfig;

namespace {

constexpr std::size_t kChunk = 1280; // 80 ms

std::vector<std::int16_t> noise(std::mt19937 &rng, int amplitude) {
  std::uniform_int_distribution<int> d(-amplitude, amplitude);
  std::vector<std::int16_t> v(kChunk);
  for (auto &s : v) s = static_cast<std::int16_t>(d(rng));
  return v;
}

std::vector<std::int16_t> tone(double amplitude) {
  std::vector<std::int16_t> v(kChunk);
  for (std::size_t i = 0; i < kChunk; ++i) {
    v[i] = static_cast<std::int16_t>(amplitude * std::sin(2.0 * 3.14159265 * 300.0 * static_cast<double>(i) / 16000.0));
  }
  return v;
}

// Feeds `chunks` copies and returns the last status.
Endpointer::Status feed(Endpointer &ep, const std::vector<std::int16_t> &chunk, int chunks) {
  Endpointer::Status s = Endpointer::Status::WAITING;
  for (int i = 0; i < chunks; ++i) s = ep.feed(chunk.data(), chunk.size());
  return s;
}

} // namespace

int main() {
  std::mt19937 rng(7);

  // Quiet room, then speech, then silence: ends after end_silence_ms.
  {
    Endpointer ep;
    CHECK(feed(ep, noise(rng, 100), 5) == Endpointer::Status::WAITING);
    CHECK(feed(ep, tone(6000), 15) == Endpointer::Status::SPEAKING);
    CHECK(ep.heard_speech());
    CHECK(feed(ep, noise(rng, 100), 5) == Endpointer::Status::SPEAKING); // 400 ms: not yet
    CHECK(feed(ep, noise(rng, 100), 6) == Endpointer::Status::DONE);     // 880 ms
    // Stays done.
    CHECK(feed(ep, tone(6000), 1) == Endpointer::Status::DONE);
  }

  // Nobody talks: gives up after the no-speech timeout.
  {
    EndpointerConfig cfg;
    cfg.no_speech_timeout_seconds = 2.0;
    Endpointer ep(cfg);
    CHECK(feed(ep, noise(rng, 100), 24) == Endpointer::Status::WAITING); // 1.92 s
    CHECK(feed(ep, noise(rng, 100), 1) == Endpointer::Status::NO_SPEECH);
    CHECK(!ep.heard_speech());
  }

  // A single click does not start speech.
  {
    Endpointer ep;
    feed(ep, noise(rng, 100), 5);
    CHECK(feed(ep, tone(8000), 1) == Endpointer::Status::WAITING);
    CHECK(feed(ep, noise(rng, 100), 1) == Endpointer::Status::WAITING);
  }

  // Talking without pause is cut at max_seconds.
  {
    EndpointerConfig cfg;
    cfg.max_seconds = 3.0;
    Endpointer ep(cfg);
    CHECK(feed(ep, tone(6000), 40) == Endpointer::Status::MAX_LENGTH);
  }

  // A loud cockpit: with the ambient level measured beforehand, its noise
  // alone is not speech, but talking over it is.
  {
    double ambient = 0.0;
    for (int i = 0; i < 20; ++i) {
      auto n = noise(rng, 2000);
      ambient = Endpointer::track_floor(ambient, Endpointer::rms(n.data(), n.size()));
    }
    Endpointer ep;
    ep.reset(ambient);
    CHECK(feed(ep, noise(rng, 2000), 30) == Endpointer::Status::WAITING);
    CHECK(feed(ep, tone(12000), 4) == Endpointer::Status::SPEAKING);
  }

  // The pilot starts talking the moment recording starts: still speech.
  {
    Endpointer ep;
    CHECK(feed(ep, tone(6000), 10) == Endpointer::Status::SPEAKING);
    CHECK(feed(ep, noise(rng, 100), 11) == Endpointer::Status::DONE);
  }

  // reset() starts over.
  {
    Endpointer ep;
    feed(ep, tone(6000), 5);
    ep.reset();
    CHECK(!ep.heard_speech());
    CHECK(ep.elapsed_seconds() == 0.0);
  }

  return test_result("endpointer");
}
