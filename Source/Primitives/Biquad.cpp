// Biquad.cpp
#include "Primitives/Biquad.h"

#include <algorithm>
#include <cmath>

namespace duster {
namespace {
struct Omega {
    double cosW;
    double alpha;
};
Omega omegaFor(float fs, float hz, float q) noexcept {
    const double f = std::clamp(static_cast<double>(hz), 1.0, 0.49 * static_cast<double>(fs));
    const double w = 2.0 * 3.14159265358979323846 * f / static_cast<double>(fs);
    return {std::cos(w), std::sin(w) / (2.0 * std::max(0.05, static_cast<double>(q)))};
}
} // namespace

void Biquad::setNormalized(double b0, double b1, double b2, double a0, double a1, double a2) noexcept {
    b0_ = static_cast<float>(b0 / a0);
    b1_ = static_cast<float>(b1 / a0);
    b2_ = static_cast<float>(b2 / a0);
    a1_ = static_cast<float>(a1 / a0);
    a2_ = static_cast<float>(a2 / a0);
}

void Biquad::setLowPass(float fs, float hz, float q) noexcept {
    const auto [c, a] = omegaFor(fs, hz, q);
    setNormalized((1.0 - c) / 2.0, 1.0 - c, (1.0 - c) / 2.0, 1.0 + a, -2.0 * c, 1.0 - a);
}

} // namespace duster
