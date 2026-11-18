#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "Primitives/Biquad.h"
#include "TestSignals.h"

using namespace duster;
using Catch::Matchers::WithinAbs;

constexpr float kFs = 48000.0f;

TEST_CASE("biquad low-pass passes lows and cuts highs", "[filter]") {
    Biquad lp;
    lp.setLowPass(kFs, 1000.0f, 0.7071f);
    REQUIRE_THAT(test::measureGainDb([&](float x) { return lp.process(x); }, kFs, 100.0f),
                 WithinAbs(0.0, 0.2));
    lp.reset();
    REQUIRE_THAT(test::measureGainDb([&](float x) { return lp.process(x); }, kFs, 1000.0f),
                 WithinAbs(-3.0, 0.2));
    lp.reset();
    REQUIRE(test::measureGainDb([&](float x) { return lp.process(x); }, kFs, 8000.0f) < -30.0f);
}
