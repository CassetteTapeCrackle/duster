// Biquad.h
#pragma once

namespace duster {

// RBJ cookbook biquad, transposed direct form II.
class Biquad {
  public:
    void setLowPass(float fs, float hz, float q) noexcept;

    float process(float x) noexcept {
        const float y = b0_ * x + z1_;
        z1_ = b1_ * x - a1_ * y + z2_;
        z2_ = b2_ * x - a2_ * y;
        return y;
    }
    void reset() noexcept { z1_ = z2_ = 0.0f; }

  private:
    void setNormalized(double b0, double b1, double b2, double a0, double a1, double a2) noexcept;

    float b0_ = 1.0f, b1_ = 0.0f, b2_ = 0.0f, a1_ = 0.0f, a2_ = 0.0f;
    float z1_ = 0.0f, z2_ = 0.0f;
};

} // namespace duster
