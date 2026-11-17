#pragma once

#include <numbers>

#include "Engine/EngineRate.h"

namespace duster {

// One-pole high-pass at kEngineRate that removes DC (from Kikrame; 20 Hz unless told otherwise).
class DcBlocker {
  public:
    explicit DcBlocker(double cutoffHz = 20.0) noexcept
        : pole_(static_cast<float>(1.0 - 2.0 * std::numbers::pi * cutoffHz / kEngineRate)) {}

    float process(float x) noexcept {
        const float y = x - x1_ + pole_ * y1_;
        x1_ = x;
        y1_ = y;
        return y;
    }
    void reset() noexcept { x1_ = y1_ = 0.0f; }

  private:
    float pole_;
    float x1_ = 0.0f;
    float y1_ = 0.0f;
};

} // namespace duster
