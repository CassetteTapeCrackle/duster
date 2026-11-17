#pragma once

namespace duster {
// The engine always renders at 48 kHz: the crusher's whole-number hold (48000 / 5 = 9600 Hz for
// Kick_05) only lands on the right frequencies at this rate. The plugin resamples to the host rate.
inline constexpr double kEngineRate = 48000.0;
} // namespace duster
