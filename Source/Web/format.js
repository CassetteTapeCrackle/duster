// Parameter metadata (mirrors Source/Plugin/DusterParams.cpp) and value formatting.
// tests/web/params-drift.test.mjs keeps the ranges, defaults and presets in step with the C++.

export const PARAMS = [
  { id: "tune", label: "TUNE", min: -12, max: 12, def: 0, fmt: "st1" },
  { id: "sweep", label: "SWEEP", min: 0, max: 60, def: 36, fmt: "st0" },
  { id: "snap", label: "SNAP", min: 1, max: 40, def: 8, centre: 8, fmt: "ms1" },
  { id: "length", label: "LENGTH", min: 80, max: 1500, def: 400, centre: 400, fmt: "ms" },
  { id: "drive", label: "DRIVE", min: 0, max: 100, def: 0, fmt: "pct" },
  // Clockwise means more crush: the knob shows 16 bits at the left, 2 at the right.
  { id: "bits", label: "BITS", min: 2, max: 16, def: 16, step: 1, invert: true, fmt: "bits" },
  { id: "rate", label: "RATE", min: 0, max: 31, def: 0, step: 1, fmt: "rate" }, // item i holds i + 1 samples
  { id: "tone", label: "TONE", min: 1000, max: 20000, def: 20000, centre: 5000, fmt: "hz" },
  { id: "ring", label: "RING", min: 0, max: 100, def: 0, fmt: "pct" },
  { id: "ringPitch", label: "PITCH", min: 200, max: 18000, def: 900, centre: 2000, fmt: "hz" },
  { id: "rumbleAmount", label: "AMOUNT", min: 0, max: 100, def: 50, fmt: "pct" },
  { id: "rumbleLength", label: "LENGTH", min: 100, max: 2000, def: 600, centre: 600, fmt: "ms" },
];

export const RUMBLE_MODES = ["OFF", "ROOM", "TAPS"];
export const DIVISIONS = ["1/4", "1/8D", "1/8", "1/8T", "1/16D", "1/16", "1/16T", "1/32"];
export const DEFAULT_DIVISION = 5;

// The factory presets, for the dev preview only (the plugin serves its own list).
const presetValues = (tune, sweep, snap, length, drive, bits, rate, tone, ring, ringPitch,
  rumbleMode, rumbleAmount, rumbleLength, rumbleDivision) =>
  ({ tune, sweep, snap, length, drive, bits, rate, tone, ring, ringPitch, rumbleMode, rumbleAmount, rumbleLength, rumbleDivision });
export const PRESETS = [
  { name: "INIT", values: presetValues(0, 36, 8, 400, 0, 16, 0, 20000, 0, 900, 0, 50, 600, 5) },
  { name: "05 CRUSHED", values: presetValues(0.2, 56, 3.7, 390, 22, 8, 4, 6365, 0, 900, 0, 50, 600, 5) },
  { name: "01 ROOM RUMBLE", values: presetValues(0.9, 58, 5.2, 425, 18, 9, 0, 12000, 0, 900, 1, 60, 900, 5) },
  { name: "02 HIGH TONE", values: presetValues(1.3, 39, 12, 410, 8, 16, 0, 20000, 60, 13470, 0, 50, 600, 5) },
  { name: "06 METAL RING", values: presetValues(4, 60, 5.8, 420, 10, 16, 0, 9000, 85, 903, 0, 50, 600, 5) },
  { name: "05 TAPS", values: presetValues(0.2, 56, 3.7, 390, 22, 8, 4, 6365, 0, 900, 2, 55, 700, 5) },
];

export const paramById = (id) => PARAMS.find((p) => p.id === id);
const clamp01 = (x) => (Number.isFinite(x) ? Math.min(1, Math.max(0, x)) : 0);
// juce::NormalisableRange::setSkewForCentre
const skewOf = (p) => (p.centre === undefined ? 1 : Math.log(0.5) / Math.log((p.centre - p.min) / (p.max - p.min)));

export function toNormalised(p, value) {
  const prop = clamp01((value - p.min) / (p.max - p.min));
  const skew = skewOf(p);
  return skew === 1 ? prop : Math.pow(prop, skew);
}

export function fromNormalised(p, norm) {
  const n = clamp01(norm), skew = skewOf(p);
  const prop = skew === 1 ? n : Math.exp(Math.log(Math.max(n, 1e-12)) / skew);
  const v = p.min + (p.max - p.min) * (n === 0 ? 0 : prop);
  return p.step ? Math.round(v / p.step) * p.step : v;
}

const kShort = (k) => (k >= 10 ? String(Math.round(k)) : k.toFixed(1).replace(/\.0$/, ""));

export function formatValue(id, v) {
  switch (paramById(id)?.fmt) {
    case "st1": {
      const mag = Math.abs(v).toFixed(1);
      return (v < 0 && mag !== "0.0" ? "-" : "+") + mag + " st";
    }
    case "st0": return "+" + Math.round(v) + " st";
    case "ms": return Math.round(v) + " ms";
    case "ms1": return (v < 10 ? v.toFixed(1) : String(Math.round(v))) + " ms";
    case "pct": return Math.round(v) + " %";
    case "bits": return Math.round(v) + " bit";
    case "rate": {
      const factor = Math.round(v) + 1;
      return factor <= 1 ? "OFF" : "÷" + factor + " " + kShort(48 / factor) + "k";
    }
    case "hz": return v < 1000 ? Math.round(v) + " Hz" : (v / 1000).toFixed(1).replace(/\.0$/, "") + " kHz";
    default: return String(v);
  }
}
