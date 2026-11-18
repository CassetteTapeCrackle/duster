#pragma once

#include <array>

#include "Engine/KickSettings.h"
#include "Engine/KickVoice.h"
#include "Engine/RumbleBus.h"
#include "Engine/SafetyLimiter.h"

namespace duster {

// The whole instrument at kEngineRate, mono: three voice slots (a retrigger crossfades the old hit out
// over 3 ms, and a fast roll can have two fading at once), the rumble bus fed by the kick, then the
// safety limiter.
class DusterEngine {
  public:
    // The last stage, after the limiter: a full-velocity hit peaks near -12 dBFS. Drive, crush and
    // the rumble all happen before it, so the trim changes the level and nothing else.
    static constexpr float kOutputGain = 0.31622777f; // -10 dB

    void prepare();                                          // allocates; message thread
    void setSettings(const KickSettings& settings) noexcept; // audio thread, once per block
    void setTempo(double bpm) noexcept { rumble_.setTempo(bpm); }
    void noteOn(float velocity01) noexcept; // takes effect at the next rendered sample
    void render(float* out, int numSamples) noexcept;

  private:
    // Linear 10 ms ramp toward the latest target, so automation never zippers.
    class Ramp {
      public:
        void snap(float value) noexcept { current_ = target_ = value, remaining_ = 0; }
        void setTarget(float value) noexcept;
        float next() noexcept;

      private:
        float current_ = 0.0f, target_ = 0.0f, step_ = 0.0f;
        int remaining_ = 0;
    };

    void startPendingNote() noexcept;

    std::array<KickVoice, 3> voices_;
    RumbleBus rumble_;
    SafetyLimiter limiter_;
    KickSettings settings_;
    Ramp drive_, tone_, ring_, amount_;
    bool hasSettings_ = false;
    bool hasPendingNote_ = false;
    float pendingVelocity_ = 0.0f;
};

} // namespace duster
