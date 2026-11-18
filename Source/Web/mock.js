// Stand-in backend for previewing the UI in a plain browser: the plugin's parameters and presets, a
// rough JS kick (the engine's laws, simplified) for the display and the audition, and the plugin's
// event shapes. Never used inside the plugin.
import { DEFAULT_DIVISION, PARAMS, PRESETS, fromNormalised, paramById, toNormalised } from "./format.js";
import { DEFAULT_SIZE_LIMITS, sanitizePreview } from "./bridge.js";

const SR = 48000, COLUMNS = 400, NOTE = 30;

const sweepSt = (ms, sweep, snap) => sweep * (0.85 * 2 ** (-ms / snap) + 0.15 * 2 ** (-ms / (5 * snap)));
function ampShape(ms, L) {
  if (ms >= L) return 0;
  if (ms < 6) return 0.6 + (0.4 * Math.max(ms, 0)) / 6;
  const rel = Math.max(6, L - 50), hold = Math.min(100, rel);
  if (ms < hold) return 1;
  if (ms < rel) return 1 - (0.3 * (ms - hold)) / (rel - hold);
  return ((rel > hold ? 0.7 : 1) * (L - ms)) / (L - rel);
}
function lowpass(x, hz, q) { // RBJ 2-pole, in place
  const w = (2 * Math.PI * Math.min(hz, SR * 0.49)) / SR, al = Math.sin(w) / (2 * q), c = Math.cos(w), a0 = 1 + al;
  const b0 = (1 - c) / 2 / a0, b1 = (1 - c) / a0, a1 = (-2 * c) / a0, a2 = (1 - al) / a0;
  let x1 = 0, x2 = 0, y1 = 0, y2 = 0;
  for (let i = 0; i < x.length; i++) {
    const v = x[i], y = b0 * v + b1 * x1 + b0 * x2 - a1 * y1 - a2 * y2;
    x2 = x1; x1 = v; y2 = y1; y1 = y; x[i] = y;
  }
}
const toneCorner = (hz) => Math.min((SR / Math.PI) * Math.atan(Math.tan((Math.PI * hz) / SR) / 0.472795), 0.49 * SR);

export function renderKick(v, note = NOTE) {
  const nBody = Math.round((v.length * SR) / 1000);
  const n = nBody + (v.rumbleMode ? Math.round((v.rumbleLength * SR) / 1000) : 0);
  const y = new Float32Array(n), f0 = 440 * 2 ** ((note - 69 + v.tune) / 12);
  const d = v.drive / 100, g = 5.75 * d, b = 0.5 * d, tb = Math.tanh(b);
  const scale = Math.max(Math.tanh(g - b) + tb, Math.tanh(g + b) - tb);
  let ph = 0;
  for (let i = 0; i < nBody; i++) {
    const x = -Math.sin(2 * Math.PI * ph);
    ph += (f0 * 2 ** (sweepSt((i * 1000) / SR, v.sweep, v.snap) / 12)) / SR;
    y[i] = d < 1e-3 ? x : (Math.tanh(g * x + b) - tb) / scale;
  }
  if (v.ring > 0) { // a feedback comb at RING PITCH, struck at every upward zero crossing
    const D = SR / v.ringPitch, fb = 0.5 + 0.49 * (v.ring / 100), c = new Float32Array(nBody);
    for (let i = 1; i < nBody; i++) {
      const j = i - D, j0 = Math.floor(j), del = j0 >= 0 ? c[j0] + (c[j0 + 1] - c[j0]) * (j - j0) : 0;
      c[i] = (y[i - 1] < 0 && y[i] >= 0 ? 1 : 0) + fb * del;
    }
    for (let i = 0; i < nBody; i++) y[i] += (v.ring / 100) * 0.3 * c[i];
  }
  const q = 2 ** (Math.round(v.bits) - 1), hold = Math.round(v.rate) + 1;
  for (let i = 0; i < nBody; i++) y[i] = i % hold ? y[i - (i % hold)] : v.bits >= 16 ? y[i] : Math.round(y[i] * q) / q;
  lowpass(y.subarray(0, nBody), toneCorner(v.tone), 0.4);
  for (let i = 0; i < nBody; i++) y[i] *= 0.8 * ampShape((i * 1000) / SR, v.length);
  if (v.rumbleMode) { // three echoes (taps) or a smeared copy (room), low-passed, added in
    const beats = [1, 0.75, 0.5, 1 / 3, 0.375, 0.25, 1 / 6, 0.125][v.rumbleDivision] ?? 0.25;
    const step = v.rumbleMode === 2 ? Math.round((SR * 60 * beats) / 140) : Math.round(SR * 0.03);
    const r = new Float32Array(n);
    for (let k = 1; k <= (v.rumbleMode === 2 ? 3 : 12); k++) {
      const gain = 0.7 * Math.exp((-3 * k * step) / SR / (v.rumbleLength / 1000));
      for (let i = k * step; i < n; i++) r[i] += gain * y[i - k * step];
    }
    lowpass(r, 250, 0.7);
    for (let i = 0; i < n; i++) y[i] += (v.rumbleAmount / 100) * Math.tanh(2 * r[i]) / 2;
  }
  for (let i = 0; i < n; i++) y[i] = Math.max(-0.89, Math.min(0.89, y[i]));
  return y;
}

