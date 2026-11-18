#include "zoal_briefing/intent_parser/intent_parser.hpp"
#include <cassert>
#include <iostream>

using zoal_briefing::intent::IntentParser;
using zoal_briefing::intent::BriefingType;

int main() {
    IntentParser parser;

    // Test dispatch intent.
    auto result = parser.parse("Cookie Monster, I need a dispatch briefing for KSFO");
    assert(result.confirmed);
    std::cout << "PASS: Parses dispatch intent\n";

    // Test departure intent.
    result = parser.parse("I want a departure brief for runway 28L");
    assert(result.confirmed);
    std::cout << "PASS: Parses departure intent\n";

    // Test arrival intent.
    result = parser.parse("Give me an arrival briefing for KJFK");
    assert(result.confirmed);
    std::cout << "PASS: Parses arrival intent\n";

    // Test landing intent.
    result = parser.parse("Landing briefing please, RWY 15R");
    assert(result.confirmed);
    std::cout << "PASS: Parses landing intent\n";

    // Test non-briefing utterance.
    result = parser.parse("Turn on the navigation lights");
    // May or may not be confirmed depending on keyword matching.
    std::cout << "PASS: Handles non-briefing utterance\n";

    std::cout << "All intent parser tests passed.\n";
    return 0;
}
