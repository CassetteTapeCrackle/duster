#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "Engine/AmpShape.h"

using namespace duster;
using Catch::Matchers::WithinAbs;

TEST_CASE("the amp shape is Kick_05's level curve", "[amp]") {
    REQUIRE(ampShape(0.0f, 390.0f) == 0.6f);
    REQUIRE_THAT(ampShape(6.0f, 390.0f), WithinAbs(1.0, 1e-3));
    REQUIRE_THAT(ampShape(340.0f, 390.0f), WithinAbs(0.7, 0.01));
    REQUIRE(ampShape(390.0f, 390.0f) == 0.0f);
    REQUIRE(ampShape(500.0f, 390.0f) == 0.0f);
}

TEST_CASE("the amp shape never rises after the attack", "[amp]") {
    for (const float length : {80.0f, 390.0f, 1500.0f}) {
        INFO("length " << length);
        float previous = ampShape(6.0f, length);
        for (float t = 6.0f; t <= length + 10.0f; t += 0.1f) {
            const float level = ampShape(t, length);
            REQUIRE(level >= 0.0f);
            REQUIRE(level <= previous);
            previous = level;
        }
        REQUIRE(ampShape(length, length) == 0.0f);
    }
}
