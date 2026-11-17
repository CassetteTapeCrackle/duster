// Drives the shipped DusterProcessor: parameters, presets, MIDI timing and host-rate resampling.
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <algorithm>
#include <cmath>
#include <set>
#include <string>
#include <vector>

#include <juce_events/juce_events.h>

#include "Plugin/DusterParams.h"
#include "Plugin/DusterPresets.h"
#include "Plugin/DusterProcessor.h"
#include "TestSignals.h"

using namespace duster;
using Catch::Matchers::WithinAbs;

namespace {
struct Juce {
    juce::ScopedJuceInitialiser_GUI init;
};

constexpr int k05Program = 1;

struct HostNote {
    int atSample;
    bool on;
};

// Renders `seconds` of the left channel in host blocks, sending note 30 events at their samples.
std::vector<float> render(DusterProcessor& p, double rate, int block, double seconds,
                          const std::vector<HostNote>& notes, int preparedBlock = 0) {
    p.prepareToPlay(rate, preparedBlock > 0 ? preparedBlock : block);
    const auto total = static_cast<int>(rate * seconds);
    std::vector<float> out(static_cast<std::size_t>(total));
    juce::AudioBuffer<float> buffer(2, block);
    juce::MidiBuffer midi;
    for (int start = 0; start < total; start += block) {
        const int n = std::min(block, total - start);
        buffer.setSize(2, n, false, false, true);
        midi.clear();
        for (const auto& note : notes)
            if (note.atSample >= start && note.atSample < start + n)
                midi.addEvent(note.on ? juce::MidiMessage::noteOn(1, 30, 1.0f)
                                      : juce::MidiMessage::noteOff(1, 30),
                              note.atSample - start);
        p.processBlock(buffer, midi);
        for (int i = 0; i < n; ++i)
            out[static_cast<std::size_t>(start + i)] = buffer.getSample(0, i);
    }
    return out;
}

std::vector<float> render05(double rate, int block, double seconds, const std::vector<HostNote>& notes,
                            int preparedBlock = 0) {
    DusterProcessor p;
    p.setCurrentProgram(k05Program);
    return render(p, rate, block, seconds, notes, preparedBlock);
}

std::size_t onset(const std::vector<float>& x) {
    const auto it = std::find_if(x.begin(), x.end(), [](float v) { return std::abs(v) > 0.01f; });
    return static_cast<std::size_t>(it - x.begin());
}

float maxDifference(const std::vector<float>& a, const std::vector<float>& b, std::size_t shiftA = 0) {
    float worst = 0.0f;
    for (std::size_t i = 0; i + shiftA < a.size() && i < b.size(); ++i)
        worst = std::max(worst, std::abs(a[i + shiftA] - b[i]));
    return worst;
}
} // namespace

TEST_CASE("05 CRUSHED rings at 9600 - f0 at every host rate", "[plugin]") {
    Juce juce;
    for (const double rate : {44100.0, 48000.0, 88200.0, 96000.0}) {
        INFO("host rate " << rate);
        const auto out = render05(rate, 512, 0.5, {{0, true}});
        const auto from = static_cast<std::size_t>(0.10 * rate);
        const auto spec = test::spectrum(out, rate, from, static_cast<std::size_t>(0.26 * rate));
        const double f0 = spec.peakHz(30.0, 80.0);
        REQUIRE_THAT(f0, WithinAbs(46.9, 1.5));
        REQUIRE_THAT(spec.peakHz(8000.0, 11000.0), WithinAbs(9600.0 - f0, 3.0));
    }
}

TEST_CASE("the block size never changes the output", "[plugin]") {
    Juce juce;
    const auto reference = render05(44100.0, 512, 0.5, {{0, true}});
    for (const int block : {1, 7, 4096}) {
        INFO("block " << block);
        REQUIRE(maxDifference(render05(44100.0, block, 0.5, {{0, true}}), reference) < 1e-5f);
    }
    // Some hosts send more than they announced in prepareToPlay.
    REQUIRE(maxDifference(render05(44100.0, 512, 0.5, {{0, true}}, 64), reference) < 1e-5f);
}

TEST_CASE("two note-ons on the same sample play one hit", "[plugin]") {
    Juce juce;
    const auto once = render05(44100.0, 512, 0.3, {{100, true}});
    const auto twice = render05(44100.0, 512, 0.3, {{100, true}, {100, true}});
    REQUIRE(once == twice);
}

