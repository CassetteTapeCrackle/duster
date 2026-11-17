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
    preset("05 CRUSHED", 0.2f, 56.0f, 3.7f, 390.0f, 22.0f, 8.0f, 4.0f, 6365.0f, 0.0f, 900.0f, 0.0f, 50.0f,
           600.0f, 5.0f),
    // Kick_01: a long sweep into a driven low room.
    preset("01 ROOM RUMBLE", 0.9f, 58.0f, 5.2f, 425.0f, 18.0f, 9.0f, 0.0f, 12000.0f, 0.0f, 900.0f, 1.0f,
           60.0f, 900.0f, 5.0f),
    // Kick_02: a slow sweep and a fixed 13.47 kHz tone that does not follow the note.
    preset("02 HIGH TONE", 1.3f, 39.0f, 12.0f, 410.0f, 8.0f, 16.0f, 0.0f, 20000.0f, 60.0f, 13470.0f, 0.0f,
           50.0f, 600.0f, 5.0f),
    // Kick_06 and 07: a comb ring with partials every 903 Hz.
    preset("06 METAL RING", 4.0f, 60.0f, 5.8f, 420.0f, 10.0f, 16.0f, 0.0f, 9000.0f, 85.0f, 903.0f, 0.0f,
           50.0f, 600.0f, 5.0f),
    // 05 CRUSHED with 1/16 taps at the host tempo.
    preset("05 TAPS", 0.2f, 56.0f, 3.7f, 390.0f, 22.0f, 8.0f, 4.0f, 6365.0f, 0.0f, 900.0f, 2.0f, 55.0f,
           700.0f, 5.0f),
};
} // namespace

std::span<const DusterPreset> factoryPresets() {
    return kPresets;
}

} // namespace duster
