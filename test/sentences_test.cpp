#include "check.hpp"

#include "zoal_briefing/audio/tts/sentences.hpp"

#include <string>
#include <vector>

using namespace zoal_briefing::audio;

namespace {

std::vector<std::string> split(const std::string &text) {
  std::vector<std::string> out;
  std::size_t from = 0;
  for (std::size_t end = next_sentence_end(text, from); end != std::string::npos;
       end = next_sentence_end(text, from)) {
    out.push_back(trimmed(text, from, end));
    from = end;
  }
  return out;
}

} // namespace

int main() {
  // Terminators followed by whitespace end a sentence; paragraph breaks too.
  CHECK((split("Runway two eight left. Wind calm! Are we ready? ") ==
         std::vector<std::string>{"Runway two eight left.", "Wind calm!", "Are we ready?"}));
  CHECK((split("Fuel is fine\n\nNext part. ") ==
         std::vector<std::string>{"Fuel is fine", "Next part."}));
  // A terminator at the very end may be followed by more: not yet a sentence.
  CHECK(split("Runway two eight left.").empty());
  // Decimals are not ends.
  CHECK((split("Fuel three point 7.5 tonnes. Done now. ") ==
         std::vector<std::string>{"Fuel three point 7.5 tonnes.", "Done now."}));
  // One word is not a sentence of its own: it joins the next.
  CHECK((split("1. Climb to flight level three three zero. ") ==
         std::vector<std::string>{"1. Climb to flight level three three zero."}));
  CHECK((split("Wind\n\nNext part. ") == std::vector<std::string>{"Wind", "Next part."}));
  CHECK(trimmed("  a b  ", 0, 7) == "a b");
  return test_result("sentences");
}
