#include "zoal_briefing/audio/tts/sentences.hpp"

#include <cctype>

namespace zoal_briefing::audio {

namespace {

bool space(char c) { return std::isspace(static_cast<unsigned char>(c)) != 0; }

int words(const std::string &text, std::size_t from, std::size_t to) {
  int n = 0;
  bool in_word = false;
  for (std::size_t i = from; i < to; ++i) {
    const bool s = space(text[i]);
    if (!s && !in_word) ++n;
    in_word = !s;
  }
  return n;
}

} // namespace

std::size_t next_sentence_end(const std::string &text, std::size_t from) {
  for (std::size_t i = from; i < text.size(); ++i) {
    const char c = text[i];
    const bool terminator = c == '.' || c == '!' || c == '?';
    const bool paragraph = c == '\n' && i + 1 < text.size() && text[i + 1] == '\n';
    if (!terminator && !paragraph) continue;
    // Followed by whitespace; at the end of the text we cannot know yet.
    if (terminator && (i + 1 >= text.size() || !space(text[i + 1]))) continue;
    if (terminator && words(text, from, i + 1) < 2) continue;
    if (words(text, from, i + 1) == 0) continue; // blank lines before any text
    std::size_t end = i + 1;
    while (end < text.size() && space(text[end])) ++end;
    // Trailing whitespace at the end of a partial text: more may join it, but
    // the sentence itself is complete.
    return end;
  }
  return std::string::npos;
}

std::string trimmed(const std::string &text, std::size_t from, std::size_t to) {
  if (to > text.size()) to = text.size();
  while (from < to && space(text[from])) ++from;
  while (to > from && space(text[to - 1])) --to;
  return text.substr(from, to - from);
}

} // namespace zoal_briefing::audio