export function previewOf(v, note = NOTE) {
  const out = renderKick(v, note), spanMs = (out.length * 1000) / SR;
  const lows = [], highs = [], pitchSt = [];
  for (let c = 0; c < COLUMNS; c++) {
    const from = Math.floor((c * out.length) / COLUMNS);
    let lo = out[from] ?? 0, hi = lo;
    for (let i = from; i < Math.floor(((c + 1) * out.length) / COLUMNS); i++) {
      lo = Math.min(lo, out[i]);
      hi = Math.max(hi, out[i]);
    }
    lows.push(lo);
    highs.push(hi);
    pitchSt.push(v.tune + sweepSt(((c + 0.5) * spanMs) / COLUMNS, v.sweep, v.snap));
  }
  return sanitizePreview({ lows, highs, pitchSt, spanMs, noteHz: 440 * 2 ** ((note - 69) / 12) });
}

export function createMockBackend({ autoStart = typeof window !== "undefined" } = {}) {
  const norm = new Map(PARAMS.map((p) => [p.id, toNormalised(p, p.def)]));
  const listeners = new Map(PARAMS.map((p) => [p.id, new Set()]));
  const previewListeners = new Set(), hitListeners = new Set(), programListeners = new Set();
  let program = 0, previewTimer = null, audio = null;
  const value = (id) => fromNormalised(paramById(id), norm.get(id));
  const values = () => ({ ...Object.fromEntries(PARAMS.map((p) => [p.id, value(p.id)])), rumbleMode: modes.index(), rumbleDivision: divisions.index() });

  const schedulePreview = () => {
    if (!autoStart) return;
    clearTimeout(previewTimer);
    previewTimer = setTimeout(() => { const p = previewOf(values()); for (const cb of previewListeners) cb(p); }, 50);
  };
  const setNorm = (id, v) => {
    norm.set(id, Math.min(1, Math.max(0, v)));
    for (const cb of listeners.get(id)) cb();
    schedulePreview();
  };
  const makeChoice = (initial) => {
    let index = initial;
    const cbs = new Set();
    return {
      index: () => index,
      set: (i) => { index = i | 0; for (const cb of cbs) cb(); schedulePreview(); },
      onChange: (cb) => cbs.add(cb),
    };
  };
  const modes = makeChoice(0), divisions = makeChoice(DEFAULT_DIVISION);

  return {
    param: (id) => {
      const meta = paramById(id);
      return {
        meta,
        normalised: () => norm.get(id),
        scaled: () => value(id),
        setNormalised: (v) => setNorm(id, v),
        dragStart: () => {},
        dragEnd: () => {},
        reset: () => setNorm(id, toNormalised(meta, meta.def)),
        onChange: (cb) => listeners.get(id).add(cb),
      };
    },
    rumbleMode: modes,
    division: divisions,
    onProgram: (cb) => programListeners.add(cb),
    onHit: (cb) => hitListeners.add(cb),
    onPreview: (cb) => previewListeners.add(cb),
    presets: async () => ({ names: PRESETS.map((p) => p.name), current: program }),
    setPreset: async (i) => {
      program = Math.min(PRESETS.length - 1, Math.max(0, i | 0));
      const v = PRESETS[program].values;
      for (const p of PARAMS) setNorm(p.id, toNormalised(p, v[p.id]));
      modes.set(v.rumbleMode);
      divisions.set(v.rumbleDivision);
      for (const cb of programListeners) cb(program);
      return program;
    },
    preview: async () => previewOf(values()),
    audition: async () => {
      if (typeof AudioContext !== "undefined") {
        audio ??= new AudioContext({ sampleRate: SR });
        const out = renderKick(values()), buf = audio.createBuffer(1, out.length, SR);
        buf.getChannelData(0).set(out);
        const src = audio.createBufferSource();
        src.buffer = buf;
        src.connect(audio.destination);
        await audio.resume();
        src.start();
      }
      for (const cb of hitListeners) cb();
    },
    sizeLimits: () => DEFAULT_SIZE_LIMITS,
    resizeTo: async () => {},
    dispose: () => clearTimeout(previewTimer),
  };
}
