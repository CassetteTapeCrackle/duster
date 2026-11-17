import { test } from "node:test";
import assert from "node:assert/strict";
import { connect, sanitizePreview } from "../../Source/Web/bridge.js";
import { PARAMS, PRESETS } from "../../Source/Web/format.js";
import { previewOf } from "../../Source/Web/mock.js";

test("outside JUCE the bridge uses the mock backend with the plugin's defaults", async () => {
  const b = await connect({ autoStart: false });
  for (const p of PARAMS) assert.ok(Math.abs(b.param(p.id).scaled() - p.def) < 1e-6, p.id);
  assert.equal(b.rumbleMode.index(), 0);
  assert.equal(b.division.index(), 5);
  b.dispose();
});

test("presets load whole sounds in the mock", async () => {
  const b = await connect({ autoStart: false });
  const { names } = await b.presets();
  assert.deepEqual(names, PRESETS.map((p) => p.name));
  assert.equal(await b.setPreset(1), 1);
  assert.equal(b.param("bits").scaled(), 8);
  assert.equal(b.param("rate").scaled(), 4);
  b.dispose();
});

test("the mock preview has the plugin's shape and a negative first swing", async () => {
  const v = { ...PRESETS[1].values };
  const p = previewOf(v);
  assert.equal(p.lows.length, 400);
  assert.equal(p.highs.length, 400);
  assert.equal(p.pitchSt.length, 400);
  assert.ok(p.lows.some((x) => x < -0.5) && p.highs.some((x) => x > 0.5));
  assert.ok(Math.abs(p.spanMs - 390) < 1);
  assert.ok(p.pitchSt[0] > p.pitchSt[399]);
});

test("a malformed preview from the backend never reaches the renderer", () => {
  const p = sanitizePreview({ lows: [NaN, -0.5, -99], highs: [0.2, -0.9, 99], pitchSt: ["x"], spanMs: -3, noteHz: Infinity });
  assert.deepEqual(p.lows, [-4, -0.5, -4]);
  assert.deepEqual(p.highs, [0.2, -0.5, 4]); // never below its low
  assert.deepEqual(p.pitchSt, [-24, -24, -24]);
  assert.equal(p.spanMs, 1);
  assert.equal(p.noteHz, 8);
  assert.deepEqual(sanitizePreview(null).lows, []);
});
