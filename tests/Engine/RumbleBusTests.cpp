#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <algorithm>
#include <cmath>
#include <vector>

#include "Engine/RumbleBus.h"
#include "TestSignals.h"

using namespace duster;
using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;

namespace {
constexpr double kFs = 48000.0;
constexpr std::size_t kKickSamples = 1440; // 30 ms

// A 30 ms 50 Hz burst at t = 0, then silence; kickEnv is 1 during the burst.
std::vector<float> renderRumble(RumbleBus& bus, double seconds, float envDuringKick = 1.0f) {
    const auto kick = test::sine(kFs, 50.0, 30e-3, 1.0f);
    std::vector<float> out(static_cast<std::size_t>(kFs * seconds));
    for (std::size_t i = 0; i < out.size(); ++i) {
        const bool inKick = i < kKickSamples;
        out[i] = bus.process(inKick ? kick[i] : 0.0f, inKick ? envDuringKick : 0.0f);
    }
    return out;
}

RumbleBus preparedBus(RumbleMode mode) {
    RumbleBus bus;
    bus.prepare();
    bus.setMode(mode);
    bus.setAmount(1.0f);
    bus.setLength(600.0f);
    return bus;
}

// 5 ms RMS windows.
std::vector<float> envelope(const std::vector<float>& x) {
    constexpr std::size_t kWindow = 240;
    std::vector<float> env;
    for (std::size_t i = 0; i + kWindow <= x.size(); i += kWindow)
        env.push_back(test::rms(x, i, i + kWindow));
    return env;
}

// Lag in samples with the largest autocorrelation in [minLag, maxLag].
std::size_t strongestLag(const std::vector<float>& x, std::size_t minLag, std::size_t maxLag) {
    std::size_t best = minLag;
    double bestSum = -1.0;
    for (std::size_t lag = minLag; lag <= maxLag; ++lag) {
        double sum = 0.0;
        for (std::size_t i = 0; i + lag < x.size(); i += 2)
            sum += static_cast<double>(x[i]) * x[i + lag];
        if (sum > bestSum)
            bestSum = sum, best = lag;
    }
    return best;
}
} // namespace

TEST_CASE("rumble off is silent", "[rumble]") {
    RumbleBus bus = preparedBus(RumbleMode::Off);
    const auto out = renderRumble(bus, 0.5);
    REQUIRE(std::all_of(out.begin(), out.end(), [](float x) { return x == 0.0f; }));
}

TEST_CASE("room rumble rings for its length and stays low", "[rumble]") {
    RumbleBus bus = preparedBus(RumbleMode::Room);
    const auto out = renderRumble(bus, 2.0, 0.0f);
    const auto env = envelope(out);
    const auto peak = std::max_element(env.begin(), env.end());
    const float floor = *peak * 1e-3f; // -60 dB
    std::size_t last = 0;
    for (std::size_t i = 0; i < env.size(); ++i)
        if (env[i] > floor)
            last = i;
    const double decayMs = static_cast<double>(last - static_cast<std::size_t>(peak - env.begin())) * 5.0;
    REQUIRE_THAT(decayMs, WithinRel(600.0, 0.25));

    const auto spec = test::spectrum(out, kFs);
    const double strongest = spec.peakNear(0.0, 1000.0);
    REQUIRE(test::toDb(spec.peakNear(12500.0, 11500.0) / strongest) <= -30.0);
}

TEST_CASE("taps fall on 1/16 notes at 140 BPM when the host has no tempo", "[rumble]") {
    RumbleBus bus = preparedBus(RumbleMode::Taps);
    bus.setTempo(100.0);
    bus.setTempo(0.0);
    const auto out = renderRumble(bus, 1.0, 0.0f);
    const double sixteenthMs = 60000.0 / 140.0 / 4.0;
    REQUIRE_THAT(static_cast<double>(strongestLag(out, 2400, 9600)) / 48.0, WithinAbs(sixteenthMs, 1.0));
}

TEST_CASE("the taps division follows the setting", "[rumble]") {
    RumbleBus bus = preparedBus(RumbleMode::Taps);
    bus.setTempo(140.0);
    bus.setDivisionBeats(0.75f); // dotted 1/8
    const auto out = renderRumble(bus, 1.5, 0.0f);
    const double dottedEighthMs = 60000.0 / 140.0 * 0.75;
    REQUIRE_THAT(static_cast<double>(strongestLag(out, 9600, 24000)) / 48.0, WithinAbs(dottedEighthMs, 1.0));
}

TEST_CASE("taps duck under the kick", "[rumble]") {
    // The kick's envelope stays up for the whole render, so every tap lands under it.
    auto renderDucked = [](float env) {
        RumbleBus bus = preparedBus(RumbleMode::Taps);
        const auto kick = test::sine(kFs, 50.0, 30e-3, 1.0f);
        std::vector<float> out(24000);
        for (std::size_t i = 0; i < out.size(); ++i)
            out[i] = bus.process(i < kKickSamples ? kick[i] : 0.0f, env);
        return test::rms(out, 4800, out.size());
    };
    REQUIRE(test::toDb(renderDucked(1.0f) / renderDucked(0.0f)) <= -12.0);
}

TEST_CASE("rumble goes to exact zeros once the tail is gone", "[rumble]") {
    for (const auto mode : {RumbleMode::Room, RumbleMode::Taps}) {
        RumbleBus bus = preparedBus(mode);
        const auto out = renderRumble(bus, 2.5, 0.0f);
        const std::size_t idleFrom = kKickSamples + static_cast<std::size_t>(2.0 * kFs);
        REQUIRE(std::all_of(out.begin() + static_cast<std::ptrdiff_t>(idleFrom), out.end(),
                            [](float x) { return x == 0.0f; }));
    }
}
