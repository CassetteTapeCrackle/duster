#pragma once

#include <vector>

#include "Engine/KickSettings.h"

namespace duster {

// One full-velocity hit at kEngineRate for the UI's waveform, through its own engine so it shares no
// state with the audio thread. Allocates: call on the message thread. Runs LENGTH, plus the rumble's
// LENGTH when the rumble is on.
std::vector<float> renderPreviewHit(const KickSettings& settings);

} // namespace duster
