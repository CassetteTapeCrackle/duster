// The kick display: the hit's waveform as a dithered silhouette with its pitch curve on top, and a
// playhead while it sounds. Drawn from the plugin's preview: per column, the lowest and highest
// sample and the pitch.
import { clamp, makeBitBuffer } from "./util.js";

const FULL_SCALE = 0.9; // the limiter's ceiling, rounded: a full hit fills the height
const LO = Math.log2(25), HI = Math.log2(1600); // pitch axis, log Hz
const CSS_PX_PER_BIT = 2;

// One column's vertical extent: one scale for both halves, so a negative swing is drawn as tall as
// a positive swing of the same size.
export function waveColumn(low, high, H) {
  const mid = Math.round((H - 1) / 2), scale = (mid - 1) / FULL_SCALE;
  return {
    top: clamp(Math.round(mid - high * scale), 0, H - 1),
    bottom: clamp(Math.round(mid - low * scale), 0, H - 1),
  };
}

// Follows the host's size: the bit buffer is rebuilt whenever the box changes, at a whole number of
// CSS pixels per bit so the dither never bands.
export function makeKickDisplay(host) {
  let B = null;
  const fit = () => {
    const W = Math.max(40, Math.floor(host.clientWidth / CSS_PX_PER_BIT) - 3);
    const H = Math.max(20, Math.floor(host.clientHeight / CSS_PX_PER_BIT) - 2);
    if (B && B.W === W && B.H === H) return;
    host.textContent = "";
    B = makeBitBuffer(host, W, H);
    B.cv.style.width = W * CSS_PX_PER_BIT + "px";
    B.cv.style.height = H * CSS_PX_PER_BIT + "px";
  };
  const seg = (x0, y0, x1, y1, fn) => {
    const n = Math.max(Math.abs(x1 - x0), Math.abs(y1 - y0), 1);
    for (let k = 0; k <= n; k++) fn(Math.round(x0 + (x1 - x0) * k / n), Math.round(y0 + (y1 - y0) * k / n));
  };
  return (preview, play) => {
    fit();
    const { W, H } = B, mid = Math.round((H - 1) / 2);
    B.clear();
    B.line(0, mid, W - 1, mid, 0.3);
    if (preview && preview.lows.length) {
      const span = preview.spanMs, cols = preview.lows.length, col = (c) => Math.min(cols - 1, Math.floor((c * cols) / W));
      const grid = span > 1000 ? 500 : 100; // faint 100 ms columns
      for (let ms = grid; ms < span; ms += grid) B.line((ms * W) / span, 0, (ms * W) / span, H - 1, 0.3);
      for (let c = 0; c < W; c++) {
        const { top, bottom } = waveColumn(preview.lows[col(c)], preview.highs[col(c)], H);
        for (let y = top; y <= bottom; y++) B.put(c, y, 0.5);
        B.set(c, top, 1);
        B.set(c, bottom, 1);
      }
      // the pitch curve, cut out of the waveform with a black halo
      const pts = [];
      for (let c = 0; c < W; c++) {
        const y = Math.round(H - 2 - ((Math.log2(preview.noteHz) + preview.pitchSt[col(c)] / 12 - LO) / (HI - LO)) * (H - 3));
        pts.push([c, clamp(y, 1, H - 2)]);
      }
      for (let i = 1; i < pts.length; i++)
        seg(...pts[i - 1], ...pts[i], (x, y) => { B.set(x, y - 1, 0); B.set(x, y + 1, 0); B.set(x - 1, y, 0); B.set(x + 1, y, 0); });
      for (let i = 1; i < pts.length; i++) seg(...pts[i - 1], ...pts[i], (x, y) => B.set(x, y, 1));
    }
    if (play >= 0) {
      const x = Math.round(play * (W - 1));
      for (let y = 0; y < H; y++) B.set(x, y, 1);
    }
    B.blit();
  };
}
