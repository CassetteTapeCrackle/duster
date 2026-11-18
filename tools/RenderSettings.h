#pragma once

// Settings lines and the one-hit render shared by DusterRender's preset and settings modes.

#include <cstdlib>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <juce_audio_processors/juce_audio_processors.h>

#include "Plugin/DusterProcessor.h"

namespace duster::render {

struct SettingsLine {
    std::string name;
    std::vector<std::pair<std::string, float>> values;
};

// "<name>\t<id>=<value> <id>=<value> ...". Callers skip blank and '#' lines first.
inline std::optional<SettingsLine> parseSettingsLine(std::string_view line) {
    const auto tab = line.find('\t');
    if (tab == std::string_view::npos || tab == 0)
        return std::nullopt;
    SettingsLine out{std::string(line.substr(0, tab)), {}};
    std::string_view rest = line.substr(tab + 1);
    while (!rest.empty()) {
        const auto space = rest.find(' ');
        const auto token = rest.substr(0, space);
        rest = space == std::string_view::npos ? std::string_view{} : rest.substr(space + 1);
        if (token.empty())
            continue;
        const auto equals = token.find('=');
        if (equals == std::string_view::npos || equals == 0)
            return std::nullopt;
        const std::string number(token.substr(equals + 1));
        char* end = nullptr;
        const float value = std::strtof(number.c_str(), &end);
        if (number.empty() || end != number.c_str() + number.size())
            return std::nullopt;
        out.values.emplace_back(std::string(token.substr(0, equals)), value);
    }
    return out;
}

// Sets every parameter to its default, then the listed ones. False on an unknown id.
inline bool applySettings(DusterProcessor& processor, const SettingsLine& settings) {
    for (auto* parameter : processor.getParameters())
        parameter->setValueNotifyingHost(parameter->getDefaultValue());
    for (const auto& [id, value] : settings.values) {
        auto* parameter = processor.parameters().getParameter(juce::String(id));
        if (parameter == nullptr)
            return false;
        parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
    }
    return true;
}

// One second of note 30 (F#0) at velocity 1, left channel, latency-compensated so the hit starts at
// sample 0.
inline std::vector<float> renderHit(DusterProcessor& processor, double rate) {
    constexpr int kNote = 30;
    constexpr int kBlock = 512;
    processor.prepareToPlay(rate, kBlock);
    const int latency = processor.getLatencySamples();
    const int total = static_cast<int>(rate) + latency;
    std::vector<float> out;
    out.reserve(static_cast<std::size_t>(total + kBlock));
    juce::AudioBuffer<float> buffer(2, kBlock);
    juce::MidiBuffer midi;
    midi.addEvent(juce::MidiMessage::noteOn(1, kNote, 1.0f), 0);
    for (int start = 0; start < total; start += kBlock) {
        processor.processBlock(buffer, midi);
        midi.clear();
        for (int i = 0; i < kBlock; ++i)
            out.push_back(buffer.getSample(0, i));
    }
    return {out.begin() + latency, out.begin() + total};
}

} // namespace duster::render
