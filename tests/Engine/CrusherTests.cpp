#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <vector>

#include "Engine/Crusher.h"
#include "TestSignals.h"

using namespace duster;
using Catch::Matchers::WithinAbs;

namespace {
std::vector<float> ramp(int n) {
    std::vector<float> x(static_cast<std::size_t>(n));
    for (std::size_t i = 0; i < x.size(); ++i)
        x[i] = 0.001f * static_cast<float>(i);
    return x;
}

// True when hz sits within widthHz of k * 9600 +- f0 for some k.
bool nearHoldImage(double hz, double f0, double widthHz) {
    for (int k = 1; k <= 2; ++k)
        for (const double side : {-f0, f0})
            if (std::abs(hz - (k * 9600.0 + side)) <= widthHz)
                return true;
    return false;
}
} // namespace

TEST_CASE("hold 5 repeats every fifth sample from the reset", "[crusher]") {
    Crusher crusher;
    crusher.setHoldFactor(5);
    crusher.reset();
    const auto x = ramp(100);
    for (std::size_t i = 0; i < x.size(); ++i)
        REQUIRE(crusher.process(x[i]) == x[i - i % 5]);
}

TEST_CASE("hold 5 at 48 kHz puts lines at k * 9600 +- f0 and nowhere else", "[crusher]") {
    constexpr double kFs = 48000.0, kF0 = 46.9;
    Crusher crusher;
    crusher.setHoldFactor(5);
    auto x = test::sine(kFs, kF0, 1.0, 1.0f);
    for (auto& s : x)
        s = crusher.process(s);
    const auto spec = test::spectrum(x, kFs);
    REQUIRE_THAT(spec.peakHz(9000.0, 10000.0), WithinAbs(9600.0 - kF0, 2.0));
    REQUIRE_THAT(spec.peakHz(18500.0, 20000.0), WithinAbs(19200.0 - kF0, 2.0));

    // No ladder of extra lines: every local peak above -90 dB in 4-22 kHz is a hold image.
    const double floor = spec.peakNear(kF0, 2.0) * std::pow(10.0, -90.0 / 20.0);
    for (std::size_t k = spec.binOf(4000.0); k <= spec.binOf(22000.0); ++k) {
        const auto& m = spec.magnitude;
        const bool localPeak = m[k] > m[k - 1] && m[k] >= m[k + 1] && m[k] > floor;
        if (localPeak) {
            INFO("peak at " << static_cast<double>(k) * spec.binHz << " Hz");
            REQUIRE(nearHoldImage(static_cast<double>(k) * spec.binHz, kF0, 10.0));
        }
    }
}

TEST_CASE("8 bits snaps every output to a multiple of 1/128", "[crusher]") {
    Crusher crusher;
    crusher.setBits(8);
    for (const float x : test::sine(48000.0, 46.9, 0.1, 0.9f)) {
        const float steps = crusher.process(x) * 128.0f;
        REQUIRE(steps == std::round(steps));
    }
}

TEST_CASE("16 bits and hold 1 pass the input through unchanged", "[crusher]") {
    Crusher crusher;
    crusher.setBits(16);
    crusher.setHoldFactor(1);
    for (const float x : test::sine(48000.0, 46.9, 0.1, 0.9f))
        REQUIRE(crusher.process(x) == x);
}

TEST_CASE("reset restarts the hold phase", "[crusher]") {
    Crusher crusher;
    crusher.setHoldFactor(5);
    const auto x = ramp(10);
    for (int i = 0; i < 3; ++i)
        crusher.process(x[static_cast<std::size_t>(i)]);
    crusher.reset();
    REQUIRE(crusher.process(x[3]) == x[3]);
    REQUIRE(crusher.process(x[4]) == x[3]);
}
