import { test } from "node:test";
import assert from "node:assert/strict";
import { PARAMS, formatValue, fromNormalised, paramById, toNormalised } from "../../Source/Web/format.js";

test("05 CRUSHED reads like the mockup", () => {
  assert.equal(formatValue("tune", 0.2), "+0.2 st");
  assert.equal(formatValue("tune", -0.04), "+0.0 st");
  assert.equal(formatValue("sweep", 56), "+56 st");
  assert.equal(formatValue("snap", 3.7), "3.7 ms");
  assert.equal(formatValue("length", 390), "390 ms");
  assert.equal(formatValue("drive", 22), "22 %");
  assert.equal(formatValue("bits", 8), "8 bit");
  assert.equal(formatValue("rate", 4), "÷5 9.6k");
  assert.equal(formatValue("rate", 0), "OFF");
  assert.equal(formatValue("rate", 31), "÷32 1.5k");
  assert.equal(formatValue("tone", 6365), "6.4 kHz");
  assert.equal(formatValue("tone", 20000), "20 kHz");
  assert.equal(formatValue("ring", 0), "0 %");
  assert.equal(formatValue("ringPitch", 900), "900 Hz");
  assert.equal(formatValue("ringPitch", 13470), "13.5 kHz");
  assert.equal(formatValue("rumbleLength", 600), "600 ms");
});

test("every parameter round-trips through normalised values", () => {
  for (const p of PARAMS) {
    for (const v of [p.min, p.def, p.max]) {
      const back = fromNormalised(p, toNormalised(p, v));
      assert.ok(Math.abs(back - v) < 1e-6 * Math.max(1, Math.abs(v)), `${p.id} ${v} -> ${back}`);
    }
  }
});

test("a centre value sits at the knob's middle, like juce::NormalisableRange::setSkewForCentre", () => {
  for (const p of PARAMS.filter((q) => q.centre !== undefined))
    assert.ok(Math.abs(toNormalised(p, p.centre) - 0.5) < 1e-9, p.id);
  assert.equal(fromNormalised(paramById("rate"), toNormalised(paramById("rate"), 4)), 4);
});
