#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "Engine/Tone.h"
#include "Primitives/Biquad.h"
#include "TestSignals.h"

using namespace duster;
using Catch::Matchers::WithinAbs;

namespace {
float toneGainDb(float toneHz, float atHz) {
    Biquad lp;
    lp.setLowPass(48000.0f, toneCornerHz(toneHz), kToneQ);
    return test::measureGainDb([&](float x) { return lp.process(x); }, 48000.0f, atHz);
}
} // namespace

TEST_CASE("TONE is the filter's -3 dB point", "[tone]") {
    for (const float tone : {1000.0f, 6365.0f, 12000.0f, 20000.0f}) {
        INFO("TONE " << tone);
        REQUIRE_THAT(toneGainDb(tone, tone), WithinAbs(-3.0, 0.3));
    }
}

TEST_CASE("TONE fully open leaves the top end alone", "[tone]") {
    REQUIRE(toneGainDb(20000.0f, 10000.0f) > -0.5f);
    REQUIRE(toneGainDb(20000.0f, 15000.0f) > -1.5f);
}
