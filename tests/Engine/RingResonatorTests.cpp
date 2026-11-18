#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <algorithm>
#include <cmath>
#include <vector>

#include "Engine/KickVoice.h"
#include "Engine/RingResonator.h"
#include "Engine/SafetyLimiter.h"
#include "TestSignals.h"

using namespace duster;
using Catch::Matchers::WithinAbs;

namespace {
constexpr int kOneSecond = 48000;

// One strike at sample 0 on silence, then the ring left alone.
std::vector<float> strikeOnce(float ring01, float pitchHz, int samples) {
    RingResonator ring;
    ring.setRing(ring01, pitchHz);
    std::vector<float> out(static_cast<std::size_t>(samples));
    for (std::size_t i = 0; i < out.size(); ++i)
        out[i] = ring.process(0.0f, i == 0, 1.0f);
    return out;
}

// Seconds until |x| last exceeds `belowPeakDb` under its peak.
double decaySeconds(const std::vector<float>& x, double belowPeakDb) {
    const float peak =
        *std::max_element(x.begin(), x.end(), [](float a, float b) { return std::abs(a) < std::abs(b); });
    const float threshold = std::abs(peak) * static_cast<float>(std::pow(10.0, -belowPeakDb / 20.0));
    std::size_t last = 0;
    for (std::size_t i = 0; i < x.size(); ++i)
        if (std::abs(x[i]) > threshold)
            last = i;
    return static_cast<double>(last) / kOneSecond;
}

// One INIT hit with RING and RING PITCH set, until the voice stops.
std::vector<float> initHit(float ring01, float ringPitchHz) {
    const KickSettings init;
    KickVoice voice;
    voice.start(init, 1.0f);
    const VoiceControls controls{init.drive01, init.toneHz, ring01, ringPitchHz, init.bits, init.holdFactor};
    std::vector<float> out;
    while (voice.active())
        out.push_back(voice.next(controls));
    return out;
}

// RMS of the spectrum between loHz and hiHz, in the spectrum's own units.
double bandRms(const std::vector<float>& x, double loHz, double hiHz) {
    const auto spec = test::spectrum(x, 48000.0);
    double sum = 0.0;
    for (std::size_t k = spec.binOf(loHz); k <= spec.binOf(hiHz); ++k)
        sum += spec.magnitude[k] * spec.magnitude[k];
    return std::sqrt(sum);
}

double meanOf(const std::vector<float>& x, std::size_t from) {
    double sum = 0.0;
    for (std::size_t i = from; i < x.size(); ++i)
        sum += x[i];
    return sum / static_cast<double>(x.size() - from);
}
} // namespace

TEST_CASE("ring 0 passes the driven body through untouched", "[ring]") {
    RingResonator ring;
    ring.setRing(0.0f, 900.0f);
    const auto body = test::sine(48000.0, 46.9, 0.5, 0.8f);
    for (std::size_t i = 0; i < body.size(); ++i)
        REQUIRE(ring.process(body[i], i % 1024 == 0, 1.0f) == body[i]);
}

TEST_CASE("a strike rings at multiples of the ring pitch", "[ring]") {
    const auto out = strikeOnce(0.6f, 900.0f, kOneSecond / 4);

    int bestLag = 0;
    double best = 0.0;
    for (int lag = 30; lag <= 80; ++lag) {
        double ac = 0.0;
        for (std::size_t i = 0; i + static_cast<std::size_t>(lag) < out.size(); ++i)
            ac += static_cast<double>(out[i]) * out[i + static_cast<std::size_t>(lag)];
        if (ac > best)
            best = ac, bestLag = lag;
    }
    REQUIRE(std::abs(bestLag - 48000.0 / 900.0) <= 0.5);

    const auto spec = test::spectrum(out, 48000.0);
    for (int k = 1; k <= 5; ++k) {
        INFO("partial " << k);
        REQUIRE_THAT(spec.peakHz(k * 900.0 - 200.0, k * 900.0 + 200.0), WithinAbs(k * 900.0, 15.0));
    }
}

