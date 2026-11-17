#include "Engine/SafetyLimiter.h"

#include <algorithm>
#include <cmath>

namespace duster {

namespace {
constexpr float kReleaseCoeff = 2.6038e-4f; // 80 ms one-pole at kEngineRate
}

void SafetyLimiter::process(float* samples, int numSamples) noexcept {
    for (int i = 0; i < numSamples; ++i) {
        const float x = std::isfinite(samples[i]) ? samples[i] : 0.0f;
        const float peak = std::abs(x);
        const float needed = peak > kCeiling ? kCeiling / peak : 1.0f;
        gain_ = needed < gain_ ? needed : std::min(needed, gain_ + (1.0f - gain_) * kReleaseCoeff);
        samples[i] = std::clamp(x * gain_, -kCeiling, kCeiling);
    }
}

} // namespace duster
