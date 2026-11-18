// Drives the shipped DusterProcessor, not just the core.
#include <catch2/catch_test_macros.hpp>

#include <juce_events/juce_events.h>

#include "Plugin/DusterProcessor.h"

namespace {
struct Juce {
    juce::ScopedJuceInitialiser_GUI init;
};
} // namespace

TEST_CASE("plugin is a MIDI instrument with stereo out", "[plugin]") {
    Juce juce;
    DusterProcessor processor;
    REQUIRE(processor.acceptsMidi());
    REQUIRE(processor.getTotalNumOutputChannels() == 2);

    constexpr int kBlock = 512;
    processor.prepareToPlay(48000.0, kBlock);
    juce::AudioBuffer<float> buffer(2, kBlock);
    buffer.clear();
    juce::MidiBuffer midi;
    processor.processBlock(buffer, midi);

    // No note, no sound.
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < kBlock; ++i)
            REQUIRE(buffer.getSample(ch, i) == 0.0f);
}
