#include "Engine/DusterEngine.h"

#include <algorithm>

#include "Engine/EngineRate.h"

namespace duster {

namespace {
constexpr int kRampSamples = static_cast<int>(10e-3 * kEngineRate);
}

void DusterEngine::Ramp::setTarget(float value) noexcept {
    if (value == target_)
        return;
    target_ = value;
    remaining_ = kRampSamples;
    step_ = (target_ - current_) / static_cast<float>(kRampSamples);
}

float DusterEngine::Ramp::next() noexcept {
    if (remaining_ > 0)
        current_ = --remaining_ == 0 ? target_ : current_ + step_;
    return current_;
}

void DusterEngine::prepare() {
    rumble_.prepare();
    limiter_.reset();
}

void DusterEngine::setSettings(const KickSettings& settings) noexcept {
    settings_ = settings;
    if (!hasSettings_) { // the first settings apply at once: no ramp up from zero
        drive_.snap(settings.drive01);
        tone_.snap(settings.toneHz);
        ring_.snap(settings.ring01);
        amount_.snap(settings.rumbleAmount01);
        hasSettings_ = true;
    }
    drive_.setTarget(settings.drive01);
    tone_.setTarget(settings.toneHz);
    ring_.setTarget(settings.ring01);
    amount_.setTarget(settings.rumbleAmount01);
    rumble_.setMode(settings.rumbleMode);
    rumble_.setLength(settings.rumbleLengthMs);
    rumble_.setDivisionBeats(settings.rumbleDivisionBeats);
}

void DusterEngine::noteOn(int midiNote, float velocity01) noexcept {
    hasPendingNote_ = true; // a second note before the next sample replaces the first
    pendingNote_ = midiNote;
    pendingVelocity_ = velocity01;
}

void DusterEngine::startPendingNote() noexcept {
    hasPendingNote_ = false;
    // Prefer a free slot, then the one already fading out; never cut a voice at full level.
    auto slot = std::find_if(voices_.begin(), voices_.end(), [](const KickVoice& v) { return !v.active(); });
    if (slot == voices_.end())
        slot = std::find_if(voices_.begin(), voices_.end(), [](const KickVoice& v) { return v.fading(); });
    if (slot == voices_.end())
        slot = voices_.begin();
    for (auto& voice : voices_)
        voice.fadeOut();
    slot->start(settings_, pendingNote_, pendingVelocity_);
}

void DusterEngine::render(float* out, int numSamples) noexcept {
    if (hasPendingNote_)
        startPendingNote();
    for (int i = 0; i < numSamples; ++i) {
        const VoiceControls controls{drive_.next(),         tone_.next(),   ring_.next(),
                                     settings_.ringPitchHz, settings_.bits, settings_.holdFactor};
        float kick = 0.0f;
        float kickEnv = 0.0f;
        for (auto& voice : voices_) {
            kick += voice.next(controls);
            kickEnv = std::max(kickEnv, voice.envelope());
        }
        rumble_.setAmount(amount_.next());
        out[i] = kick + rumble_.process(kick, kickEnv);
    }
    limiter_.process(out, numSamples);
}

} // namespace duster
