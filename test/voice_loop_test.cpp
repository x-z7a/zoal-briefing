#include "check.hpp"

#include "zoal_briefing/audio/voice_loop.hpp"
#include "zoal_briefing/log.hpp"

#include <cmath>
#include <string>
#include <thread>
#include <utility>
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
  std::string phrase() const override { return "hey jarvis"; }
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

// PENDING (with a stand-by line) until ready is set.
struct FakeResponder : Responder {
  bool ready = false;
  int calls = 0;
  std::string last_request;
  Reply reply(const std::string &request) override {
    ++calls;
    last_request = request;
    if (ready) return {Reply::Status::READY, "Departure briefing."};
    return {Reply::Status::PENDING, "Stand by."};
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

  explicit Rig(VoiceLoopConfig cfg = {}, Responder *responder = nullptr)
      : loop(std::move(cfg), wake, stt, tts, out, machine, responder) {}

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
      CHECK(states[0].rfind("IDLE -> WAKE_DETECTED: heard \"hey jarvis\" (score 0.90)", 0) == 0);
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

  // A Responder's answer replaces the echo.
  {
    FakeResponder responder;
    responder.ready = true;
    Rig r({}, &responder);
    r.loop.request_listen();
    r.loop.step();
    r.speak_request();
    CHECK(r.machine.current() == State::PLAYBACK);
    CHECK(responder.last_request == "departure briefing for runway two eight left");
    CHECK(!r.tts.spoken.empty() && r.tts.spoken.back() == "Departure briefing.");
  }

  // Not ready yet: "Stand by." once, wait (mic ignored), then the briefing.
  {
    FakeResponder responder;
    Rig r({}, &responder);
    r.loop.request_listen();
    r.loop.step();
    r.speak_request();
    CHECK(r.machine.current() == State::BRIEFING_GENERATING);
    CHECK(!r.tts.spoken.empty() && r.tts.spoken.back() == "Stand by.");

    // While "Stand by." plays, the responder is not asked again.
    const int calls = responder.calls;
    r.push(chunk_of(8000), 3);
    CHECK(responder.calls == calls);
    r.out.finish();
    r.loop.step();
    CHECK(responder.calls == calls + 1);
    CHECK(r.machine.current() == State::BRIEFING_GENERATING);
    CHECK(r.tts.spoken.size() == 1); // the stand-by is not repeated

    responder.ready = true;
    std::this_thread::sleep_for(std::chrono::milliseconds(250)); // past the poll interval
    r.loop.step();
    CHECK(r.machine.current() == State::PLAYBACK);
    CHECK(r.tts.spoken.back() == "Departure briefing.");
    r.out.finish();
    r.loop.step();
    CHECK(r.machine.current() == State::IDLE);
  }

  // Waiting gives up after max_wait_seconds.
  {
    VoiceLoopConfig cfg;
    cfg.max_wait_seconds = 0.0;
    FakeResponder responder;
    Rig r(cfg, &responder);
    r.loop.request_listen();
    r.loop.step();
    r.speak_request();
    CHECK(r.machine.current() == State::BRIEFING_GENERATING);
    r.out.finish();
    r.loop.step();
    CHECK(r.machine.current() == State::PLAYBACK);
    // A sentence at a time: the first sentence of gave_up.
    CHECK(r.tts.spoken.back() == "Sorry, captain, the briefing is not ready.");
  }

  // Cancel while waiting returns to idle.
  {
    FakeResponder responder;
    Rig r({}, &responder);
    r.loop.request_listen();
    r.loop.step();
    r.speak_request();
    r.loop.request_cancel();
    r.loop.step();
    CHECK(r.machine.current() == State::IDLE);
  }

  // Several sentences: one clip each, the next synthesized while one plays.
  {
    struct Fixed : Responder {
      Reply reply(const std::string &) override {
        return {Reply::Status::READY, "Runway two eight left. Wind calm.\n\nCleared for takeoff."};
      }
    } responder;
    Rig r({}, &responder);
    r.loop.request_listen();
    r.loop.step();
    r.speak_request();
    CHECK(r.machine.current() == State::PLAYBACK);
    // The first sentence plays; the next is already synthesized meanwhile.
    CHECK(r.out.played == 1 && r.tts.spoken.size() == 2);
    CHECK(r.tts.spoken[0] == "Runway two eight left." && r.tts.spoken[1] == "Wind calm.");
    CHECK(r.loop.speech_progress().current == "Runway two eight left.");
    r.out.finish();
    r.loop.step();
    CHECK(r.out.played == 2);
    CHECK(r.loop.speech_progress().spoken == "Runway two eight left.");
    CHECK(r.loop.speech_progress().current == "Wind calm.");
    for (int i = 0; i < 5 && r.machine.current() != State::IDLE; ++i) {
      r.out.finish();
      r.loop.step();
    }
    CHECK(r.out.played == 3 && r.tts.spoken.back() == "Cleared for takeoff.");
    CHECK(r.machine.current() == State::IDLE);
    CHECK(r.loop.speech_progress().current.empty());
  }

  // A STREAMING reply is spoken as it is written: whole sentences only, and
  // the unfinished tail waits for the rest.
  {
    struct Streaming : Responder {
      std::string text;
      bool done = false;
      Reply reply(const std::string &) override {
        if (text.empty()) return {Reply::Status::PENDING, ""};
        return {done ? Reply::Status::READY : Reply::Status::STREAMING, text};
      }
    } responder;
    VoiceLoopConfig cfg;
    Rig r(cfg, &responder);
    r.loop.request_listen();
    r.loop.step();
    r.speak_request();
    CHECK(r.machine.current() == State::BRIEFING_GENERATING); // nothing written yet
    CHECK(r.tts.spoken.empty());

    responder.text = "Runway two eight left. Wind two nine";
    std::this_thread::sleep_for(std::chrono::milliseconds(250));
    r.loop.step();
    CHECK(r.machine.current() == State::PLAYBACK);
    CHECK(r.tts.spoken.size() == 1 && r.tts.spoken[0] == "Runway two eight left.");

    r.out.finish();
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    r.loop.step(); // the tail is not a sentence yet: wait
    CHECK(r.tts.spoken.size() == 1 && r.machine.current() == State::PLAYBACK);

    responder.text = "Runway two eight left. Wind two nine zero at one four. Cleared";
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    r.loop.step();
    CHECK(r.tts.spoken.size() == 2 && r.tts.spoken[1] == "Wind two nine zero at one four.");

    responder.text += " for takeoff";
    responder.done = true;
    r.out.finish();
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    r.loop.step(); // complete: the tail is the last sentence
    CHECK(r.tts.spoken.size() == 3 && r.tts.spoken[2] == "Cleared for takeoff");
    r.out.finish();
    r.loop.step();
    CHECK(r.machine.current() == State::IDLE);
  }

  // A reply that stops continuing (the briefing failed) is said after.
  {
    struct Changing : Responder {
      int calls = 0;
      Reply reply(const std::string &) override {
        if (++calls == 1) return {Reply::Status::STREAMING, "Runway two eight left. Wind"};
        return {Reply::Status::READY, "Sorry, captain, it failed."};
      }
    } responder;
    Rig r({}, &responder);
    r.loop.request_listen();
    r.loop.step();
    r.speak_request();
    for (int i = 0; i < 10 && r.machine.current() != State::IDLE; ++i) {
      r.out.finish();
      std::this_thread::sleep_for(std::chrono::milliseconds(110));
      r.loop.step();
    }
    CHECK((r.tts.spoken == std::vector<std::string>{"Runway two eight left.", "Wind",
                                                    "Sorry, captain, it failed."}));
    CHECK(r.machine.current() == State::IDLE);
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
