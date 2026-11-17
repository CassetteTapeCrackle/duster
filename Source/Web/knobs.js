// 1-bit dial knob (Kikrame's), bound to a bridge parameter.
import { fillDisk, makeBitBuffer } from "./util.js";
import { formatValue } from "./format.js";

const DEG = Math.PI / 180, SWEEP0 = 135 * DEG, SWEEP = 270 * DEG;

function dialFace(face) {
  const B = makeBitBuffer(face, 40, 40, 1), c = 20;
  return (f, active) => {
    B.clear();
    const dim = active ? 1 : 0.4, a1 = SWEEP0 + SWEEP * f;
    B.arc(c, c, 16.5, SWEEP0, SWEEP0 + SWEEP, dim, (t) => (Math.round(t * 80) % 2 ? -1 : null)); // dotted track
    for (const r of [15.5, 16.5, 17.5]) B.arc(c, c, r, SWEEP0, a1, dim);
    for (let k = 0; k <= 10; k++) {
      const a = SWEEP0 + SWEEP * k / 10;
      B.put(c + Math.cos(a) * 19.4, c + Math.sin(a) * 19.4, 0.95 * dim);
    }
    B.arc(c, c, 11.5, 0, Math.PI * 2, 0.75 * dim);
    for (const o of [-0.5, 0, 0.5]) B.line(c + o, c + o, c + Math.cos(a1) * 10 + o, c + Math.sin(a1) * 10 + o, dim);
    fillDisk(B, c, c, 1.6, dim);
    B.blit();
  };
}

// Departure Mono's ÷ reads as + at 11 px, so the value text draws it as a pixel glyph.
function setValueText(el, text) {
  el.textContent = "";
  text.split("÷").forEach((part, i) => {
    if (i > 0) {
      const glyph = document.createElement("i");
      glyph.className = "obelus";
      el.appendChild(glyph);
    }
    el.appendChild(document.createTextNode(part));
  });
}

// Vertical drag (shift = fine), wheel, double-click resets. Every change is a host gesture.
export function makeKnob(host, param, isActive, inactiveLabel) {
  const meta = param.meta;
  const flip = meta.invert ? (n) => 1 - n : (n) => n; // the dial's position from the parameter's, and back
  const el = document.createElement("div");
  el.className = "knob";
  el.innerHTML = `<div class="kface"></div><div class="klbl">${meta.label}</div><div class="kval"></div>`;
  host.appendChild(el);
  const draw = dialFace(el.querySelector(".kface"));
  const val = el.querySelector(".kval");
  const dial = () => flip(param.normalised());
  const setDial = (n) => param.setNormalised(flip(Math.min(1, Math.max(0, n))));

  let dragging = false;
  el.addEventListener("pointerdown", (e) => {
    if (e.button !== 0 || dragging || !isActive()) return;
    dragging = true;
    el.setPointerCapture(e.pointerId);
    const y0 = e.clientY, v0 = dial();
    param.dragStart();
    const move = (ev) => setDial(v0 + ((y0 - ev.clientY) / 180) * (ev.shiftKey ? 0.2 : 1));
    // lostpointercapture fires after pointerup, pointercancel and any other loss of the pointer,
    // so every dragStart gets exactly one dragEnd.
    const end = () => {
      dragging = false;
      param.dragEnd();
      el.removeEventListener("pointermove", move);
      el.removeEventListener("lostpointercapture", end);
    };
    el.addEventListener("pointermove", move);
    el.addEventListener("lostpointercapture", end);
  });
  el.addEventListener("contextmenu", (e) => e.preventDefault());
  el.addEventListener("dblclick", () => !dragging && isActive() && param.reset());
  el.addEventListener("wheel", (e) => {
    if (!isActive() || dragging || e.deltaY === 0) return;
    e.preventDefault();
    // stepped knobs (BITS, RATE) move a whole step per notch, or the rounding would swallow it
    const d = meta.step ? meta.step / (meta.max - meta.min) : e.shiftKey ? 0.002 : 0.01;
    param.dragStart();
    setDial(dial() - Math.sign(e.deltaY) * d);
    param.dragEnd();
  }, { passive: false });

  let last = "";
  return () => {
    const active = isActive(), n = dial(), key = n + "|" + param.scaled() + "|" + active;
    if (key === last) return;
    last = key;
    el.classList.toggle("dis", !active);
    setValueText(val, active ? formatValue(meta.id, param.scaled()) : inactiveLabel());
    draw(n, active);
  };
}
