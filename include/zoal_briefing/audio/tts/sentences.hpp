#ifndef ZOAL_BRIEFING_AUDIO_TTS_SENTENCES_HPP
#define ZOAL_BRIEFING_AUDIO_TTS_SENTENCES_HPP

#include <cstddef>
#include <string>

namespace zoal_briefing::audio {

// Where the next whole sentence in text, starting at from, ends: just past its
// terminator (. ! ? or a paragraph break) and the whitespace after it, so the
// next sentence starts there. npos when text from `from` holds no complete
// sentence yet, which is how a reply still being streamed is read: the
// unfinished tail waits for more text.
//
// A terminator only counts when whitespace follows it (so "3.5" and a "." at
// the very end of a partial text do not), and the sentence has at least two
// words (so "1." or "Captain." alone are not spoken as their own clip). A
// paragraph break always ends one.
std::size_t next_sentence_end(const std::string &text, std::size_t from);

// text[from, to) without leading and trailing whitespace.
std::string trimmed(const std::string &text, std::size_t from, std::size_t to);

} // namespace zoal_briefing::audio

#endif // ZOAL_BRIEFING_AUDIO_TTS_SENTENCES_HPP