TEST_CASE("an 18 kHz ring at full ring stays finite and dies within a second", "[ring]") {
    const auto out = strikeOnce(1.0f, 18000.0f, kOneSecond);
    REQUIRE(std::all_of(out.begin(), out.end(), [](float x) { return std::isfinite(x); }));
    REQUIRE(decaySeconds(out, 60.0) < 1.0);
}

TEST_CASE("more ring rings longer", "[ring]") {
    const double shortRing = decaySeconds(strikeOnce(0.3f, 900.0f, kOneSecond), 40.0);
    const double longRing = decaySeconds(strikeOnce(1.0f, 900.0f, kOneSecond), 40.0);
    REQUIRE(longRing > 2.0 * shortRing);
}

TEST_CASE("the ring never builds up DC from the body or the strikes", "[ring]") {
    RingResonator ring;
    ring.setRing(1.0f, 900.0f);
    std::vector<float> fromBody(kOneSecond), fromStrikes(kOneSecond);
    for (std::size_t i = 0; i < fromBody.size(); ++i)
        fromBody[i] = ring.process(0.2f, false, 1.0f) - 0.2f;
    RingResonator struck;
    struck.setRing(1.0f, 900.0f);
    for (std::size_t i = 0; i < fromStrikes.size(); ++i)
        fromStrikes[i] = struck.process(0.0f, i % 1024 == 0, 1.0f); // a strike per 47 Hz body cycle
    REQUIRE(std::abs(meanOf(fromBody, kOneSecond / 2)) < 0.01);
    REQUIRE(std::abs(meanOf(fromStrikes, kOneSecond / 2)) < 0.01);
}

TEST_CASE("the lowest ring pitch rings at a 240-sample period", "[ring]") {
    const auto out = strikeOnce(0.6f, 200.0f, kOneSecond / 2);
    int bestLag = 0;
    double best = 0.0;
    for (int lag = 200; lag <= 280; ++lag) {
        double ac = 0.0;
        for (std::size_t i = 0; i + static_cast<std::size_t>(lag) < out.size(); ++i)
            ac += static_cast<double>(out[i]) * out[i + static_cast<std::size_t>(lag)];
        if (ac > best)
            best = ac, bestLag = lag;
    }
    REQUIRE(bestLag == 240);
}

TEST_CASE("sweeping ring and pitch every sample stays finite", "[ring]") {
    RingResonator ring;
    for (int i = 0; i < kOneSecond; ++i) {
        const float t = static_cast<float>(i) / kOneSecond;
        ring.setRing(t, 200.0f * std::pow(90.0f, t)); // 200 Hz to 18 kHz
        REQUIRE(
            std::isfinite(ring.process(0.8f * std::sin(0.01f * static_cast<float>(i)), i % 1024 == 0, 1.0f)));
    }
}

TEST_CASE("a DC step at note-on does not thump through the ring", "[ring]") {
    // The biased drive's DC appears as a step when a hit starts.
    RingResonator ring;
    ring.setRing(1.0f, 900.0f);
    float worst = 0.0f;
    for (int i = 0; i < kOneSecond / 5; ++i)
        worst = std::max(worst, std::abs(ring.process(0.25f, false, 1.0f) - 0.25f));
    REQUIRE(worst < 0.5f);
}

TEST_CASE("RING changes the colour, not the level", "[ring]") {
    const auto dry = initHit(0.0f, 900.0f);
    const double dryLow = bandRms(dry, 30.0, 150.0);
    for (const float ring : {0.0f, 0.5f, 1.0f})
        for (const float pitch : {200.0f, 900.0f, 5000.0f, 13470.0f, 18000.0f}) {
            INFO("ring " << ring << ", pitch " << pitch);
            const auto hit = initHit(ring, pitch);
            float peak = 0.0f;
            for (const float x : hit)
                peak = std::max(peak, std::abs(x));
            REQUIRE(peak <= SafetyLimiter::kCeiling);
            REQUIRE(std::abs(test::toDb(bandRms(hit, 30.0, 150.0) / dryLow)) <= 1.0);
        }
}
