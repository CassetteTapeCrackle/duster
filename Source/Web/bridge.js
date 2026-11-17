// One interface to the plugin, whether the page runs inside the JUCE WebView or in a plain browser
// (dev preview), where a mock backend stands in.
import { DIVISIONS, PARAMS, RUMBLE_MODES, toNormalised } from "./format.js";

export const DEFAULT_SIZE_LIMITS = [560, 340, 1120, 680]; // used only by the dev preview
const MAX_COLUMNS = 2048;
const finite = (x, lo, hi) => (Number.isFinite(x) ? Math.min(hi, Math.max(lo, x)) : lo);

// Never trust what arrives from the backend: the renderers assume finite, in-range numbers.
export function sanitizePreview(p) {
  const n = Array.isArray(p?.lows) && Array.isArray(p?.highs) ? Math.min(p.lows.length, p.highs.length, MAX_COLUMNS) : 0;
  const column = (a, i, lo, hi) => finite(Array.isArray(a) ? a[i] : 0, lo, hi);
  const lows = Array.from({ length: n }, (_, i) => column(p.lows, i, -4, 4));
  return {
    lows,
    highs: Array.from({ length: n }, (_, i) => Math.max(lows[i], column(p.highs, i, -4, 4))),
    pitchSt: Array.from({ length: n }, (_, i) => column(p.pitchSt, i, -24, 96)),
    spanMs: finite(p?.spanMs, 1, 10000),
    noteHz: finite(p?.noteHz, 8, 13000),
  };
}

export const sanitizeProgram = (p) => (Number.isInteger(p) && p >= 0 ? p : 0);
export const sanitizePresets = (p) => ({
  names: Array.isArray(p?.names) ? p.names.filter((n) => typeof n === "string") : [],
  current: sanitizeProgram(p?.current),
});
const sanitizeChoice = (i, count) => (Number.isInteger(i) && i >= 0 && i < count ? i : 0);

// options.juceModule lets tests pass JUCE's frontend module explicitly.
export async function connect(options = {}) {
  if (options.juceModule || (typeof window !== "undefined" && window.__JUCE__)) return connectJuce(options.juceModule);
  const { createMockBackend } = await import("./mock.js");
  return createMockBackend(options);
}

async function connectJuce(juceModule) {
  const Juce = juceModule ?? (await import("./juce/index.js"));
  const params = new Map();
  for (const p of PARAMS) {
    const s = Juce.getSliderState(p.id);
    params.set(p.id, {
      meta: p,
      normalised: () => s.getNormalisedValue(),
      scaled: () => s.getScaledValue(),
      setNormalised: (v) => s.setNormalisedValue(Math.min(1, Math.max(0, v))),
      dragStart: () => s.sliderDragStarted(),
      dragEnd: () => s.sliderDragEnded(),
      // A one-shot change still needs a gesture, or touch-automation hosts drop it.
      reset: () => {
        s.sliderDragStarted();
        s.setNormalisedValue(toNormalised(p, p.def));
        s.sliderDragEnded();
      },
      onChange: (cb) => s.valueChangedEvent.addListener(cb),
    });
  }
  const choice = (id, count) => {
    const s = Juce.getComboBoxState(id);
    return {
      index: () => sanitizeChoice(s.getChoiceIndex(), count),
      set: (i) => s.setChoiceIndex(i),
      onChange: (cb) => s.valueChangedEvent.addListener(cb),
    };
  };
  const native = (name) => Juce.getNativeFunction(name);
  const getPresets = native("getPresets");
  const setPreset = native("setPreset");
  const resizeTo = native("resizeTo");
  const getPreview = native("getPreview");
  const audition = native("audition");
  const backend = window.__JUCE__.backend;
  return {
    param: (id) => params.get(id),
    rumbleMode: choice("rumbleMode", RUMBLE_MODES.length),
    division: choice("rumbleDivision", DIVISIONS.length),
    onProgram: (cb) => backend.addEventListener("program", (e) => cb(sanitizeProgram(e?.program))),
    onHit: (cb) => backend.addEventListener("hit", () => cb()),
    onPreview: (cb) => backend.addEventListener("preview", (e) => cb(sanitizePreview(e))),
    presets: async () => sanitizePresets(await getPresets()),
    setPreset: async (i) => sanitizeProgram(await setPreset(i)),
    preview: async () => sanitizePreview(await getPreview()),
    audition: () => audition(),
    sizeLimits: () => {
      const l = window.__JUCE__.initialisationData?.sizeLimits;
      return Array.isArray(l) && l.length === 4 && l.every(Number.isFinite) ? l : DEFAULT_SIZE_LIMITS;
    },
    resizeTo: (w, h) => resizeTo(Math.round(w), Math.round(h)),
    dispose: () => {},
  };
}
