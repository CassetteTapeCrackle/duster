#pragma once

#include <algorithm>
#include <cmath>

namespace duster {

// Asymmetric tanh saturation. The bias adds the even harmonics; drive 0.2 lands on Kick_05's
// h2 -28 dB and h3 -21 dB. The gain starts at 0, so turning DRIVE up from zero never jumps, and the
// output is scaled so its larger extreme sits at full scale for |x| <= 1.
inline float driveSample(float x, float drive01) noexcept {
    constexpr float kGainPerDrive = 5.75f, kBiasPerDrive = 0.5f, kBypassBelow = 1e-3f;
    if (drive01 < kBypassBelow)
        return x;
    const float g = kGainPerDrive * drive01;
    const float b = kBiasPerDrive * drive01;
    const float tb = std::tanh(b);
    const float scale = std::max(std::tanh(g - b) + tb, std::tanh(g + b) - tb);
    return (std::tanh(g * x + b) - tb) / scale;
}

} // namespace duster
