#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>

#include "Engine/EngineRate.h"
#include "Engine/KickOscillator.h"

using namespace duster;
using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;

namespace {
constexpr int samplesIn(float ms) {
    return static_cast<int>(ms * kEngineRate / 1000.0);
}

void advance(KickOscillator& osc, int samples) {
    for (int i = 0; i < samples; ++i)
        osc.next();
}
} // namespace

TEST_CASE("the sweep is the two-stage law fitted to Kick_05", "[sweep]") {
    REQUIRE(sweepSemitones(0.0f, 50.0f, 5.0f) == 50.0f);
    for (const float t : {1.0f, 10.0f, 100.0f}) {
        const double expected = 50.0 * (0.85 * std::exp2(-t / 5.0) + 0.15 * std::exp2(-t / 25.0));
        REQUIRE_THAT(sweepSemitones(t, 50.0f, 5.0f), WithinAbs(expected, 1e-4));
    }
}

TEST_CASE("the oscillator follows Kick_05's measured pitch", "[sweep]") {
    KickOscillator osc;
    osc.start(46.9f, 50.0f, 5.0f);
    int done = 0;
    const std::pair<float, float> kMeasured[] = {
        {3.5f, 323.0f}, {8.8f, 133.0f}, {20.0f, 72.0f}, {35.0f, 57.0f}};
    for (const auto& [ms, hz] : kMeasured) {
        advance(osc, samplesIn(ms) - done);
        done = samplesIn(ms);
        INFO("at " << ms << " ms");
        REQUIRE_THAT(osc.currentHz(), WithinRel(hz, 0.05f));
    }
}

TEST_CASE("the first swing goes negative", "[sweep]") {
    KickOscillator osc;
    osc.start(46.9f, 50.0f, 5.0f);
    float first = 0.0f;
    for (int i = 0; i < 100 && first == 0.0f; ++i)
        first = osc.next();
    REQUIRE(first < 0.0f);
}

TEST_CASE("a note far above Nyquist stays finite and clamps at 20 kHz", "[sweep]") {
    KickOscillator osc;
    osc.start(12543.9f /* MIDI 127 */, 60.0f, 1.0f);
    osc.next();
    REQUIRE(osc.currentHz() == 20000.0f);
    for (int i = 0; i < samplesIn(500.0f); ++i) {
        REQUIRE(std::isfinite(osc.next()));
        REQUIRE(osc.currentHz() <= 20000.0f);
    }
}

TEST_CASE("an upward zero crossing is flagged once per cycle", "[sweep]") {
    KickOscillator osc;
    osc.start(100.0f, 0.0f, 5.0f);
    int crossings = 0;
    for (int i = 0; i < samplesIn(1000.0f); ++i) {
        osc.next();
        crossings += osc.crossedZeroUpward() ? 1 : 0;
    }
    REQUIRE(crossings >= 99);
    REQUIRE(crossings <= 101);
}

TEST_CASE("the sweep never returns a subnormal on long notes", "[sweep]") {
    for (float t = 0.0f; t <= 1500.0f; t += 0.5f) {
        INFO("at " << t << " ms");
        REQUIRE(std::fpclassify(sweepSemitones(t, 60.0f, 1.0f)) != FP_SUBNORMAL);
    }
}
