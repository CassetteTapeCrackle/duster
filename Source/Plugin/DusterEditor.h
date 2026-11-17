#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>

#include "Engine/KickSettings.h"

class DusterProcessor;

// The plugin window: a WebView showing the embedded UI in Source/Web, bound to the parameters
// through JUCE's web relays. A 30 Hz timer sends the page preset changes, a "hit" for every note
// (the playhead) and, when the sound changes, a fresh preview of the hit for the display.
class DusterEditor final : public juce::AudioProcessorEditor,
                           private juce::Timer,
                           private juce::AudioProcessorListener {
  public:
    explicit DusterEditor(DusterProcessor& owner);
    ~DusterEditor() override;

    void resized() override;

    // Maps a request path ("/", "/main.js", "/juce/index.js") to an embedded file. Public for tests.
    static std::optional<juce::WebBrowserComponent::Resource> serveWebResource(const juce::String& url);
    // Every embedded file's request path, for tests.
    static juce::StringArray webResourcePaths();
    // One hit for the display: the lowest and highest sample of each of 400 columns, the pitch in
    // semitones above the note per column,
    // the span in ms and the note's frequency. Renders offline: message thread only.
    static juce::var previewEvent(const duster::KickSettings& settings, int midiNote);

  private:
    void timerCallback() override;
    // Any thread (automation arrives on the audio thread): only marks the preview stale.
    void audioProcessorParameterChanged(juce::AudioProcessor*, int, float) override {
        previewStale_.store(true);
    }
    void audioProcessorChanged(juce::AudioProcessor*, const ChangeDetails&) override {}
    void refreshPreview();
    juce::WebBrowserComponent::Options browserOptions();

    DusterProcessor& processor_;

    // Relays must exist before the browser (their options are baked into it) and outlive it.
    std::vector<std::unique_ptr<juce::WebSliderRelay>> sliderRelays_;
    juce::WebComboBoxRelay rumbleModeRelay_{"rumbleMode"};
    juce::WebComboBoxRelay divisionRelay_{"rumbleDivision"};

    juce::WebBrowserComponent browser_;

    // Attachments come after the browser: they push initial values into it.
    std::vector<std::unique_ptr<juce::WebSliderParameterAttachment>> sliderAttachments_;
    juce::WebComboBoxParameterAttachment rumbleModeAttachment_, divisionAttachment_;

    juce::var preview_;
    std::atomic<bool> previewStale_{false};
    int previewNote_ = -1;
    std::uint32_t lastPreviewMs_ = 0;
    std::uint32_t lastHitCount_ = 0;
    int lastProgram_ = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DusterEditor)
};
