#include "Engine/RingResonator.h"

#include <algorithm>
#include <cmath>

#include "Engine/EngineRate.h"

namespace duster {

void RingResonator::reset() noexcept {
    buffer_.fill(0.0f);
    write_ = 0;
    inputDc_.reset();
}

void RingResonator::setRing(float ring01, float pitchHz) noexcept {
    ring_ = ring01;
    feedback_ = 0.5f + 0.49f * ring01; // below 1 at every setting
    const auto delay = static_cast<float>(kEngineRate) / std::max(pitchHz, 1.0f);
    delaySamples_ = std::clamp(delay, 1.0f, static_cast<float>(kBufferSize - 2));
}

float RingResonator::process(float driven, bool strike, float env) noexcept {
    const float readPos = static_cast<float>(write_) - delaySamples_ + kBufferSize;
    const auto whole = static_cast<int>(readPos);
    const float frac = readPos - static_cast<float>(whole);
    const float a = buffer_[static_cast<std::size_t>(whole % kBufferSize)];
    const float b = buffer_[static_cast<std::size_t>((whole + 1) % kBufferSize)];
    const float delayed = a + frac * (b - a);

    const float excitation = inputDc_.process(driven + (strike ? env * ring_ : 0.0f));
    const float comb = excitation + feedback_ * delayed;
    buffer_[static_cast<std::size_t>(write_)] = comb;
    write_ = (write_ + 1) % kBufferSize;
    return driven + ring_ * comb;
}

} // namespace duster
