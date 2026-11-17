// Renders factory presets through DusterProcessor (resampler included) to 24-bit mono WAVs for
// tuning against the sample pack:
//
//   DusterRender <outDir> [presetName...]      (every preset when none is named)
//
// Writes <outDir>/<preset>_44k.wav and <preset>_48k.wav: note 30 (F#0) at velocity 1, one second,
// latency-compensated so the hit starts at sample 0.
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_events/juce_events.h>

#include "Plugin/DusterPresets.h"
#include "Plugin/DusterProcessor.h"

namespace {
constexpr int kNote = 30;
constexpr int kBlock = 512;

std::vector<float> renderHit(int program, double rate) {
    DusterProcessor processor;
    processor.setCurrentProgram(program);
    processor.prepareToPlay(rate, kBlock);
    const int latency = processor.getLatencySamples();
    const int total = static_cast<int>(rate) + latency;
    std::vector<float> out;
    out.reserve(static_cast<std::size_t>(total));
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

bool writeWav(const juce::File& file, const std::vector<float>& samples, double rate) {
    file.deleteFile();
    std::unique_ptr<juce::OutputStream> stream = file.createOutputStream();
    if (stream == nullptr)
        return false;
    juce::WavAudioFormat wav;
    const auto options =
        juce::AudioFormatWriterOptions{}.withSampleRate(rate).withNumChannels(1).withBitsPerSample(24);
    const auto writer = wav.createWriterFor(stream, options); // takes the stream on success
    if (writer == nullptr)
        return false;
    const float* channels[] = {samples.data()};
    return writer->writeFromFloatArrays(channels, 1, static_cast<int>(samples.size()));
}

int programNamed(const std::string& name) {
    const auto presets = duster::factoryPresets();
    for (std::size_t i = 0; i < presets.size(); ++i)
        if (presets[i].name == name)
            return static_cast<int>(i);
    return -1;
}
} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: DusterRender <outDir> [presetName...]\n");
        return 2;
    }
    const juce::ScopedJuceInitialiser_GUI juce;
    const auto dir = juce::File::getCurrentWorkingDirectory().getChildFile(argv[1]);
    dir.createDirectory();

    std::vector<std::string> names(argv + 2, argv + argc);
    if (names.empty())
        for (const auto& preset : duster::factoryPresets())
            names.emplace_back(preset.name);

    for (const auto& name : names) {
        const int program = programNamed(name);
        if (program < 0) {
            std::fprintf(stderr, "no preset named \"%s\"\n", name.c_str());
            return 1;
        }
        for (const auto& [rate, suffix] : {std::pair{44100.0, "44k"}, std::pair{48000.0, "48k"}}) {
            const auto file = dir.getChildFile(juce::String(name) + "_" + suffix + ".wav");
            if (!writeWav(file, renderHit(program, rate), rate)) {
                std::fprintf(stderr, "could not write %s\n", file.getFullPathName().toRawUTF8());
                return 1;
            }
            std::printf("%s\n", file.getFullPathName().toRawUTF8());
        }
    }
    return 0;
}