TEST_CASE("a note lands on its sample", "[plugin]") {
    Juce juce;
    const auto atZero = render05(44100.0, 512, 0.5, {{0, true}});
    const auto at300 = render05(44100.0, 512, 0.5, {{300, true}});
    const auto lag = static_cast<long>(onset(at300)) - static_cast<long>(onset(atZero));
    REQUIRE(std::abs(lag - 300) <= 1);

    const auto lastSample = render05(44100.0, 512, 0.5, {{511, true}});
    REQUIRE(onset(lastSample) < lastSample.size());
    REQUIRE(std::abs(static_cast<long>(onset(lastSample)) - static_cast<long>(onset(atZero)) - 511) <= 1);
}

TEST_CASE("note-off is ignored", "[plugin]") {
    Juce juce;
    const auto plain = render05(44100.0, 512, 0.5, {{0, true}});
    const auto withOff = render05(44100.0, 512, 0.5, {{0, true}, {2205, false}});
    REQUIRE(plain == withOff);
}

TEST_CASE("the reported latency is where the hit appears", "[plugin]") {
    Juce juce;
    DusterProcessor at48;
    at48.setCurrentProgram(k05Program);
    const auto out48 = render(at48, 48000.0, 512, 0.2, {{0, true}});
    REQUIRE(at48.getLatencySamples() == 0);

    for (const double rate : {22050.0, 44100.0, 88200.0, 192000.0}) {
        INFO("host rate " << rate);
        DusterProcessor host;
        host.setCurrentProgram(k05Program);
        const auto out = render(host, rate, 512, 0.2, {{0, true}});
        const double compensated = static_cast<double>(onset(out)) - host.getLatencySamples();
        // Within one engine sample (the onset detector's resolution), plus half a host sample.
        const double tolerance = rate / 48000.0 + 0.5;
        REQUIRE_THAT(compensated, WithinAbs(static_cast<double>(onset(out48)) * rate / 48000.0, tolerance));
    }
}

TEST_CASE("state round-trips every knob and the program", "[plugin]") {
    Juce juce;
    DusterProcessor source;
    source.setCurrentProgram(k05Program);
    for (auto* p : source.getParameters()) {
        auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(p);
        REQUIRE(ranged != nullptr);
        const float v = ranged->getDefaultValue() < 0.5f ? 0.8f : 0.2f;
        ranged->setValueNotifyingHost(v);
    }
    juce::MemoryBlock state;
    source.getStateInformation(state);

    DusterProcessor restored;
    restored.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
    REQUIRE(restored.getCurrentProgram() == k05Program);
    const auto& a = source.getParameters();
    const auto& b = restored.getParameters();
    REQUIRE(a.size() == b.size());
    for (int i = 0; i < a.size(); ++i)
        REQUIRE_THAT(b[i]->getValue(), WithinAbs(a[i]->getValue(), 1e-6));
}

TEST_CASE("factory presets have unique names and in-range values", "[presets]") {
    Juce juce;
    DusterProcessor p;
    std::set<std::string> names;
    for (const auto& preset : factoryPresets()) {
        INFO("preset " << preset.name);
        REQUIRE(names.insert(std::string(preset.name)).second);
        std::set<std::string> ids;
        for (const auto& [id, value] : preset.values) {
            INFO("param " << id);
            REQUIRE(ids.insert(std::string(id)).second);
            auto* param = p.parameters().getParameter(juce::String(std::string(id)));
            REQUIRE(param != nullptr);
            const auto& range = param->getNormalisableRange();
            REQUIRE(value >= range.start);
            REQUIRE(value <= range.end);
        }
        REQUIRE(static_cast<int>(ids.size()) == p.getParameters().size());
    }
    REQUIRE(factoryPresets()[0].name == "INIT");
    REQUIRE(factoryPresets()[k05Program].name == "05 CRUSHED");
}

TEST_CASE("INIT holds every parameter's default", "[presets]") {
    Juce juce;
    DusterProcessor p;
    for (const auto& [id, value] : factoryPresets()[0].values) {
        INFO("param " << id);
        auto* param = p.parameters().getParameter(juce::String(std::string(id)));
        REQUIRE_THAT(param->convertFrom0to1(param->getDefaultValue()), WithinAbs(value, 1e-4));
    }
}
