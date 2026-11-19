#include "check.hpp"

#include "zoal_briefing/audio_pipeline/voice_loop.hpp"
#include "zoal_briefing/log.hpp"

#include <cmath>
#include <string>
#include <thread>
#include <vector>

using namespace zoal_briefing::audio;
using zoal_briefing::state::State;
using zoal_briefing::state::StateMachine;

namespace {

constexpr std::int16_t kWakeMarker = 7777;

// Fires when a chunk starts with the marker sample.
struct FakeWake : WakeDetector {
  int resets = 0;
  std::optional<float> feed(const std::int16_t *pcm, std::size_t) override {
    if (pcm[0] == kWakeMarker) return 0.9f;
    return std::nullopt;
  }
  void reset() override { ++resets; }
  std::string phrase() const override { return "cookie monster"; }
};

struct FakeStt : Transcriber {
  std::optional<std::string> reply = std::string("departure briefing for runway two eight left");
  std::size_t last_samples = 0;
  std::optional<std::string> transcribe(const std::vector<std::int16_t> &pcm) override {
    last_samples = pcm.size();
    return reply;
  }
};

struct FakeTts : Synthesizer {
  std::vector<std::string> spoken;
  double seconds = 0.5;
  std::optional<PcmClip> synthesize(const std::string &text) override {
    spoken.push_back(text);
    PcmClip clip;
    clip.sample_rate = 16000;
    clip.samples.assign(static_cast<std::size_t>(seconds * 16000), 100);
    return clip;
  }
};

struct FakeOutput : AudioOutput {
  std::function<void()> pending;
  int played = 0;
  int stops = 0;
  void play(PcmClip, std::function<void()> on_done) override {
    ++played;
    pending = std::move(on_done);
  }
  void stop() override {
    ++stops;
    finish();
  }
  void finish() {
    if (pending) {
      auto cb = std::move(pending);
      pending = nullptr;
      cb();
    }
  }
};

std::vector<std::int16_t> chunk_of(double amplitude) {
  std::vector<std::int16_t> v(kChunkSamples);
  for (std::size_t i = 0; i < v.size(); ++i) {
    v[i] = static_cast<std::int16_t>(amplitude * std::sin(0.12 * static_cast<double>(i)));
  }
  return v;
}

struct Rig {
  FakeWake wake;
  FakeStt stt;
  FakeTts tts;
  FakeOutput out;
  StateMachine machine;
  VoiceLoop loop;

  explicit Rig(VoiceLoopConfig cfg = {}) : loop(cfg, wake, stt, tts, out, machine) {}

  void push(const std::vector<std::int16_t> &chunk, int times = 1) {
    for (int i = 0; i < times; ++i) {
      loop.push_audio(chunk.data(), chunk.size());
      loop.step();
    }
  }
  void wake_word() {
    auto c = chunk_of(0);
    c[0] = kWakeMarker;
    push(c);
  }
  void speak_request() {
    push(chunk_of(50), 5);    // room
    push(chunk_of(8000), 15); // 1.2 s of "speech"
    push(chunk_of(50), 12);   // ~1 s of quiet ends it
  }
};

} // namespace

int main() {
  std::vector<std::string> log_lines;
  zoal_briefing::log::set_sink([&](const std::string &l) { log_lines.push_back(l); });

  // The whole echo cycle.
  {
    Rig r;
    r.push(chunk_of(50), 3);
    CHECK(r.machine.current() == State::IDLE);

    r.wake_word();
    CHECK(r.machine.current() == State::WAKE_DETECTED);
    CHECK(r.tts.spoken.size() == 1 && r.tts.spoken[0] == "Yes, captain.");
    CHECK(r.out.played == 1);

    // While acknowledging, the mic is ignored (it hears us).
    r.push(chunk_of(8000), 5);
    CHECK(r.machine.current() == State::WAKE_DETECTED);

    r.out.finish();
    r.loop.step();
    CHECK(r.machine.current() == State::LISTENING);

    r.speak_request();
    CHECK(r.machine.current() == State::PLAYBACK);
    CHECK(r.stt.last_samples >= 20 * kChunkSamples);
    CHECK(r.tts.spoken.size() == 2 &&
          r.tts.spoken[1] == "departure briefing for runway two eight left");
    CHECK(r.loop.last_transcript() == "departure briefing for runway two eight left");

    const int resets_before = r.wake.resets;
    r.out.finish();
    r.loop.step();
    CHECK(r.machine.current() == State::IDLE);
    CHECK(r.wake.resets == resets_before + 1);

    // The transitions were logged in order.
    std::vector<std::string> states;
    for (const auto &l : log_lines) {
      if (l.find("[state] ") != std::string::npos) states.push_back(l.substr(l.find("[state] ") + 8));
    }
    CHECK(states.size() == 5);
    if (states.size() == 5) {
      CHECK(states[0].rfind("IDLE -> WAKE_DETECTED: heard \"cookie monster\" (score 0.90)", 0) == 0);
      CHECK(states[1].rfind("WAKE_DETECTED -> LISTENING", 0) == 0);
      CHECK(states[2].rfind("LISTENING -> STT_COMPLETE: \"departure briefing", 0) == 0);
      CHECK(states[3].rfind("STT_COMPLETE -> PLAYBACK", 0) == 0);
      CHECK(states[4].rfind("PLAYBACK -> IDLE: playback finished", 0) == 0);
    }
  }

  // Listen command, nobody speaks: back to idle without transcribing.
  {
    Rig r;
    r.loop.request_listen();
    r.loop.step();
    CHECK(r.machine.current() == State::LISTENING);
    r.push(chunk_of(50), 51); // > 4 s
    CHECK(r.machine.current() == State::IDLE);
    CHECK(r.stt.last_samples == 0);
    CHECK(r.tts.spoken.empty());
  }

  // Nothing recognized: "Say again."
  {
    Rig r;
    r.stt.reply = std::string();
    r.loop.request_listen();
    r.loop.step();
    r.speak_request();
    CHECK(r.machine.current() == State::PLAYBACK);
    CHECK(!r.tts.spoken.empty() && r.tts.spoken.back() == "Say again.");
  }

  // Playback that never reports completion does not wedge the loop.
  {
    VoiceLoopConfig cfg;
    cfg.playback_grace_seconds = 0.0;
    Rig r(cfg);
    r.tts.seconds = 0.02;
    r.wake_word();
    CHECK(r.machine.current() == State::WAKE_DETECTED);
    std::this_thread::sleep_for(std::chrono::milliseconds(60));
    r.loop.step();
    CHECK(r.machine.current() == State::LISTENING);
  }

  // Cancel stops speech and returns to idle.
  {
    Rig r;
    r.wake_word();
    r.loop.request_cancel();
    r.loop.step();
    CHECK(r.machine.current() == State::IDLE);
    CHECK(r.out.stops == 1);
  }

  // The worker thread drives the same loop.
  {
    Rig r;
    r.loop.start();
    auto c = chunk_of(0);
    c[0] = kWakeMarker;
    r.loop.push_audio(c.data(), c.size());
    for (int i = 0; i < 100 && r.machine.current() != State::WAKE_DETECTED; ++i) {
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    CHECK(r.machine.current() == State::WAKE_DETECTED);
    r.loop.stop();
    CHECK(r.machine.current() == State::IDLE);
  }

  zoal_briefing::log::reset_sink();
  return test_result("voice_loop");
}
