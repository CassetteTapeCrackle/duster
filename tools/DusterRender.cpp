// Renders Duster through DusterProcessor (resampler included) to 24-bit mono WAVs for tuning against
// the sample pack:
//
//   DusterRender <outDir> [presetName...]                     every preset when none is named
//   DusterRender <outDir> --settings <file> [--rate 44100|48000]  one hit per settings line
//   DusterRender --list-params                                 id, min, max, default, step per line
//
// Writes <outDir>/<name>_44k.wav and <name>_48k.wav (one of them with --rate): note 30 (F#0) at
// velocity 1, one second, latency-compensated so the hit starts at sample 0. A settings line is
// "<name>\t<id>=<value> ...": unlisted parameters take their defaults; blank and '#' lines are skipped.
#include <cmath>
#include <cstdio>
#include <fstream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_events/juce_events.h>

#include "Plugin/DusterPresets.h"
#include "Plugin/DusterProcessor.h"
#include "RenderSettings.h"

namespace {
using Rates = std::vector<std::pair<double, const char*>>;

bool writeWav(const juce::File& file, const std::vector<float>& samples, double rate) {
    file.deleteFile();
    std::unique_ptr<juce::OutputStream> stream = file.createOutputStream();
    if (stream == nullptr)
        return false;
    juce::WavAudioFormat wav;
    const auto options =
        juce::AudioFormatWriterOptions{}.withSampleRate(rate).withNumChannels(1).withBitsPerSample(24);
    auto writer = wav.createWriterFor(stream, options); // takes the stream on success
    if (writer == nullptr)
        return false;
    const float* channels[] = {samples.data()};
    const bool written = writer->writeFromFloatArrays(channels, 1, static_cast<int>(samples.size()));
    writer.reset(); // flushes the header before success is reported
    return written;
}

// Renders one hit per rate with `setUp` applied to a fresh processor first.
template <typename SetUp>
bool renderAll(const juce::File& dir, const std::string& name, const Rates& rates, SetUp&& setUp) {
    for (const auto& [rate, suffix] : rates) {
        DusterProcessor processor;
        setUp(processor);
        const auto file = dir.getChildFile(juce::String(name) + "_" + suffix + ".wav");
        if (!writeWav(file, duster::render::renderHit(processor, rate), rate)) {
            std::fprintf(stderr, "could not write %s\n", file.getFullPathName().toRawUTF8());
            return false;
        }
        std::printf("%s\n", file.getFullPathName().toRawUTF8());
    }
    return true;
}

int programNamed(const std::string& name) {
    const auto presets = duster::factoryPresets();
    for (std::size_t i = 0; i < presets.size(); ++i)
        if (presets[i].name == name)
            return static_cast<int>(i);
    return -1;
}

int listParams() {
    DusterProcessor processor;
    for (auto* parameter : processor.getParameters()) {
        auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(parameter);
        if (ranged == nullptr)
            continue;
        const auto& range = ranged->getNormalisableRange();
        const double step = range.interval;
        double fallback = ranged->convertFrom0to1(ranged->getDefaultValue());
        if (step > 0.0)
            fallback = std::round(fallback / step) * step + 0.0; // + 0.0 turns -0 into 0
        std::printf("%s\t%g\t%g\t%g\t%g\n", ranged->getParameterID().toRawUTF8(),
                    static_cast<double>(range.start), static_cast<double>(range.end), fallback, step);
    }
    return 0;
}

int renderSettingsFile(const juce::File& dir, const char* path, const Rates& rates) {
    std::ifstream file(path);
    if (!file) {
        std::fprintf(stderr, "cannot read %s\n", path);
        return 1;
    }
    std::vector<duster::render::SettingsLine> lines;
    std::string text;
    for (int number = 1; std::getline(file, text); ++number) {
        if (text.empty() || text[0] == '#')
            continue;
        auto line = duster::render::parseSettingsLine(text);
        DusterProcessor check;
        if (!line || !duster::render::applySettings(check, *line)) {
            std::fprintf(stderr, "%s:%d: malformed line or unknown parameter\n", path, number);
            return 1;
        }
        lines.push_back(std::move(*line));
    }
    for (const auto& line : lines)
        if (!renderAll(dir, line.name, rates,
                       [&](DusterProcessor& p) { duster::render::applySettings(p, line); }))
            return 1;
    return 0;
}
} // namespace

int main(int argc, char** argv) {
    const juce::ScopedJuceInitialiser_GUI juce;
    std::vector<std::string> args(argv + 1, argv + argc);
    for (const auto& arg : args)
        if (arg == "--list-params")
            return listParams();
    if (args.empty()) {
        std::fprintf(stderr, "usage: DusterRender <outDir> [presetName...] | <outDir> --settings <file> "
                             "[--rate 44100|48000] | --list-params\n");
        return 2;
    }
    const auto dir = juce::File::getCurrentWorkingDirectory().getChildFile(args[0]);
    dir.createDirectory();

    if (args.size() >= 3 && args[1] == "--settings") {
        Rates rates{{44100.0, "44k"}, {48000.0, "48k"}};
        if (args.size() == 5 && args[3] == "--rate") {
            if (args[4] != "44100" && args[4] != "48000") {
                std::fprintf(stderr, "--rate takes 44100 or 48000\n");
                return 2;
            }
            rates = args[4] == "44100" ? Rates{{44100.0, "44k"}} : Rates{{48000.0, "48k"}};
        }
        return renderSettingsFile(dir, args[2].c_str(), rates);
    }

    std::vector<std::string> names(args.begin() + 1, args.end());
    if (names.empty())
        for (const auto& preset : duster::factoryPresets())
            names.emplace_back(preset.name);
    for (const auto& name : names) {
        const int program = programNamed(name);
        if (program < 0) {
            std::fprintf(stderr, "no preset named \"%s\"\n", name.c_str());
            return 1;
        }
        if (!renderAll(dir, name, {{44100.0, "44k"}, {48000.0, "48k"}},
                       [&](DusterProcessor& p) { p.setCurrentProgram(program); }))
            return 1;
    }
    return 0;
}
