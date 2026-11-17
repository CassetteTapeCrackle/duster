#pragma once

namespace duster {

// Last thing in the engine (mono, from Kikrame). Instant attack, 80 ms release, hard clamp at
// -1 dBFS as a guarantee, NaN and Inf replaced with silence.
class SafetyLimiter {
  public:
    static constexpr float kCeiling = 0.8912509f; // -1 dBFS

    void reset() noexcept { gain_ = 1.0f; }
    void process(float* samples, int numSamples) noexcept;

  private:
    float gain_ = 1.0f;
};

} // namespace duster
