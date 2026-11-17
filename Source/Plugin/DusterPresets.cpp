#include "Plugin/DusterPresets.h"

namespace duster {

namespace {
// Order: tune, sweep, snap, length, drive, bits, rate, tone, ring, ringPitch,
// rumbleMode, rumbleAmount, rumbleLength, rumbleDivision.
constexpr DusterPreset preset(std::string_view name, float tune, float sweep, float snap, float length,
                              float drive, float bits, float rate, float tone, float ring, float ringPitch,
                              float rumbleMode, float rumbleAmount, float rumbleLength,
                              float rumbleDivision) {
    return {name,
            {{{"tune", tune},
              {"sweep", sweep},
              {"snap", snap},
              {"length", length},
              {"drive", drive},
              {"bits", bits},
              {"rate", rate},
              {"tone", tone},
              {"ring", ring},
              {"ringPitch", ringPitch},
              {"rumbleMode", rumbleMode},
              {"rumbleAmount", rumbleAmount},
              {"rumbleLength", rumbleLength},
              {"rumbleDivision", rumbleDivision}}}};
}

constexpr std::array kPresets = {
    preset("INIT", 0.0f, 36.0f, 8.0f, 400.0f, 0.0f, 16.0f, 0.0f, 20000.0f, 0.0f, 900.0f, 0.0f, 50.0f, 600.0f,
           5.0f),
    // Kick_05 from the Mathys Lenne pack: 8 bits held for 5 engine samples (9.6 kHz), played on F#0.
    preset("05 CRUSHED", 0.2f, 50.0f, 5.0f, 390.0f, 20.0f, 8.0f, 4.0f, 7500.0f, 0.0f, 900.0f, 0.0f, 50.0f,
           600.0f, 5.0f),
};
} // namespace

std::span<const DusterPreset> factoryPresets() {
    return kPresets;
}

} // namespace duster
