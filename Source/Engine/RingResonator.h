#pragma once

#include <array>

#include "Primitives/DcBlocker.h"

namespace duster {

// RING: a feedback comb tuned to 48000 / pitchHz samples, after DRIVE. It is fed by the driven body's
// top (above 100 Hz, half the lowest ring pitch) plus a strike at each upward zero crossing of the body
// (scaled by the amp envelope), so it rings with the kick's own cycles instead of a separate layer.
// The comb is scaled by 1 - feedback, so its resonances pass at unity gain, and RING crossfades the
// body's top into it: more ring keeps only the comb's partials and rings longer, never louder.
class RingResonator {
  public:
    void reset() noexcept;
    void setRing(float ring01, float pitchHz) noexcept;
    // Returns the driven body with its top crossfaded into the ring.
    float process(float driven, bool strike, float env) noexcept;

  private:
    static constexpr int kBufferSize = 256; // > 48000 / 200 Hz, the lowest ring pitch
    std::array<float, kBufferSize> buffer_{};
    int write_ = 0;
    float ring_ = 0.0f;
    float feedback_ = 0.5f;
    float delaySamples_ = 48000.0f / 900.0f;
    DcBlocker inputDc_{100.0};
};

} // namespace duster
