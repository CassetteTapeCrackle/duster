#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

#include "Engine/DusterEngine.h"
#include "Engine/PreviewRender.h"
#include "Engine/SafetyLimiter.h"
#include "TestSignals.h"

using namespace duster;
using Catch::Matchers::WithinAbs;

namespace {
constexpr double kFs = 48000.0;
constexpr int kMs = 48;

KickSettings crushed05() {
    KickSettings s;
    s.tuneSt = 0.2f;
    s.sweepSt = 56.0f;
    s.snapMs = 3.7f;
    s.lengthMs = 390.0f;
    s.drive01 = 0.22f;
    s.bits = 8;
    s.holdFactor = 5;
    s.toneHz = 6365.0f;
    return s;
}

struct Note {
    int atSample;
    int midiNote;
};

// Renders `samples` in blocks of `block`, firing each note at its sample. `eachBlock(start, engine)`
// runs before each block, for automation.
template <typename EachBlock>
std::vector<float> render(const KickSettings& s, const std::vector<Note>& notes, int samples, float velocity,
                          int block, EachBlock&& eachBlock) {
    DusterEngine engine;
    engine.prepare();
    engine.setSettings(s);
    std::vector<float> out(static_cast<std::size_t>(samples));
    int pos = 0;
    while (pos < samples) {
        int end = std::min(samples, pos + block);
        for (const auto& note : notes)
            if (note.atSample > pos && note.atSample < end)
                end = note.atSample; // split the block so the note lands on its sample
        eachBlock(pos, engine);
        for (const auto& note : notes)
            if (note.atSample == pos)
                engine.noteOn(note.midiNote, velocity);
        engine.render(out.data() + pos, end - pos);
        pos = end;
    }
    return out;
}

std::vector<float> render(const KickSettings& s, const std::vector<Note>& notes, int samples,
                          float velocity) {
    return render(s, notes, samples, velocity, 512, [](int, DusterEngine&) {});
}

float largestStep(const std::vector<float>& x, std::size_t from, std::size_t to) {
    float worst = 0.0f;
    for (std::size_t i = std::max<std::size_t>(from, 1); i < to; ++i)
        worst = std::max(worst, std::abs(x[i] - x[i - 1]));
    return worst;
}

float peakAbs(const std::vector<float>& x, std::size_t from = 0, std::size_t to = SIZE_MAX) {
    float p = 0.0f;
    for (std::size_t i = from; i < std::min(to, x.size()); ++i)
        p = std::max(p, std::abs(x[i]));
    return p;
}
} // namespace

TEST_CASE("05 CRUSHED settings render Kick_05's body and ring", "[engine]") {
    const auto out = render(crushed05(), {{0, 30}}, 500 * kMs, 1.0f);
    const auto spec = test::spectrum(out, kFs, 100 * kMs, 260 * kMs);
    const double f0 = spec.peakHz(30.0, 80.0);
    REQUIRE_THAT(f0, WithinAbs(46.9, 1.0));
    REQUIRE_THAT(spec.peakHz(9000.0, 10000.0), WithinAbs(9600.0 - f0, 2.0));
    REQUIRE_THAT(spec.peakHz(18500.0, 19500.0), WithinAbs(19200.0 - f0, 2.0));
    const double ringDb = test::toDb(spec.peakNear(9600.0 - f0, 3.0) / spec.peakNear(f0, 3.0));
    REQUIRE_THAT(ringDb, WithinAbs(-53.0, 4.0));
    REQUIRE(peakAbs(out, 390 * kMs) == 0.0f);
}

TEST_CASE("a retrigger crossfades without a click", "[engine]") {
    const KickSettings init;
    const auto retrig = render(init, {{0, 30}, {100 * kMs, 30}}, 200 * kMs, 0.8f);
    REQUIRE(largestStep(retrig, 99 * kMs, 105 * kMs) < 0.25f);

    // Once the old hit has faded, the new one is the solo hit, shifted.
    const auto solo = render(init, {{0, 30}}, 100 * kMs, 0.8f);
    float worst = 0.0f;
    for (std::size_t i = 4 * kMs; i < 60 * kMs; ++i)
        worst = std::max(worst, std::abs(retrig[100 * kMs + i] - solo[i]));
    REQUIRE(worst < 1e-5f);

    const auto close = render(init, {{0, 30}, {5 * kMs, 30}, {10 * kMs, 30}}, 50 * kMs, 0.8f);
    REQUIRE(largestStep(close, 0, close.size()) < 0.1f);

    // Three notes inside one 3 ms fade: no voice may be cut while it is still loud.
    const auto roll = render(init, {{0, 30}, {1 * kMs, 30}, {2 * kMs, 30}}, 50 * kMs, 0.8f);
    REQUIRE(largestStep(roll, 0, roll.size()) < 0.1f);
}

TEST_CASE("two notes before the same sample play once, the later one", "[engine]") {
    DusterEngine engine;
    engine.prepare();
    engine.setSettings(KickSettings{});
    engine.noteOn(30, 1.0f);
    engine.noteOn(42, 0.5f);
    std::vector<float> both(100 * kMs);
    engine.render(both.data(), static_cast<int>(both.size()));
    const auto later = render(KickSettings{}, {{0, 42}}, 100 * kMs, 0.5f);
    REQUIRE(both == later);
}

