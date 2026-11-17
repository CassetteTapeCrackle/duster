#include "Engine/KickVoice.h"

#include <cmath>

#include "Engine/AmpShape.h"
#include "Engine/Drive.h"
#include "Engine/EngineRate.h"

namespace duster {

namespace {
constexpr float kVoiceLevel = 0.8f; // a full-velocity hit peaks under the limiter's ceiling
constexpr float kFadeStep = 1.0f / (3e-3f * static_cast<float>(kEngineRate));
constexpr float kToneQ = 0.7071f;

float noteHz(int midiNote, float tuneSt) noexcept {
    return 440.0f * std::exp2((static_cast<float>(midiNote) - 69.0f + tuneSt) / 12.0f);
}
} // namespace

void KickVoice::start(const KickSettings& settings, int midiNote, float velocity01) noexcept {
    osc_.start(noteHz(midiNote, settings.tuneSt), settings.sweepSt, settings.snapMs);
    driveDc_.reset();
    ring_.reset();
    crusher_.reset();
    tone_.reset();
    toneHz_ = 0.0f; // forces the filter to be set on the first sample
    sample_ = 0;
    lengthMs_ = settings.lengthMs;
    velocity_ = velocity01;
    fade_ = 1.0f;
    active_ = true;
    fading_ = false;
}

void KickVoice::stop() noexcept {
    active_ = false;
    fading_ = false;
    envelope_ = 0.0f;
}

float KickVoice::next(const VoiceControls& controls) noexcept {
    if (!active_)
        return 0.0f;
    const auto tMs = static_cast<float>(static_cast<double>(sample_++) * 1000.0 / kEngineRate);
    if (tMs >= lengthMs_) {
        stop();
        return 0.0f;
    }
    if (fading_) {
        fade_ -= kFadeStep;
        if (fade_ <= 0.0f) {
            stop();
            return 0.0f;
        }
    }

    if (controls.toneHz != toneHz_) {
        tone_.setLowPass(static_cast<float>(kEngineRate), controls.toneHz, kToneQ);
        toneHz_ = controls.toneHz;
    }
    ring_.setRing(controls.ring01, controls.ringPitchHz);
    crusher_.setBits(controls.bits);
    crusher_.setHoldFactor(controls.holdFactor);

    const float shape = ampShape(tMs, lengthMs_);
    float x = driveDc_.process(driveSample(osc_.next(), controls.drive01));
    x = ring_.process(x, osc_.crossedZeroUpward(), shape);
    x = tone_.process(crusher_.process(x));

    envelope_ = shape * velocity_ * fade_;
    return x * envelope_ * kVoiceLevel;
}

} // namespace duster
