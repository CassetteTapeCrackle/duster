#pragma once

#include <numbers>

#include "Engine/EngineRate.h"

namespace duster {

// One-pole high-pass that removes DC below about 20 Hz at kEngineRate (from Kikrame).
class DcBlocker {
  public:
    float process(float x) noexcept {
        const float y = x - x1_ + kPole * y1_;
        x1_ = x;
        y1_ = y;
        return y;
    }
    void reset() noexcept { x1_ = y1_ = 0.0f; }

  private:
    static constexpr float kPole = static_cast<float>(1.0 - 2.0 * std::numbers::pi * 20.0 / kEngineRate);
    float x1_ = 0.0f;
    float y1_ = 0.0f;
};

} // namespace duster
