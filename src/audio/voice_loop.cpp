#include "zoal_briefing/audio/voice_loop.hpp"

#include "zoal_briefing/log.hpp"

#include <array>
#include <cstdio>

namespace zoal_briefing::audio {

using state::State;

namespace {

constexpr const char *kLog = "voice";

std::string fmt_seconds(double s) {
  char buf[32];
  std::snprintf(buf, sizeof(buf), "%.1f s", s);
  return buf;
}

double seconds_since(std::chrono::steady_clock::time_point t) {
  return std::chrono::duration<double>(std::chrono::steady_clock::now() - t).count();
}

} // namespace

VoiceLoop::VoiceLoop(VoiceLoopConfig config, WakeDetector &wake, Transcriber &stt,
                     Synthesizer &tts, AudioOutput &output, state::StateMachine &machine)
    : cfg_(std::move(config)), wake_(wake), stt_(stt), tts_(tts), output_(output),
      machine_(machine), endpointer_(cfg_.endpointer) {}

VoiceLoop::~VoiceLoop() { stop(); }

void VoiceLoop::start() {
  if (running_.exchange(true)) return;
  log::info(kLog, "listening for \"" + wake_.phrase() + "\"");
  worker_ = std::thread([this] { run(); });
}

void VoiceLoop::stop() {
  if (!running_.exchange(false)) return;
  if (worker_.joinable()) worker_.join();
  output_.stop();
  machine_.reset("voice loop stopped");
}

void VoiceLoop::push_audio(const std::int16_t *pcm, std::size_t count) { ring_.push(pcm, count); }

void VoiceLoop::request_listen() { listen_requested_ = true; }

void VoiceLoop::request_cancel() { cancel_requested_ = true; }

std::string VoiceLoop::last_transcript() const {
  std::lock_guard<std::mutex> lock(transcript_mutex_);
  return last_transcript_;
}

void VoiceLoop::run() {
  while (running_.load()) {
    const bool had_audio = ring_.available() >= kChunkSamples;
    step();
    if (!had_audio) std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
}

void VoiceLoop::step() {
  if (cancel_requested_.exchange(false)) {
    output_.stop();
    back_to_idle("cancelled");
    return;
  }

  const State state = machine_.current();

  if (listen_requested_.exchange(false)) {
    if (state == State::IDLE) {
      begin_listening("listen command (no wake word)");
      return;
    }
    log::info(kLog, std::string("listen command ignored while ") + state::to_string(state));
  }

  // Waiting on our own speech: the acknowledgement or the echo.
  if (state == State::WAKE_DETECTED || state == State::PLAYBACK) {
    const bool finished = finished_token_->load() == playing_token_;
    const bool stuck = std::chrono::steady_clock::now() >= playback_deadline_;
    if (finished || stuck) {
      if (!finished) log::warn(kLog, "playback did not report completion; moving on");
      if (state == State::WAKE_DETECTED) {
        begin_listening("acknowledged; recording request");
      } else {
        back_to_idle("playback finished");
      }
      return;
    }
    ring_.discard(); // the microphone is hearing us, not the pilot
    return;
  }

  std::array<std::int16_t, kChunkSamples> chunk{};
  if (!ring_.pop(chunk.data(), chunk.size())) return;

  if (state == State::IDLE) {
    on_idle_chunk(chunk.data());
  } else if (state == State::LISTENING) {
    on_listening_chunk(chunk.data());
  }
  // Other states belong to later components; audio is dropped meanwhile.
}

void VoiceLoop::on_idle_chunk(const std::int16_t *chunk) {
  ambient_rms_ = Endpointer::track_floor(ambient_rms_, Endpointer::rms(chunk, kChunkSamples));
  const auto score = wake_.feed(chunk, kChunkSamples);
  if (!score) return;

  char detail[96];
  std::snprintf(detail, sizeof(detail), "heard \"%s\" (score %.2f)", wake_.phrase().c_str(),
                static_cast<double>(*score));
  if (!machine_.transition_to(State::WAKE_DETECTED, detail)) return;

  if (!speak(cfg_.acknowledgement)) {
    begin_listening("acknowledgement unavailable; recording request");
  }
}

void VoiceLoop::begin_listening(const std::string &reason) {
  ring_.discard();
  recording_.clear();
  endpointer_.reset(ambient_rms_);
  machine_.transition_to(State::LISTENING, reason);
}

void VoiceLoop::on_listening_chunk(const std::int16_t *chunk) {
  recording_.insert(recording_.end(), chunk, chunk + kChunkSamples);
  const auto status = endpointer_.feed(chunk, kChunkSamples);
  if (status == Endpointer::Status::WAITING || status == Endpointer::Status::SPEAKING) return;
  finish_listening(status);
}

void VoiceLoop::finish_listening(Endpointer::Status status) {
  const double recorded = static_cast<double>(recording_.size()) / kCaptureSampleRate;
  if (status == Endpointer::Status::NO_SPEECH) {
    back_to_idle("no speech within " + fmt_seconds(recorded));
    return;
  }
  log::info(kLog, "recorded " + fmt_seconds(recorded) + " (" + to_string(status) + ")");

  const auto started = std::chrono::steady_clock::now();
  const auto text = stt_.transcribe(recording_);
  const std::string took = fmt_seconds(seconds_since(started));
  recording_.clear();

  if (!text) {
    machine_.transition_to(State::STT_COMPLETE, "transcription failed (" + took + ")");
  } else if (text->empty()) {
    machine_.transition_to(State::STT_COMPLETE, "no words recognized (" + took + ")");
  } else {
    {
      std::lock_guard<std::mutex> lock(transcript_mutex_);
      last_transcript_ = *text;
    }
    machine_.transition_to(State::STT_COMPLETE, "\"" + *text + "\" (" + took + ")");
  }

  const std::string reply = (text && !text->empty()) ? *text : cfg_.say_again;
  if (!machine_.transition_to(State::PLAYBACK, "echoing transcript")) return;
  if (!speak(reply)) back_to_idle("nothing to play");
}

bool VoiceLoop::speak(const std::string &text) {
  const auto started = std::chrono::steady_clock::now();
  auto clip = tts_.synthesize(text);
  if (!clip || clip->samples.empty()) {
    log::error(kLog, "speech synthesis failed for \"" + text + "\"");
    return false;
  }
  log::info(kLog, "speaking \"" + text + "\" (" + fmt_seconds(clip->seconds()) +
                      ", synthesized in " + fmt_seconds(seconds_since(started)) + ")");

  playback_deadline_ =
      std::chrono::steady_clock::now() +
      std::chrono::duration_cast<std::chrono::steady_clock::duration>(
          std::chrono::duration<double>(clip->seconds() + cfg_.playback_grace_seconds));
  const std::uint64_t token = ++playing_token_;
  auto finished = finished_token_;
  output_.play(std::move(*clip), [finished, token] { finished->store(token); });
  return true;
}

void VoiceLoop::back_to_idle(const std::string &reason) {
  ring_.discard();
  recording_.clear();
  wake_.reset();
  machine_.reset(reason);
}

} // namespace zoal_briefing::audio