TEST_CASE("velocity scales the level, nothing else", "[engine]") {
    const KickSettings init;
    const auto loud = render(init, {{0, 30}}, 300 * kMs, 1.0f);
    const auto soft = render(init, {{0, 30}}, 300 * kMs, 0.5f);
    REQUIRE_THAT(test::toDb(peakAbs(soft) / peakAbs(loud)), WithinAbs(-6.02, 0.5));
}

TEST_CASE("every hit with the same settings is identical", "[engine]") {
    const auto out = render(crushed05(), {{0, 30}, {500 * kMs, 30}}, 1000 * kMs, 1.0f);
    for (std::size_t i = 0; i < 500 * kMs; ++i)
        REQUIRE(out[i] == out[500 * kMs + i]);
}

TEST_CASE("knob corners stay finite and under the ceiling", "[engine]") {
    KickSettings high;
    high.sweepSt = 60.0f;
    high.snapMs = 1.0f;
    KickSettings harsh;
    harsh.bits = 2;
    harsh.drive01 = 1.0f;
    harsh.ring01 = 1.0f;
    harsh.ringPitchHz = 18000.0f;
    harsh.rumbleMode = RumbleMode::Taps;
    harsh.rumbleAmount01 = 1.0f;
    for (const auto& [s, note] : {std::pair{high, 127}, std::pair{harsh, 30}, std::pair{harsh, 127}}) {
        const auto out = render(s, {{0, note}, {50 * kMs, note}}, 1500 * kMs, 1.0f);
        REQUIRE(std::all_of(out.begin(), out.end(), [](float x) { return std::isfinite(x); }));
        REQUIRE(peakAbs(out) <= SafetyLimiter::kCeiling);
    }
}

TEST_CASE("sweeping TONE mid-hit does not click", "[engine]") {
    const auto out =
        render(KickSettings{}, {{0, 30}}, 200 * kMs, 0.8f, 48, [](int pos, DusterEngine& engine) {
            KickSettings s;
            const float t = std::clamp((static_cast<float>(pos) - 100.0f * kMs) / (5.0f * kMs), 0.0f, 1.0f);
            s.toneHz = 20000.0f + t * (1000.0f - 20000.0f); // 20 kHz to 1 kHz over 5 ms
            engine.setSettings(s);
        });
    REQUIRE(largestStep(out, 95 * kMs, 130 * kMs) < 0.25f);
}

TEST_CASE("a new note does not cut the rumble tail", "[engine]") {
    KickSettings room;
    room.rumbleMode = RumbleMode::Room;
    room.rumbleAmount01 = 1.0f;
    KickSettings dry = room;
    dry.rumbleMode = RumbleMode::Off;
    // The rumble alone: the render with it minus the render without it.
    auto rumbleOf = [&](const std::vector<Note>& notes) {
        const auto wet = render(room, notes, 600 * kMs, 0.5f);
        const auto kickOnly = render(dry, notes, 600 * kMs, 0.5f);
        std::vector<float> rumble(wet.size());
        for (std::size_t i = 0; i < rumble.size(); ++i)
            rumble[i] = wet[i] - kickOnly[i];
        return rumble;
    };
    const auto retriggered = rumbleOf({{0, 30}, {500 * kMs, 30}});
    const auto untouched = rumbleOf({{0, 30}});
    // The new kick reaches the room's output 24 ms later; until then the old tail must carry on.
    const float tail = test::rms(untouched, 500 * kMs, 520 * kMs);
    REQUIRE(tail > 0.0f);
    REQUIRE(std::abs(test::toDb(test::rms(retriggered, 500 * kMs, 520 * kMs) / tail)) < 3.0);
}

TEST_CASE("the preview is the live hit", "[engine]") {
    KickSettings s = crushed05();
    s.rumbleMode = RumbleMode::Room;
    const auto preview = renderPreviewHit(s, 30);
    const auto live = render(s, {{0, 30}}, static_cast<int>(preview.size()), 1.0f);
    REQUIRE(preview == live);
}

TEST_CASE("the limiter holds the ceiling and passes quiet signals", "[engine]") {
    SafetyLimiter limiter;
    auto hot = test::sine(kFs, 50.0, 0.5, 10.0f);
    hot[100] = std::numeric_limits<float>::quiet_NaN();
    hot[200] = std::numeric_limits<float>::infinity();
    limiter.process(hot.data(), static_cast<int>(hot.size()));
    REQUIRE(std::all_of(hot.begin(), hot.end(),
                        [](float x) { return std::isfinite(x) && std::abs(x) <= SafetyLimiter::kCeiling; }));

    SafetyLimiter fresh;
    auto quiet = test::sine(kFs, 50.0, 0.5, 0.5f);
    const auto original = quiet;
    fresh.process(quiet.data(), static_cast<int>(quiet.size()));
    REQUIRE(quiet == original);
}
