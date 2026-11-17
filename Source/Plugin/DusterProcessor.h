#pragma once

#include <atomic>
#include <cstdint>

#include <juce_audio_processors/juce_audio_processors.h>

#include "Engine/DusterEngine.h"
#include "Plugin/DusterParams.h"
#include "Plugin/HostRateResampler.h"

// The Duster instrument: MIDI in, the same kick on both output channels. The engine runs at 48 kHz
// and HostRateResampler brings it to the host rate. Note-offs are ignored: a hit always plays out.
class DusterProcessor final : public juce::AudioProcessor {
  public:
    DusterProcessor();

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override;
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    // The longest LENGTH plus the longest rumble tail.
    double getTailLengthSeconds() const override { return 3.5; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram_.load(); }
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& parameters() noexcept { return apvts_; }

    // For the editor (message thread). An audition plays the last note at full velocity on the next
    // block; the hit counter lets the page draw a playhead for every hit.
    void requestAudition() noexcept { auditionRequested_.store(true); }
    std::uint32_t hitCount() const noexcept { return hitCount_.load(); }
    int lastNote() const noexcept { return lastNote_.load(); }
    duster::KickSettings currentSettings() const noexcept { return reader_.read(); }

  private:
    double hostBpm() const noexcept;

    juce::AudioProcessorValueTreeState apvts_;
    duster::params::ParamReader reader_;
    duster::DusterEngine engine_;
    duster::HostRateResampler resampler_;
    std::atomic<int> currentProgram_{0};
    std::atomic<bool> auditionRequested_{false};
    std::atomic<std::uint32_t> hitCount_{0};
    std::atomic<int> lastNote_{30}; // F#0, Kick_05's note, until a note is played

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DusterProcessor)
};
