#include "Plugin/DusterEditor.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <string>

#include "BinaryData.h"
#include "DusterWebPaths.h"
#include "Engine/EngineRate.h"
#include "Engine/KickOscillator.h"
#include "Engine/PreviewRender.h"
#include "Plugin/DusterParams.h"
#include "Plugin/DusterPresets.h"
#include "Plugin/DusterProcessor.h"

namespace {
constexpr int kMinWidth = 560, kMinHeight = 340, kMaxWidth = 1120, kMaxHeight = 680;
constexpr int kPreviewColumns = 400;
constexpr std::uint32_t kPreviewEveryMs = 50; // at most this often while a knob moves

// The page's knobs, in Source/Web/format.js order.
constexpr std::array kSliderIds = {
    duster::params::kTune,      duster::params::kSweep,        duster::params::kSnap,
    duster::params::kLength,    duster::params::kDrive,        duster::params::kBits,
    duster::params::kRate,      duster::params::kTone,         duster::params::kRing,
    duster::params::kRingPitch, duster::params::kRumbleAmount, duster::params::kRumbleLength};

const char* mimeFor(const juce::String& filename) {
    const auto ext = filename.fromLastOccurrenceOf(".", false, false).toLowerCase();
    if (ext == "html")
        return "text/html";
    if (ext == "js" || ext == "mjs")
        return "text/javascript";
    if (ext == "css")
        return "text/css";
    if (ext == "woff2")
        return "font/woff2";
    if (ext == "txt")
        return "text/plain";
    return "application/octet-stream";
}

std::vector<std::unique_ptr<juce::WebSliderRelay>> makeSliderRelays() {
    std::vector<std::unique_ptr<juce::WebSliderRelay>> relays;
    for (const char* id : kSliderIds)
        relays.push_back(std::make_unique<juce::WebSliderRelay>(id));
    return relays;
}

juce::RangedAudioParameter& param(DusterProcessor& p, const char* id) {
    return *p.parameters().getParameter(id);
}
} // namespace

std::optional<juce::WebBrowserComponent::Resource> DusterEditor::serveWebResource(const juce::String& url) {
    auto path = juce::URL::removeEscapeChars(
        url.upToFirstOccurrenceOf("?", false, false).upToFirstOccurrenceOf("#", false, false));
    path = path.trimCharactersAtStart("/");
    if (path.isEmpty())
        path = "index.html";

    for (const auto& file : duster::web::kWebFiles) {
        if (path != file.path)
            continue;
        for (int i = 0; i < BinaryData::namedResourceListSize; ++i) {
            const char* name = BinaryData::namedResourceList[i];
            if (juce::String(file.filename) != BinaryData::getNamedResourceOriginalFilename(name))
                continue;
            int size = 0;
            const char* data = BinaryData::getNamedResource(name, size);
            if (data == nullptr)
                return std::nullopt;
            std::vector<std::byte> bytes(static_cast<size_t>(size));
            std::memcpy(bytes.data(), data, static_cast<size_t>(size));
            return juce::WebBrowserComponent::Resource{std::move(bytes), mimeFor(file.filename)};
        }
    }
    return std::nullopt;
}

juce::StringArray DusterEditor::webResourcePaths() {
    juce::StringArray paths;
    for (const auto& file : duster::web::kWebFiles)
        paths.add(file.path);
    return paths;
}

juce::var DusterEditor::previewEvent(const duster::KickSettings& settings, int midiNote) {
    const juce::ScopedNoDenormals noDenormals; // the render's tails decay into denormals off the audio thread
    const auto hit = duster::renderPreviewHit(settings, midiNote);
    const double spanMs = static_cast<double>(hit.size()) * 1000.0 / duster::kEngineRate;
    juce::Array<juce::var> lows, highs, pitchSt;
    for (int c = 0; c < kPreviewColumns; ++c) {
        const auto from = hit.size() * static_cast<std::size_t>(c) / kPreviewColumns;
        const auto to = std::max(from + 1, hit.size() * static_cast<std::size_t>(c + 1) / kPreviewColumns);
        const auto [low, high] =
            std::minmax_element(hit.begin() + static_cast<std::ptrdiff_t>(from),
                                hit.begin() + static_cast<std::ptrdiff_t>(std::min(to, hit.size())));
        lows.add(*low);
        highs.add(*high);
        const auto tMs = static_cast<float>((c + 0.5) * spanMs / kPreviewColumns);
        pitchSt.add(settings.tuneSt + duster::sweepSemitones(tMs, settings.sweepSt, settings.snapMs));
    }
    auto* event = new juce::DynamicObject();
    event->setProperty("lows", lows);
    event->setProperty("highs", highs);
    event->setProperty("pitchSt", pitchSt);
    event->setProperty("spanMs", spanMs);
    event->setProperty("noteHz", 440.0 * std::exp2((midiNote - 69) / 12.0));
    return juce::var(event);
}

