#pragma once

#include <cstdint>

#include "Engine/Crusher.h"
#include "Engine/KickOscillator.h"
#include "Engine/KickSettings.h"
#include "Engine/RingResonator.h"
#include "Primitives/Biquad.h"
#include "Primitives/DcBlocker.h"

namespace duster {

// The knobs that may move during a hit, already smoothed by the engine.
struct VoiceControls {
    float drive01;
    float toneHz;
    float ring01;
    float ringPitchHz;
    int bits;
    int holdFactor;
};

// One hit: oscillator -> drive -> ring -> crusher -> tone, times the amp shape and velocity.
// SWEEP, SNAP, LENGTH and TUNE are latched at start, so every hit with the same settings is identical.
class KickVoice {
  public:
    void start(const KickSettings& settings, float velocity01) noexcept;
    void fadeOut() noexcept { fading_ = active_; } // 3 ms, for a retrigger
    bool active() const noexcept { return active_; }
    bool fading() const noexcept { return fading_; }
    // The level of the last sample's envelope (amp shape x velocity x fade), for rumble ducking.
    float envelope() const noexcept { return envelope_; }
    float next(const VoiceControls& controls) noexcept;

  private:
    void stop() noexcept;

    KickOscillator osc_;
    DcBlocker driveDc_{10.0}; // the biased drive's DC; 10 Hz keeps sub notes intact
    RingResonator ring_;
    Crusher crusher_;
    Biquad tone_;
    float toneHz_ = 0.0f;
    std::int64_t sample_ = 0;
    float lengthMs_ = 0.0f;
    float velocity_ = 0.0f;
    float fade_ = 1.0f;
    float envelope_ = 0.0f;
    bool active_ = false;
    bool fading_ = false;
};

} // namespace duster
