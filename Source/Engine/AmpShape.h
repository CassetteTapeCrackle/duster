#pragma once

#include <algorithm>

namespace duster {

// Kick_05's level over time, the same for every hit: 0.6 to 1 over 6 ms, hold to 100 ms, sag to 0.7 by
// lengthMs - 50, then a straight 50 ms release to silence at lengthMs. Short lengths squeeze out the
// hold and the sag first, so the curve never jumps.
inline float ampShape(float tMs, float lengthMs) noexcept {
    constexpr float kAttackMs = 6.0f, kStartLevel = 0.6f, kHoldEndMs = 100.0f;
    constexpr float kReleaseMs = 50.0f, kSagLevel = 0.7f;
    if (tMs >= lengthMs)
        return 0.0f;
    if (tMs < kAttackMs)
        return kStartLevel + (1.0f - kStartLevel) * std::max(tMs, 0.0f) / kAttackMs;

    const float releaseStart = std::max(kAttackMs, lengthMs - kReleaseMs);
    const float holdEnd = std::min(kHoldEndMs, releaseStart);
    if (tMs < holdEnd)
        return 1.0f;
    if (tMs < releaseStart)
        return 1.0f - (1.0f - kSagLevel) * (tMs - holdEnd) / (releaseStart - holdEnd);

    const float releaseFrom = releaseStart > holdEnd ? kSagLevel : 1.0f;
    return releaseFrom * (lengthMs - tMs) / (lengthMs - releaseStart);
}

} // namespace duster
