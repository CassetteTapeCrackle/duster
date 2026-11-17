#include <catch2/catch_test_macros.hpp>

#include "Engine/EngineRate.h"

// Kick_05's ring sits at 48000 / 5 = 9600 Hz; any other engine rate moves it.
TEST_CASE("the engine renders at exactly 48 kHz", "[engine]") {
    STATIC_REQUIRE(duster::kEngineRate == 48000.0);
}
