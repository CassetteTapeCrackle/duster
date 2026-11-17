#include "Plugin/DusterProcessor.h"

#include <algorithm>
#include <string>

#include "Plugin/DusterEditor.h"
#include "Plugin/DusterPresets.h"

using namespace duster;

namespace {
const juce::Identifier kStateVersionId{"version"};
const juce::Identifier kProgramId{"program"};
constexpr int kStateVersion = 1;

// A preset change is a user gesture as far as the host is concerned (touch automation records it).
void setAsGesture(juce::RangedAudioParameter& p, float normalised) {
    p.beginChangeGesture();
    p.setValueNotifyingHost(normalised);
    p.endChangeGesture();
}
} // namespace

DusterProcessor::DusterProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts_(*this, nullptr, "DusterState", params::createLayout()), reader_(apvts_) {
    apvts_.state.setProperty(kStateVersionId, kStateVersion, nullptr);
}

void DusterProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
    engine_.prepare();
    engine_.setSettings(reader_.read());
    resampler_.prepare(sampleRate, samplesPerBlock);
    setLatencySamples(resampler_.latencyHostSamples());
}

bool DusterProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
    return layouts.getMainInputChannelSet().isDisabled() &&
           layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

double DusterProcessor::hostBpm() const noexcept {
    if (auto* playHead = getPlayHead())
        if (auto position = playHead->getPosition())
            if (auto bpm = position->getBpm())
                return *bpm;
    return 0.0; // the rumble falls back to 140 BPM
}

void DusterProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) {
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();
    buffer.clear();
    engine_.setSettings(reader_.read());
    engine_.setTempo(hostBpm());

    auto renderEngine = [this](float* dest, int count) { engine_.render(dest, count); };
    float* out = buffer.getWritePointer(0);
    if (auditionRequested_.exchange(false)) {
        resampler_.advanceToNow(renderEngine);
        engine_.noteOn(1.0f);
        hitCount_.fetch_add(1);
    }
    int done = 0;
    for (const auto event : midi) {
        const auto message = event.getMessage();
        if (!message.isNoteOn())
            continue;
        const int at = std::clamp(event.samplePosition, done, numSamples);
        resampler_.process(out + done, at - done, renderEngine);
        done = at;
        resampler_.advanceToNow(renderEngine);
        engine_.noteOn(message.getFloatVelocity());
        hitCount_.fetch_add(1);
    }
    resampler_.process(out + done, numSamples - done, renderEngine);
    if (buffer.getNumChannels() > 1)
        buffer.copyFrom(1, 0, buffer, 0, 0, numSamples);
}

juce::AudioProcessorEditor* DusterProcessor::createEditor() {
    return new DusterEditor(*this);
}

const juce::String DusterProcessor::getName() const {
    return JucePlugin_Name;
}

int DusterProcessor::getNumPrograms() {
    return static_cast<int>(factoryPresets().size());
}

const juce::String DusterProcessor::getProgramName(int index) {
    if (index < 0 || index >= getNumPrograms())
        return {};
    return std::string(factoryPresets()[static_cast<std::size_t>(index)].name);
}

void DusterProcessor::setCurrentProgram(int index) {
    if (index < 0 || index >= getNumPrograms())
        return;
    currentProgram_.store(index);
    apvts_.state.setProperty(kProgramId, index, nullptr);
    for (const auto& [id, value] : factoryPresets()[static_cast<std::size_t>(index)].values)
        if (auto* p = apvts_.getParameter(juce::String(std::string(id))))
            setAsGesture(*p, p->convertTo0to1(value));
    updateHostDisplay(ChangeDetails{}.withProgramChanged(true));
}

void DusterProcessor::getStateInformation(juce::MemoryBlock& destData) {
    if (auto xml = apvts_.copyState().createXml())
        copyXmlToBinary(*xml, destData);
}

void DusterProcessor::setStateInformation(const void* data, int sizeInBytes) {
    if (auto xml = getXmlFromBinary(data, sizeInBytes))
        if (xml->hasTagName(apvts_.state.getType())) {
            apvts_.replaceState(juce::ValueTree::fromXml(*xml));
            // Restore the preset index without re-applying the preset (that would overwrite the
            // saved parameter values when a host restores its Program parameter).
            currentProgram_.store(std::clamp(static_cast<int>(apvts_.state.getProperty(kProgramId, 0)), 0,
                                             getNumPrograms() - 1));
            apvts_.state.setProperty(kStateVersionId, kStateVersion, nullptr);
        }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new DusterProcessor();
}
