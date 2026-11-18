#include "Engine/KickOscillator.h"

#include <algorithm>
#include <cmath>
#include <numbers>

#include "Engine/EngineRate.h"

namespace duster {

namespace {
constexpr float kFastShare = 0.85f;
constexpr float kSlowShare = 0.15f;
constexpr float kSlowSnapRatio = 5.0f;
constexpr float kMaxHz = 20000.0f;
constexpr float kMinExponent = -60.0f;
} // namespace

float sweepSemitones(float tMs, float sweepSt, float snapMs) noexcept {
    // Exponents stop at -60 (2^-60 st is inaudible) so long notes never hit slow subnormals.
    const float fast = std::exp2(std::max(-tMs / snapMs, kMinExponent));
    const float slow = std::exp2(std::max(-tMs / (kSlowSnapRatio * snapMs), kMinExponent));
    return sweepSt * (kFastShare * fast + kSlowShare * slow);
}

void KickOscillator::start(float noteHz, float sweepSt, float snapMs) noexcept {
    *this = KickOscillator{};
    noteHz_ = noteHz;
    sweepSt_ = sweepSt;
    snapMs_ = snapMs;
}

float KickOscillator::next() noexcept {
    const auto tMs = static_cast<float>(static_cast<double>(sample_) * 1000.0 / kEngineRate);
    ++sample_;
    hz_ = std::min(noteHz_ * std::exp2(sweepSemitones(tMs, sweepSt_, snapMs_) / 12.0f), kMaxHz);

    // -sin so the first swing goes negative, like Kick_05.
    const auto out = static_cast<float>(-std::sin(2.0 * std::numbers::pi * phase_));
    phase_ += hz_ / kEngineRate;
    phase_ -= std::floor(phase_);

    crossedUp_ = last_ < 0.0f && out >= 0.0f;
    last_ = out;
    return out;
}

} // namespace duster
