#include "zoal_briefing/audio/voice_loop.hpp"

#include "zoal_briefing/audio/tts/sentences.hpp"
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

std::chrono::steady_clock::duration seconds(double s) {
  return std::chrono::duration_cast<std::chrono::steady_clock::duration>(
      std::chrono::duration<double>(s));
}

bool starts_with(const std::string &s, const std::string &prefix) {
  return s.size() >= prefix.size() && s.compare(0, prefix.size(), prefix) == 0;
}

// A whole sentence is there to speak.
bool has_sentence(const std::string &text) {
  return next_sentence_end(text, 0) != std::string::npos;
}

} // namespace

VoiceLoop::VoiceLoop(VoiceLoopConfig config, WakeDetector &wake, Transcriber &stt,
                     Synthesizer &tts, AudioOutput &output, state::StateMachine &machine,
                     Responder *responder)
    : cfg_(std::move(config)), wake_(wake), stt_(stt), tts_(tts), output_(output),
      machine_(machine), responder_(responder), endpointer_(cfg_.endpointer) {}

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

SpeechProgress VoiceLoop::speech_progress() const {
  std::lock_guard<std::mutex> lock(progress_mutex_);
  return progress_;
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

  if (state == State::BRIEFING_GENERATING) {
    on_waiting();
    return;
  }

  // Our own speech: the microphone hears us, not the pilot.
  if (state == State::WAKE_DETECTED) {
    ring_.discard();
    if (pump() == Speech::DONE) begin_listening("acknowledged; recording request");
    return;
  }
  if (state == State::PLAYBACK) {
    ring_.discard();
    on_speaking();
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

  say(cfg_.acknowledgement, true);
  if (pump() == Speech::DONE) begin_listening("acknowledgement unavailable; recording request");
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

  if (!text || text->empty()) {
    speak_reply(cfg_.say_again, "asking to repeat");
    return;
  }
  if (!responder_) {
    speak_reply(*text, "echoing transcript");
    return;
  }

  pending_request_ = *text;
  reset_wait();
  const Reply reply = responder_->reply(*text);
  if (reply.status == Reply::Status::READY) {
    pending_request_.clear();
    speak_reply(reply.text, "speaking the reply");
    return;
  }
  if (reply.status == Reply::Status::STREAMING && has_sentence(reply.text)) {
    begin_streaming(reply.text, "speaking the reply as it is written");
    return;
  }
  if (!machine_.transition_to(State::BRIEFING_GENERATING, "reply not ready; waiting")) return;
  if (reply.status == Reply::Status::PENDING && !reply.text.empty()) {
    say(reply.text, true); // "stand by"
    pump();
  }
}

void VoiceLoop::reset_wait() {
  const auto now = std::chrono::steady_clock::now();
  wait_deadline_ = now + seconds(cfg_.max_wait_seconds);
  next_poll_ = now;
}

// BRIEFING_GENERATING: the "stand by" plays out, then we ask again until
// there is something to say.
void VoiceLoop::on_waiting() {
  ring_.discard();
  if (pump() == Speech::PLAYING) return;
  const auto now = std::chrono::steady_clock::now();
  if (now < next_poll_) return;
  next_poll_ = now + std::chrono::milliseconds(200);

  const Reply reply = responder_ ? responder_->reply(pending_request_) : Reply{};
  if (reply.status == Reply::Status::READY) {
    pending_request_.clear();
    speak_reply(reply.text, "reply ready");
  } else if (reply.status == Reply::Status::STREAMING && has_sentence(reply.text)) {
    begin_streaming(reply.text, "reply being written; speaking it");
  } else if (now >= wait_deadline_) {
    pending_request_.clear();
    speak_reply(cfg_.gave_up, "gave up waiting after " + fmt_seconds(cfg_.max_wait_seconds));
  }
}

// PLAYBACK: sentences play out; a STREAMING reply is asked for its new text.
void VoiceLoop::on_speaking() {
  const auto now = std::chrono::steady_clock::now();
  if (!utterance_.complete && responder_ && now >= next_poll_) {
    next_poll_ = now + std::chrono::milliseconds(100);
    const Reply reply = responder_->reply(pending_request_);
    if (reply.status != Reply::Status::PENDING) {
      if (continue_saying(reply.text, reply.status == Reply::Status::READY)) {
        wait_deadline_ = now + seconds(cfg_.max_wait_seconds);
      }
      if (utterance_.complete) pending_request_.clear();
    }
  }

  const Speech speech = pump();
  if (speech == Speech::DONE) {
    back_to_idle("playback finished");
  } else if (speech == Speech::WAITING_FOR_TEXT && now >= wait_deadline_) {
    log::warn(kLog, "the reply stopped growing for " + fmt_seconds(cfg_.max_wait_seconds));
    pending_request_.clear();
    utterance_.text += "\n\n" + cfg_.gave_up;
    utterance_.complete = true;
    split_sentences();
  }
}

void VoiceLoop::speak_reply(const std::string &text, const std::string &reason) {
  if (!machine_.transition_to(State::PLAYBACK, reason)) return;
  say(text, true);
  if (pump() == Speech::DONE) back_to_idle("nothing to play");
}

void VoiceLoop::begin_streaming(const std::string &text, const std::string &reason) {
  if (!machine_.transition_to(State::PLAYBACK, reason)) return;
  say(text, false);
  wait_deadline_ = std::chrono::steady_clock::now() + seconds(cfg_.max_wait_seconds);
  pump();
}

// --- speech, a sentence at a time ----------------------------------------------

void VoiceLoop::say(const std::string &text, bool complete) {
  tts_.begin_utterance(); // a voice change waits for this, never mid-reply
  utterance_ = Utterance{};
  utterance_.text = text;
  utterance_.complete = complete;
  split_sentences();
  std::lock_guard<std::mutex> lock(progress_mutex_);
  progress_ = SpeechProgress{};
}

bool VoiceLoop::continue_saying(const std::string &text, bool complete) {
  Utterance &u = utterance_;
  const std::string current = u.text.substr(u.reply_offset);
  bool grew = false;
  if (starts_with(text, current)) {
    grew = text.size() > current.size();
    u.text.resize(u.reply_offset);
    u.text += text;
  } else if (!starts_with(current, text)) {
    // Not a continuation (the briefing failed, or was replaced): say it next.
    log::info(kLog, "the reply changed while speaking; saying the new text after it");
    u.text += "\n\n";
    u.reply_offset = u.text.size();
    u.text += text;
    grew = !text.empty();
  }
  u.complete = complete;
  split_sentences();
  return grew;
}

void VoiceLoop::split_sentences() {
  Utterance &u = utterance_;
  std::size_t end = 0;
  while ((end = next_sentence_end(u.text, u.split)) != std::string::npos) {
    std::string sentence = trimmed(u.text, u.split, end);
    if (!sentence.empty()) u.sentences.push_back(std::move(sentence));
    u.split = end;
  }
  if (u.complete && u.split < u.text.size()) {
    std::string rest = trimmed(u.text, u.split, u.text.size());
    if (!rest.empty()) u.sentences.push_back(std::move(rest));
    u.split = u.text.size();
  }
}

VoiceLoop::Speech VoiceLoop::pump() {
  if (clip_playing()) {
    if (!utterance_.next_clip) synthesize_next(); // ready for when this one ends
    return Speech::PLAYING;
  }
  if (finished_token_->load() != playing_token_ && stuck_token_ != playing_token_) {
    stuck_token_ = playing_token_;
    log::warn(kLog, "playback did not report completion; moving on");
  }
  if (utterance_.next_clip || synthesize_next()) {
    play_next();
    return Speech::PLAYING;
  }
  return utterance_.complete ? Speech::DONE : Speech::WAITING_FOR_TEXT;
}

bool VoiceLoop::synthesize_next() {
  Utterance &u = utterance_;
  while (!u.sentences.empty()) {
    std::string sentence = std::move(u.sentences.front());
    u.sentences.pop_front();
    const auto started = std::chrono::steady_clock::now();
    auto clip = tts_.synthesize(sentence);
    if (!clip || clip->samples.empty()) {
      log::error(kLog, "speech synthesis failed for \"" + sentence + "\"");
      continue;
    }
    log::info(kLog, "speaking \"" + sentence + "\" (" + fmt_seconds(clip->seconds()) +
                        ", synthesized in " + fmt_seconds(seconds_since(started)) + ")");
    u.next_clip = std::move(clip);
    u.next_sentence = std::move(sentence);
    return true;
  }
  return false;
}

void VoiceLoop::play_next() {
  Utterance &u = utterance_;
  if (!u.next_clip) return;
  PcmClip clip = std::move(*u.next_clip);
  u.next_clip.reset();
  playback_deadline_ =
      std::chrono::steady_clock::now() + seconds(clip.seconds() + cfg_.playback_grace_seconds);
  {
    std::lock_guard<std::mutex> lock(progress_mutex_);
    if (!progress_.current.empty()) {
      if (!progress_.spoken.empty()) progress_.spoken += ' ';
      progress_.spoken += progress_.current;
    }
    progress_.current = std::move(u.next_sentence);
  }
  u.next_sentence.clear();
  const std::uint64_t token = ++playing_token_;
  auto finished = finished_token_;
  output_.play(std::move(clip), [finished, token] { finished->store(token); });
}

bool VoiceLoop::clip_playing() const {
  return finished_token_->load() != playing_token_ &&
         std::chrono::steady_clock::now() < playback_deadline_;
}

void VoiceLoop::back_to_idle(const std::string &reason) {
  ring_.discard();
  pending_request_.clear();
  recording_.clear();
  utterance_ = Utterance{};
  {
    std::lock_guard<std::mutex> lock(progress_mutex_);
    progress_ = SpeechProgress{};
  }
  wake_.reset();
  machine_.reset(reason);
}

} // namespace zoal_briefing::audio
