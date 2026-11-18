#pragma once

#include <cstdint>

namespace duster {

// Semitones above the note at tMs after the hit. Two stages fitted to Kick_05 (within 2.6 %): a fast
// drop of 85 % of the sweep with half-life snapMs, and a slow 15 % tail with half-life 5 * snapMs.
float sweepSemitones(float tMs, float sweepSt, float snapMs) noexcept;

// The kick's body: a sine whose pitch falls from note + sweep to the note, at kEngineRate.
class KickOscillator {
  public:
    void start(float noteHz, float sweepSt, float snapMs) noexcept;
    float next() noexcept;
    float currentHz() const noexcept { return hz_; }
    // True when the last sample crossed zero going up: strikes the ring once per body cycle.
    bool crossedZeroUpward() const noexcept { return crossedUp_; }

  private:
    double phase_ = 0.0; // in cycles, [0, 1)
    std::int64_t sample_ = 0;
    float noteHz_ = 0.0f;
    float sweepSt_ = 0.0f;
    float snapMs_ = 1.0f;
    float hz_ = 0.0f;
    float last_ = 0.0f;
    bool crossedUp_ = false;
};

} // namespace duster
