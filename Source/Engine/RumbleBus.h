#pragma once

#include <array>
#include <vector>

#include "Primitives/Biquad.h"

namespace duster {

enum class RumbleMode { Off, Room, Taps };

// The rumble, a parallel bus fed by the finished kick. ROOM is a short dense reverb; TAPS is a feedback
// delay synced to the host tempo (140 BPM without one), ducked under the kick. Both then go through a
// fixed soft drive and a 4-pole low-pass at 250 Hz. LENGTH is the tail's -60 dB time. Returns the
// rumble only; the caller adds it.
class RumbleBus {
  public:
    void prepare(); // allocates the delay lines; call off the audio thread
    void reset() noexcept;
    void setMode(RumbleMode mode) noexcept;
    void setAmount(float amount01) noexcept { amount_ = amount01; }
    void setLength(float lengthMs) noexcept;
    void setTempo(double bpm) noexcept;          // <= 0 means unknown: 140 BPM
    void setDivisionBeats(float beats) noexcept; // TAPS spacing in quarter notes: 0.25 is 1/16
    float process(float kick, float kickEnv) noexcept;

  private:
    struct DelayLine {
        std::vector<float> buffer;
        std::size_t write = 0;
        float read(std::size_t delay) const noexcept {
            return buffer[(write + buffer.size() - delay) % buffer.size()];
        }
        void push(float x) noexcept {
            buffer[write] = x;
            write = (write + 1) % buffer.size();
        }
    };

    float processRoom(float kick) noexcept;
    float processTaps(float kick) noexcept;
    void updateDecay() noexcept;
    void clearState() noexcept;

    std::array<DelayLine, 4> room_;
    std::array<float, 4> roomGain_{};
    DelayLine taps_;
    std::size_t tapsDelay_ = 1;
    float tapsFeedback_ = 0.0f;
    Biquad lowPassA_, lowPassB_;

    RumbleMode mode_ = RumbleMode::Off;
    float amount_ = 0.5f;
    float lengthMs_ = 600.0f;
    double bpm_ = 140.0;
    float divisionBeats_ = 0.25f;
    std::size_t quietSamples_ = 0;
    bool idle_ = true;
};

} // namespace duster
