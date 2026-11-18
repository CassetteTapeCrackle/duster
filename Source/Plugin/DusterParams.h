#pragma once

#include <array>
#include <atomic>

#include <juce_audio_processors/juce_audio_processors.h>

#include "Engine/KickSettings.h"

namespace duster::params {

// Fixed forever: sessions and presets depend on these.
inline constexpr const char* kTune = "tune";
inline constexpr const char* kSweep = "sweep";
inline constexpr const char* kSnap = "snap";
inline constexpr const char* kLength = "length";
inline constexpr const char* kDrive = "drive";
inline constexpr const char* kBits = "bits";
inline constexpr const char* kRate = "rate";
inline constexpr const char* kTone = "tone";
inline constexpr const char* kRing = "ring";
inline constexpr const char* kRingPitch = "ringPitch";
inline constexpr const char* kRumbleMode = "rumbleMode";
inline constexpr const char* kRumbleAmount = "rumbleAmount";
inline constexpr const char* kRumbleLength = "rumbleLength";
inline constexpr const char* kRumbleDivision = "rumbleDivision";

// RATE: item i holds every (i + 1)-th engine sample. Item 0 is OFF, item 4 is 48 kHz / 5 = 9.6 kHz.
inline constexpr int kRateItems = 32;
// TAPS DIVISION items and their length in quarter notes.
inline constexpr std::array<const char*, 8> kDivisionNames = {"1/4",   "1/8D", "1/8",   "1/8T",
                                                              "1/16D", "1/16", "1/16T", "1/32"};
inline constexpr std::array<float, 8> kDivisionBeats = {1.0f,   0.75f, 0.5f,        1.0f / 3.0f,
                                                        0.375f, 0.25f, 1.0f / 6.0f, 0.125f};
inline constexpr int kDefaultDivision = 5; // 1/16

juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

// Reads every parameter into engine units. Caches the raw value pointers, so read() is safe on the
// audio thread.
class ParamReader {
  public:
    explicit ParamReader(juce::AudioProcessorValueTreeState& state);
    KickSettings read() const noexcept;

  private:
    std::atomic<float>* tune_;
    std::atomic<float>* sweep_;
    std::atomic<float>* snap_;
    std::atomic<float>* length_;
    std::atomic<float>* drive_;
    std::atomic<float>* bits_;
    std::atomic<float>* rate_;
    std::atomic<float>* tone_;
    std::atomic<float>* ring_;
    std::atomic<float>* ringPitch_;
    std::atomic<float>* rumbleMode_;
    std::atomic<float>* rumbleAmount_;
    std::atomic<float>* rumbleLength_;
    std::atomic<float>* rumbleDivision_;
};

} // namespace duster::params
