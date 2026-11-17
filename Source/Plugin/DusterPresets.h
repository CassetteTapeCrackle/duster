#pragma once

#include <array>
#include <span>
#include <string_view>
#include <utility>

namespace duster {

// A factory preset: a whole sound, every parameter in its own units (choices as their item index).
struct DusterPreset {
    std::string_view name;
    std::array<std::pair<std::string_view, float>, 14> values;
};

// Exposed to the host as programs, INIT first.
std::span<const DusterPreset> factoryPresets();

} // namespace duster
