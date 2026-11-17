#pragma once

#include "Engine/RumbleBus.h"

namespace duster {

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
