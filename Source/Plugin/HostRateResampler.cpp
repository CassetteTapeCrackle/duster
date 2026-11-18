#include "Plugin/HostRateResampler.h"

#include "Engine/EngineRate.h"

namespace duster {

namespace {
constexpr double kSincLatency =
    100.0; // juce::WindowedSincInterpolator's algorithmic latency, in input samples
}

void HostRateResampler::prepare(double hostRate, int maxHostBlock) {
    maxHostBlock_ = std::max(1, maxHostBlock);
    ratio_ = kEngineRate / hostRate;
    passthrough_ = std::abs(hostRate - kEngineRate) < 0.5;
    interpolator_.reset();
    const auto capacity = static_cast<std::size_t>(std::ceil(maxHostBlock_ * ratio_)) + 4 * kLead + 64;
    fifo_.assign(capacity, 0.0f);
    read_ = 0;
    write_ = passthrough_ ? 0 : kLead; // the lead starts as silence
    engineRendered_ = 0;
    hostDone_ = 0;
    latency_ = passthrough_ ? 0 : static_cast<int>(std::lround((kSincLatency + kLead) / ratio_));
}

} // namespace duster
