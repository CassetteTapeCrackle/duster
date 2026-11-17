// Duster UI entry point: builds the layout of mockup v1 and runs the render loop.
import { connect } from "./bridge.js";
import { DIVISIONS, RUMBLE_MODES, paramById } from "./format.js";
import { makeKnob } from "./knobs.js";
import { makeKickDisplay } from "./display.js";
import { isKnobActive, playFraction } from "./view.js";

(async () => {
try {
await document.fonts.ready; // the rows' height depends on the font
const bridge = await connect();
let program = 0, presetNames = [], preview = null, hitMs = -1;

const app = document.getElementById("app");
app.innerHTML = `
  <div class="plug3">
    <div class="top"><span class="logo">DUSTER</span>
      <span class="preset"><button class="pv" title="Previous preset">&lt;</button><span class="pname"></span><button class="pn" title="Next preset">&gt;</button></span></div>
    <div class="vis" title="Click to play"></div>
    <div class="row main"></div><div class="row bot"></div>
  </div>
  <div class="grip" title="Resize"></div>`;

const knob = (host, id) => makeKnob(host, bridge.param(id), () => isKnobActive(id, bridge.rumbleMode.index()), () => "NO RUMBLE");
const main = app.querySelector(".main"), bot = app.querySelector(".bot");
const knobs = ["tune", "sweep", "snap", "length", "drive", "bits", "rate", "tone"].map((id) => knob(main, id));
knobs.push(knob(bot, "ring"), knob(bot, "ringPitch"));
const rum = document.createElement("div");
rum.className = "rumble";
rum.innerHTML = `<div class="sw"><span class="mode">${RUMBLE_MODES.map((m, i) => `<button data-r="${i}">${m}</button>`).join("")}</span></div>
  <div class="klbl">RUMBLE</div>
  <div class="division"><button class="dv" title="Shorter taps">&lt;</button><span class="dname"></span><button class="dn" title="Longer taps">&gt;</button></div>`;
bot.appendChild(rum);
knobs.push(knob(bot, "rumbleAmount"), knob(bot, "rumbleLength"));

const rumButtons = [...rum.querySelectorAll(".mode button")];
rumButtons.forEach((b) => b.addEventListener("click", () => bridge.rumbleMode.set(Number(b.dataset.r))));
const division = rum.querySelector(".division"), dname = rum.querySelector(".dname");
const stepDivision = (d) => {
  if (!isKnobActive("rumbleDivision", bridge.rumbleMode.index())) return;
  bridge.division.set(Math.min(DIVISIONS.length - 1, Math.max(0, bridge.division.index() + d)));
};
rum.querySelector(".dv").addEventListener("click", () => stepDivision(1)); // later items are shorter
rum.querySelector(".dn").addEventListener("click", () => stepDivision(-1));

const vis = app.querySelector(".vis");
const drawVis = makeKickDisplay(vis);

const pname = app.querySelector(".pname");
const stepPreset = async (d) => {
  if (!presetNames.length) return;
  program = await bridge.setPreset((program + d + presetNames.length) % presetNames.length);
};
app.querySelector(".pv").addEventListener("click", () => stepPreset(-1));
app.querySelector(".pn").addEventListener("click", () => stepPreset(1));
vis.addEventListener("click", () => bridge.audition());

// Dev preview only: ?preset=1 starts on a preset (the plugin never loads the page with a query).
const query = new URLSearchParams(location.search);
if (query.has("preset")) bridge.setPreset(Number(query.get("preset"))).then((p) => (program = p));

bridge.presets().catch(() => ({ names: [], current: 0 })).then(({ names, current }) => {
  presetNames = names;
  program = current;
});
bridge.onProgram((p) => (program = p));
bridge.onPreview((p) => (preview = p));
bridge.onHit(() => (hitMs = performance.now()));
bridge.preview().then((p) => (preview ??= p)).catch(() => {});

// Keep every knob canvas at a whole-number pixel scale, so the dither never bands (the display sizes itself).
function snapCanvases() {
  const dpr = window.devicePixelRatio || 1;
  for (const box of app.querySelectorAll(".kface")) {
    const c = box.querySelector("canvas");
    if (!c) continue;
    const k = Math.max(1, Math.floor((box.clientWidth * dpr) / c.width));
    c.style.width = (c.width * k) / dpr + "px";
  }
}
new ResizeObserver(snapCanvases).observe(document.body);
snapCanvases();

// Resize grip: report the new size to the plugin, at most once per frame.
const grip = app.querySelector(".grip");
let pendingSize = null;
const [minW, minH, maxW, maxH] = bridge.sizeLimits(); // from DusterEditor, so the two never drift
const clampSize = (w, h) => [Math.min(maxW, Math.max(minW, w)), Math.min(maxH, Math.max(minH, h))];
grip.addEventListener("pointerdown", (e) => {
  if (e.button !== 0) return;
  grip.setPointerCapture(e.pointerId);
  const x0 = e.screenX, y0 = e.screenY, w0 = window.innerWidth, h0 = window.innerHeight;
  const move = (ev) => (pendingSize = clampSize(w0 + ev.screenX - x0, h0 + ev.screenY - y0));
  const end = () => {
    grip.removeEventListener("pointermove", move);
    grip.removeEventListener("lostpointercapture", end);
  };
  grip.addEventListener("pointermove", move);
  grip.addEventListener("lostpointercapture", end);
});

// Drawing faster than 60 fps (ProMotion runs rAF at 120) only costs CPU.
const MIN_FRAME_MS = 1000 / 60 - 1;
let last = 0;
function frame(now) {
  requestAnimationFrame(frame);
  if (now - last < MIN_FRAME_MS) return;
  last = now;
  drawVis(preview, preview ? playFraction(hitMs, now, preview.spanMs) : -1);
  knobs.forEach((k) => k());
  pname.textContent = presetNames[program] ?? "";
  const mode = bridge.rumbleMode.index();
  rumButtons.forEach((b) => b.classList.toggle("on", Number(b.dataset.r) === mode));
  division.classList.toggle("dis", !isKnobActive("rumbleDivision", mode));
  dname.textContent = mode === 1 ? "ROOM" : DIVISIONS[bridge.division.index()] + " TAPS";
  if (pendingSize) {
    bridge.resizeTo(pendingSize[0], pendingSize[1]);
    pendingSize = null;
  }
}
requestAnimationFrame(frame);
} catch (err) {
  // Never leave a silent black window: say what broke.
  document.getElementById("app").textContent = "DUSTER UI FAILED TO START: " + (err && err.message ? err.message : err);
  console.error(err);
}
})();
