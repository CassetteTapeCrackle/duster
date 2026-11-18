#include "Engine/RumbleBus.h"

#include <algorithm>
#include <cmath>

#include "Engine/EngineRate.h"

namespace duster {

namespace {
constexpr std::array<std::size_t, 4> kRoomDelays = {1153, 1559, 1907, 2341}; // 24 to 49 ms, coprime
constexpr double kDefaultBpm = 140.0, kMinBpm = 40.0, kMaxBpm = 300.0;
constexpr float kMinDivisionBeats = 0.125f, kMaxDivisionBeats = 1.0f;
constexpr float kMinLengthMs = 100.0f, kMaxLengthMs = 2000.0f;
constexpr float kDrive = 2.0f;
constexpr float kLowPassHz = 250.0f;
constexpr float kDuckDepth = 0.8f; // -14 dB under a full kick
constexpr float kSilence = 1e-6f;  // -120 dB: below this the tail is gone

// Gain per pass so a loop of delaySamples decays 60 dB in lengthMs.
float decayGain(std::size_t delaySamples, float lengthMs) noexcept {
    const double passes =
        static_cast<double>(lengthMs) * 1e-3 * kEngineRate / static_cast<double>(delaySamples);
    return static_cast<float>(std::pow(10.0, -3.0 / passes));
}
} // namespace

void RumbleBus::prepare() {
    for (std::size_t i = 0; i < room_.size(); ++i)
        room_[i].buffer.assign(kRoomDelays[i] + 1, 0.0f);
    const auto longestTap = static_cast<std::size_t>(kMaxDivisionBeats * 60.0 / kMinBpm * kEngineRate);
    taps_.buffer.assign(longestTap + 1, 0.0f);
    // Butterworth 4-pole as two biquads.
    lowPassA_.setLowPass(static_cast<float>(kEngineRate), kLowPassHz, 0.5412f);
    lowPassB_.setLowPass(static_cast<float>(kEngineRate), kLowPassHz, 1.3066f);
    updateDecay();
    reset();
}

void RumbleBus::reset() noexcept {
    clearState();
    idle_ = true;
}

void RumbleBus::clearState() noexcept {
    for (auto& line : room_)
        std::fill(line.buffer.begin(), line.buffer.end(), 0.0f);
    std::fill(taps_.buffer.begin(), taps_.buffer.end(), 0.0f);
    lowPassA_.reset();
    lowPassB_.reset();
    quietSamples_ = 0;
}

void RumbleBus::setMode(RumbleMode mode) noexcept {
    if (mode != mode_)
        reset(); // no stale tail from the other mode
    mode_ = mode;
}

void RumbleBus::setLength(float lengthMs) noexcept {
    lengthMs = std::clamp(lengthMs, kMinLengthMs, kMaxLengthMs);
    if (lengthMs == lengthMs_)
        return; // the plugin calls this every block; skip the pow() work
    lengthMs_ = lengthMs;
    updateDecay();
}

void RumbleBus::setTempo(double bpm) noexcept {
    bpm = bpm > 0.0 ? std::clamp(bpm, kMinBpm, kMaxBpm) : kDefaultBpm;
    if (bpm == bpm_)
        return;
    bpm_ = bpm;
    updateDecay();
}

void RumbleBus::setDivisionBeats(float beats) noexcept {
    beats = std::clamp(beats, kMinDivisionBeats, kMaxDivisionBeats);
    if (beats == divisionBeats_)
        return;
    divisionBeats_ = beats;
    updateDecay();
}

void RumbleBus::updateDecay() noexcept {
    for (std::size_t i = 0; i < room_.size(); ++i)
        roomGain_[i] = decayGain(kRoomDelays[i], lengthMs_);
    const double tapSamples = static_cast<double>(divisionBeats_) * 60.0 / bpm_ * kEngineRate;
    const std::size_t maxTap = taps_.buffer.empty() ? 1 : taps_.buffer.size() - 1;
    tapsDelay_ = std::clamp(static_cast<std::size_t>(std::lround(tapSamples)), std::size_t{1}, maxTap);
    tapsFeedback_ = decayGain(tapsDelay_, lengthMs_);
}

float RumbleBus::processRoom(float kick) noexcept {
    std::array<float, 4> out{};
    for (std::size_t i = 0; i < room_.size(); ++i)
        out[i] = roomGain_[i] * room_[i].read(kRoomDelays[i]);
    // Hadamard mixing keeps the loop lossless before the decay gains.
    const float a = out[0], b = out[1], c = out[2], d = out[3];
    const std::array<float, 4> mixed = {0.5f * (a + b + c + d), 0.5f * (a - b + c - d),
                                        0.5f * (a + b - c - d), 0.5f * (a - b - c + d)};
    for (std::size_t i = 0; i < room_.size(); ++i)
        room_[i].push(0.5f * kick + mixed[i]);
    return 0.5f * (a + b + c + d);
}

float RumbleBus::processTaps(float kick) noexcept {
    const float delayed = taps_.read(tapsDelay_);
    taps_.push(kick + tapsFeedback_ * delayed);
    return delayed;
}

float RumbleBus::process(float kick, float kickEnv) noexcept {
    if (mode_ == RumbleMode::Off || (idle_ && kick == 0.0f))
        return 0.0f;
    idle_ = false;

    const float wet = mode_ == RumbleMode::Room ? processRoom(kick) : processTaps(kick);
    const bool quiet = std::abs(wet) < kSilence && std::abs(kick) < kSilence;
    quietSamples_ = quiet ? quietSamples_ + 1 : 0;
    if (quietSamples_ > std::max(kRoomDelays.back(), tapsDelay_)) { // every line holds only silence
        reset();
        return 0.0f;
    }

    float y = lowPassB_.process(lowPassA_.process(std::tanh(kDrive * wet) / kDrive));
    if (mode_ == RumbleMode::Taps)
        y *= 1.0f - kDuckDepth * kickEnv;
    return amount_ * y;
}

} // namespace duster
