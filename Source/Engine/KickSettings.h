#pragma once

#include "Engine/RumbleBus.h"

namespace duster {

// Every hit is pitched from F#0 (MIDI 30), the note the presets were tuned on: TUNE moves it up to
// 12 st either way, and the MIDI note that triggers a hit never changes its pitch.
inline constexpr float kBaseHz = 46.249303f; // 440 Hz x 2^((30 - 69) / 12)

// Every knob, in engine units. The defaults are the INIT preset.
struct KickSettings {
    float tuneSt = 0.0f, sweepSt = 36.0f, snapMs = 8.0f, lengthMs = 400.0f;
    float drive01 = 0.0f;
    int bits = 16;
    int holdFactor = 1;
    float toneHz = 20000.0f;
    float ring01 = 0.0f, ringPitchHz = 900.0f;
    RumbleMode rumbleMode = RumbleMode::Off;
    float rumbleAmount01 = 0.5f, rumbleLengthMs = 600.0f;
    float rumbleDivisionBeats = 0.25f; // TAPS spacing in quarter notes: 0.25 is 1/16
};

} // namespace duster
