#include "Engine/PreviewRender.h"

#include "Engine/DusterEngine.h"
#include "Engine/EngineRate.h"

namespace duster {

std::vector<float> renderPreviewHit(const KickSettings& settings, int midiNote) {
    const float rumbleMs = settings.rumbleMode == RumbleMode::Off ? 0.0f : settings.rumbleLengthMs;
    std::vector<float> out(static_cast<std::size_t>((settings.lengthMs + rumbleMs) * 1e-3f * kEngineRate));
    DusterEngine engine;
    engine.prepare();
    engine.setSettings(settings);
    engine.noteOn(midiNote, 1.0f);
    engine.render(out.data(), static_cast<int>(out.size()));
    return out;
}

} // namespace duster
