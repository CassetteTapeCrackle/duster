// 1-bit drawing with a 4x4 Bayer dither (from Kikrame, trimmed to what Duster draws).
export const clamp = (x, a, b) => Math.max(a, Math.min(b, x));
export const BAYER = [0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5].map((n) => (n + 0.5) / 16);

export function makeBitBuffer(host, W, H, cssAspect) {
  const cv = document.createElement("canvas");
  cv.width = W;
  cv.height = H;
  cv.style.imageRendering = "pixelated";
  cv.style.aspectRatio = String(cssAspect || W / H);
  host.appendChild(cv);
  const ctx = cv.getContext("2d"), img = ctx.createImageData(W, H), buf = new Float32Array(W * H);
  const B = {
    W, H, buf, cv,
    clear() { buf.fill(0); },
    put(x, y, L) { x |= 0; y |= 0; if (x < 0 || y < 0 || x >= W || y >= H) return; const i = y * W + x; if (L > buf[i]) buf[i] = L; },
    set(x, y, L) { x |= 0; y |= 0; if (x >= 0 && y >= 0 && x < W && y < H) buf[y * W + x] = L; },
    // gap(t) may return null (draw L), -1 (skip pixel) or a value (override)
    line(x0, y0, x1, y1, L, gap) {
      const n = Math.ceil(Math.hypot(x1 - x0, y1 - y0) * 2) + 1;
      for (let k = 0; k <= n; k++) {
        const t = k / n, g = gap ? gap(t) : null;
        if (g === -1) continue;
        B.put(x0 + (x1 - x0) * t, y0 + (y1 - y0) * t, g == null ? L : g);
      }
    },
    arc(cx, cy, r, a0, a1, L, gap) {
      const n = Math.ceil(Math.abs(a1 - a0) * r * 2) + 1;
      for (let k = 0; k <= n; k++) {
        const t = k / n, a = a0 + (a1 - a0) * t, g = gap ? gap(t) : null;
        if (g === -1) continue;
        B.put(cx + Math.cos(a) * r, cy + Math.sin(a) * r, g == null ? L : g);
      }
    },
    blit() {
      for (let y = 0; y < H; y++) for (let x = 0; x < W; x++) {
        const k = y * W + x, o = k * 4;
        const c = buf[k] > BAYER[(y & 3) * 4 + (x & 3)] ? 255 : 0;
        img.data[o] = img.data[o + 1] = img.data[o + 2] = c;
        img.data[o + 3] = 255;
      }
      ctx.putImageData(img, 0, 0);
    },
  };
  return B;
}

export function fillDisk(B, cx, cy, r, L) {
  for (let y = Math.floor(cy - r); y <= cy + r; y++)
    for (let x = Math.floor(cx - r); x <= cx + r; x++)
      if (Math.hypot(x + 0.5 - cx, y + 0.5 - cy) <= r) B.put(x, y, L);
}
