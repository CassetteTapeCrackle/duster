#pragma once

#include <algorithm>
#include <cmath>
#include <numbers>

#include "Engine/EngineRate.h"

namespace duster {

// TONE's low-pass is a 2-pole RBJ with a gentle knee: Q 0.4 fits both of Kick_05's crusher lines
// (9.55 and 19.15 kHz), where 0.707 cut the upper one 9 dB too hard at 48 kHz.
inline constexpr float kToneQ = 0.4f;

// The filter corner for a TONE value, so that TONE is the -3 dB frequency. At Q 0.4 a 2-pole
// low-pass is 3 dB down at 0.4728 of its corner (x^4 + (1/Q^2 - 2) x^2 - 1 = 0), measured in the
// bilinear transform's warped frequency, tan(pi f / fs).
inline float toneCornerHz(float toneHz) noexcept {
    constexpr double kMinus3dbRatio = 0.472795;
    const double warped = std::tan(std::numbers::pi * toneHz / kEngineRate) / kMinus3dbRatio;
    return static_cast<float>(
        std::min(kEngineRate / std::numbers::pi * std::atan(warped), 0.49 * kEngineRate));
}

} // namespace duster
