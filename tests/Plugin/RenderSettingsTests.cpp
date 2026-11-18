#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <string>
#include <utility>
#include <vector>

#include <juce_events/juce_events.h>

#include "Plugin/DusterPresets.h"
#include "Plugin/DusterProcessor.h"
#include "RenderSettings.h"

using namespace duster;
using Catch::Matchers::WithinAbs;

namespace {
struct Juce {
    juce::ScopedJuceInitialiser_GUI init;
};
} // namespace

TEST_CASE("a settings line parses", "[render]") {
    const auto line = render::parseSettingsLine("05 CRUSHED\ttune=0.2 sweep=56");
    REQUIRE(line.has_value());
    REQUIRE(line->name == "05 CRUSHED");
    REQUIRE(line->values.size() == 2);
    REQUIRE(line->values[0].first == "tune");
    REQUIRE_THAT(line->values[0].second, WithinAbs(0.2, 1e-6));
    REQUIRE(line->values[1].first == "sweep");
    REQUIRE_THAT(line->values[1].second, WithinAbs(56.0, 1e-6));
    REQUIRE_FALSE(render::parseSettingsLine("x\ttune").has_value());
    REQUIRE_FALSE(render::parseSettingsLine("x\ttune=abc").has_value());
}

TEST_CASE("a settings line renders exactly like its preset", "[render]") {
    Juce juce;
    const auto presets = factoryPresets();
    for (int i = 0; i < static_cast<int>(presets.size()); ++i) {
        const auto& preset = presets[static_cast<std::size_t>(i)];
        INFO("preset " << preset.name);
        DusterProcessor fromPreset;
        fromPreset.setCurrentProgram(i);
        render::SettingsLine line{std::string(preset.name), {}};
        for (const auto& [id, value] : preset.values)
            line.values.emplace_back(std::string(id), value);
        DusterProcessor fromLine;
        REQUIRE(render::applySettings(fromLine, line));
        REQUIRE(render::renderHit(fromPreset, 48000.0) == render::renderHit(fromLine, 48000.0));
    }
}

TEST_CASE("unlisted parameters return to their defaults", "[render]") {
    Juce juce;
    DusterProcessor p;
    auto* drive = p.parameters().getParameter("drive");
    drive->setValueNotifyingHost(drive->convertTo0to1(80.0f));
    REQUIRE(render::applySettings(p, {"x", {{"tune", 1.0f}}}));
    REQUIRE_THAT(drive->getValue(), WithinAbs(drive->getDefaultValue(), 1e-9));
    REQUIRE_FALSE(render::applySettings(p, {"x", {{"nope", 1.0f}}}));
}
