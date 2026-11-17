#include "Plugin/DusterProcessor.h"

DusterProcessor::DusterProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)) {}

void DusterProcessor::prepareToPlay(double, int) {}

bool DusterProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
    return layouts.getMainInputChannelSet().isDisabled() &&
           layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void DusterProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) {
    buffer.clear();
}

juce::AudioProcessorEditor* DusterProcessor::createEditor() {
    return new juce::GenericAudioProcessorEditor(*this);
}

const juce::String DusterProcessor::getName() const {
    return JucePlugin_Name;
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new DusterProcessor();
}
