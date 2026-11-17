import { test } from "node:test";
import assert from "node:assert/strict";
import { isKnobActive, playFraction } from "../../Source/Web/view.js";

test("the rumble knobs grey out when the rumble is off", () => {
  for (const id of ["rumbleAmount", "rumbleLength"]) {
    assert.equal(isKnobActive(id, 0), false, id);
    assert.equal(isKnobActive(id, 1), true, id);
    assert.equal(isKnobActive(id, 2), true, id);
  }
  assert.equal(isKnobActive("drive", 0), true);
});

test("the division only matters for taps", () => {
  assert.deepEqual([0, 1, 2].map((m) => isKnobActive("rumbleDivision", m)), [false, false, true]);
});

test("the playhead runs over the preview's span, then stops", () => {
  assert.equal(playFraction(-1, 1000, 500), -1);
  assert.equal(playFraction(1000, 1250, 500), 0.5);
  assert.equal(playFraction(1000, 1600, 500), -1);
});
