#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>

#include "Engine/Drive.h"
#include "TestSignals.h"

using namespace duster;
using Catch::Matchers::WithinAbs;

TEST_CASE("drive keeps silence silent and is a bypass at zero", "[drive]") {
    for (const float d : {0.0f, 0.2f, 1.0f})
        REQUIRE(driveSample(0.0f, d) == 0.0f);
    for (float x = -1.0f; x <= 1.0f; x += 0.01f)
        REQUIRE_THAT(driveSample(x, 0.0f), WithinAbs(x, 1e-3));
}

TEST_CASE("turning drive up from zero does not jump", "[drive]") {
    for (float x = -1.0f; x <= 1.0f; x += 0.01f)
        REQUIRE_THAT(driveSample(x, 0.002f), WithinAbs(x, 0.01));
}

TEST_CASE("drive is monotonic and stays near full scale", "[drive]") {
    for (const float d : {0.0f, 0.2f, 0.5f, 1.0f}) {
        INFO("drive " << d);
        float previous = driveSample(-1.0f, d);
        for (float x = -1.0f; x <= 1.0f; x += 0.001f) {
            const float y = driveSample(x, d);
            REQUIRE(y >= previous);
            REQUIRE(std::abs(y) <= 1.05f);
            previous = y;
        }
    }
}

TEST_CASE("drive 20 % gives Kick_05's harmonics", "[drive]") {
    constexpr double kFs = 48000.0, kF0 = 46.9;
    auto x = test::sine(kFs, kF0, 2.0, 1.0f);
    for (auto& s : x)
        s = driveSample(s, 0.2f);
    const auto spec = test::spectrum(x, kFs);
    const double fundamental = spec.peakNear(kF0, 2.0);
    REQUIRE_THAT(test::toDb(spec.peakNear(3 * kF0, 2.0) / fundamental), WithinAbs(-21.0, 3.0));
    REQUIRE_THAT(test::toDb(spec.peakNear(2 * kF0, 2.0) / fundamental), WithinAbs(-28.0, 4.0));
}
