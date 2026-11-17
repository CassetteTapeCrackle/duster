#pragma once

#include <algorithm>
#include <cmath>

namespace duster {

// BITS and RATE: quantize, then sample-and-hold for a whole number of engine samples. At 48 kHz a
// hold of 5 gives Kick_05's 9.6 kHz ring; a fractional hold would add a ladder of extra lines.
class Crusher {
  public:
    static constexpr int kBitsOff = 16;

    // 16 or more is off; otherwise steps of 1 / 2^(bits - 1).
    void setBits(int bits) noexcept {
        steps_ = bits >= kBitsOff ? 0.0f : std::exp2(static_cast<float>(bits - 1));
    }
    // 1 is off; n holds every n-th sample for n samples.
    void setHoldFactor(int holdFactor) noexcept { holdFactor_ = std::max(1, holdFactor); }
    void reset() noexcept { phase_ = 0; }

    float process(float x) noexcept {
        if (phase_ == 0)
            held_ = steps_ > 0.0f ? std::round(x * steps_) / steps_ : x;
        if (++phase_ >= holdFactor_)
            phase_ = 0;
        return held_;
    }

  private:
    float steps_ = 0.0f;
    int holdFactor_ = 1;
    int phase_ = 0;
    float held_ = 0.0f;
};

} // namespace duster