juce::WebBrowserComponent::Options DusterEditor::browserOptions() {
    auto options = juce::WebBrowserComponent::Options{}.withNativeIntegrationEnabled();
    for (const auto& relay : sliderRelays_)
        options = options.withOptionsFrom(*relay);
    return options.withOptionsFrom(rumbleModeRelay_)
        .withOptionsFrom(divisionRelay_)
        .withNativeFunction("getPresets",
                            [this](const juce::Array<juce::var>&, auto complete) {
                                juce::Array<juce::var> names;
                                for (const auto& preset : duster::factoryPresets())
                                    names.add(juce::String(std::string(preset.name)));
                                auto* o = new juce::DynamicObject();
                                o->setProperty("names", names);
                                o->setProperty("current", processor_.getCurrentProgram());
                                complete(juce::var(o));
                            })
        .withNativeFunction(
            "setPreset",
            [this](const juce::Array<juce::var>& args, auto complete) {
                if (!args.isEmpty() && (args[0].isInt() || args[0].isDouble()))
                    processor_.setCurrentProgram(static_cast<int>(args[0]));
                complete(processor_.getCurrentProgram()); // the page trusts this, not its own guess
            })
        .withNativeFunction("getPreview",
                            [this](const juce::Array<juce::var>&, auto complete) { complete(preview_); })
        .withNativeFunction("audition",
                            [this](const juce::Array<juce::var>&, auto complete) {
                                processor_.requestAudition();
                                complete(juce::var());
                            })
        .withNativeFunction("resizeTo",
                            [this](const juce::Array<juce::var>& args, auto complete) {
                                // The page draws its own resize grip: the native corner resizer would
                                // sit under the WebView and never receive the mouse.
                                if (args.size() >= 2)
                                    setSize(juce::jlimit(kMinWidth, kMaxWidth, static_cast<int>(args[0])),
                                            juce::jlimit(kMinHeight, kMaxHeight, static_cast<int>(args[1])));
                                complete(juce::var());
                            })
        .withInitialisationData("sizeLimits",
                                juce::Array<juce::var>{kMinWidth, kMinHeight, kMaxWidth, kMaxHeight})
        .withResourceProvider([](const auto& url) { return serveWebResource(url); });
}

DusterEditor::DusterEditor(DusterProcessor& owner)
    : AudioProcessorEditor(owner), processor_(owner), sliderRelays_(makeSliderRelays()),
      browser_(browserOptions()),
      rumbleModeAttachment_(param(owner, duster::params::kRumbleMode), rumbleModeRelay_),
      divisionAttachment_(param(owner, duster::params::kRumbleDivision), divisionRelay_) {
    for (std::size_t i = 0; i < kSliderIds.size(); ++i)
        sliderAttachments_.push_back(std::make_unique<juce::WebSliderParameterAttachment>(
            param(owner, kSliderIds[i]), *sliderRelays_[i]));
    refreshPreview();
    lastHitCount_ = processor_.hitCount();
    owner.addListener(this);
    addAndMakeVisible(browser_);
    browser_.goToURL(juce::WebBrowserComponent::getResourceProviderRoot());
    setResizable(true, false); // host edges resize; the page's grip calls resizeTo
    setResizeLimits(kMinWidth, kMinHeight, kMaxWidth, kMaxHeight);
    setSize(kMinWidth, kMinHeight);
    startTimerHz(30);
}

DusterEditor::~DusterEditor() {
    processor_.removeListener(this);
    stopTimer();
}

void DusterEditor::resized() {
    browser_.setBounds(getLocalBounds());
}

void DusterEditor::refreshPreview() {
    previewStale_.store(false);
    previewNote_ = processor_.lastNote();
    preview_ = previewEvent(processor_.currentSettings(), previewNote_);
    lastPreviewMs_ = juce::Time::getMillisecondCounter();
}

void DusterEditor::timerCallback() {
    // The preset can change while the host is not processing (stopped, bypassed): send it on its own.
    if (const int program = processor_.getCurrentProgram(); program != lastProgram_) {
        lastProgram_ = program;
        auto* o = new juce::DynamicObject();
        o->setProperty("program", program);
        browser_.emitEventIfBrowserIsVisible("program", juce::var(o));
    }
    if (const auto hits = processor_.hitCount(); hits != lastHitCount_) {
        lastHitCount_ = hits;
        browser_.emitEventIfBrowserIsVisible("hit", juce::var());
    }
    const bool changed = previewStale_.load() || processor_.lastNote() != previewNote_;
    // Hidden windows skip the render; the stale flag catches the display up when it shows again.
    if (changed && isShowing() && juce::Time::getMillisecondCounter() - lastPreviewMs_ >= kPreviewEveryMs) {
        refreshPreview();
        browser_.emitEventIfBrowserIsVisible("preview", preview_);
    }
}
