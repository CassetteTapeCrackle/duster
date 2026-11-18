#include "Plugin/DusterParams.h"

#include <cmath>
#include <cstdio>

#include "Engine/EngineRate.h"

namespace duster::params {

namespace {
using Float = juce::AudioParameterFloat;
using Attr = juce::AudioParameterFloatAttributes;

juce::NormalisableRange<float> skewed(float min, float max, float interval, float centre) {
    juce::NormalisableRange<float> range(min, max, interval);
    range.setSkewForCentre(centre);
    return range;
}

void addFloat(juce::AudioProcessorValueTreeState::ParameterLayout& layout, const char* id, const char* name,
              juce::NormalisableRange<float> range, float defaultValue, const char* label) {
    layout.add(std::make_unique<Float>(juce::ParameterID{id, 1}, name, range, defaultValue,
                                       Attr().withLabel(label)));
}

// "OFF", then "÷2 24k" to "÷32 1.5k".
juce::StringArray rateNames() {
    juce::StringArray names{"OFF"};
    for (int factor = 2; factor <= kRateItems; ++factor) {
        char rate[16];
        std::snprintf(rate, sizeof(rate), "%.3g", kEngineRate / 1000.0 / factor);
        names.add(juce::String(juce::CharPointer_UTF8("\xc3\xb7")) + juce::String(factor) + " " +
                  juce::String(rate) + "k");
    }
    return names;
}

int rounded(const std::atomic<float>* value) noexcept {
    return static_cast<int>(std::lround(value->load()));
}
} // namespace

juce::AudioProcessorValueTreeState::ParameterLayout createLayout() {
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    addFloat(layout, kTune, "Tune", {-12.0f, 12.0f, 0.01f}, 0.0f, "st");
    addFloat(layout, kSweep, "Sweep", {0.0f, 60.0f, 0.1f}, 36.0f, "st");
    addFloat(layout, kSnap, "Snap", skewed(1.0f, 40.0f, 0.1f, 8.0f), 8.0f, "ms");
    addFloat(layout, kLength, "Length", skewed(80.0f, 1500.0f, 1.0f, 400.0f), 400.0f, "ms");
    addFloat(layout, kDrive, "Drive", {0.0f, 100.0f, 0.1f}, 0.0f, "%");
    layout.add(std::make_unique<juce::AudioParameterInt>(juce::ParameterID{kBits, 1}, "Bits", 2, 16, 16));
    layout.add(
        std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{kRate, 1}, "Rate", rateNames(), 0));
    addFloat(layout, kTone, "Tone", skewed(1000.0f, 20000.0f, 1.0f, 5000.0f), 20000.0f, "Hz");
    addFloat(layout, kRing, "Ring", {0.0f, 100.0f, 0.1f}, 0.0f, "%");
    addFloat(layout, kRingPitch, "Ring Pitch", skewed(200.0f, 18000.0f, 1.0f, 2000.0f), 900.0f, "Hz");
    layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{kRumbleMode, 1}, "Rumble",
                                                            juce::StringArray{"OFF", "ROOM", "TAPS"}, 0));
    addFloat(layout, kRumbleAmount, "Rumble Amount", {0.0f, 100.0f, 0.1f}, 50.0f, "%");
    addFloat(layout, kRumbleLength, "Rumble Length", skewed(100.0f, 2000.0f, 1.0f, 600.0f), 600.0f, "ms");
    juce::StringArray divisions;
    for (const char* name : kDivisionNames)
        divisions.add(name);
    layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{kRumbleDivision, 1},
                                                            "Rumble Division", divisions, kDefaultDivision));
    return layout;
}

ParamReader::ParamReader(juce::AudioProcessorValueTreeState& state)
    : tune_(state.getRawParameterValue(kTune)), sweep_(state.getRawParameterValue(kSweep)),
      snap_(state.getRawParameterValue(kSnap)), length_(state.getRawParameterValue(kLength)),
      drive_(state.getRawParameterValue(kDrive)), bits_(state.getRawParameterValue(kBits)),
      rate_(state.getRawParameterValue(kRate)), tone_(state.getRawParameterValue(kTone)),
      ring_(state.getRawParameterValue(kRing)), ringPitch_(state.getRawParameterValue(kRingPitch)),
      rumbleMode_(state.getRawParameterValue(kRumbleMode)),
      rumbleAmount_(state.getRawParameterValue(kRumbleAmount)),
      rumbleLength_(state.getRawParameterValue(kRumbleLength)),
      rumbleDivision_(state.getRawParameterValue(kRumbleDivision)) {}

KickSettings ParamReader::read() const noexcept {
    KickSettings s;
    s.tuneSt = tune_->load();
    s.sweepSt = sweep_->load();
    s.snapMs = snap_->load();
    s.lengthMs = length_->load();
    s.drive01 = drive_->load() / 100.0f;
    s.bits = rounded(bits_);
    s.holdFactor = rounded(rate_) + 1;
    s.toneHz = tone_->load();
    s.ring01 = ring_->load() / 100.0f;
    s.ringPitchHz = ringPitch_->load();
    s.rumbleMode = static_cast<RumbleMode>(std::clamp(rounded(rumbleMode_), 0, 2));
    s.rumbleAmount01 = rumbleAmount_->load() / 100.0f;
    s.rumbleLengthMs = rumbleLength_->load();
    const auto division = static_cast<std::size_t>(std::clamp(rounded(rumbleDivision_), 0, 7));
    s.rumbleDivisionBeats = kDivisionBeats[division];
    return s;
}

} // namespace duster::params
