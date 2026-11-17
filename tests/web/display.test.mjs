import { test } from "node:test";
import assert from "node:assert/strict";
import { waveColumn } from "../../Source/Web/display.js";

test("a negative swing is drawn as tall as a positive swing of the same size", () => {
  const H = 80, mid = Math.round((H - 1) / 2);
  for (const peak of [0, 0.1, 0.5, 0.89]) {
    const { top, bottom } = waveColumn(-peak, peak, H);
    assert.equal(mid - top, bottom - mid, `peak ${peak}`);
  }
});

test("columns stay inside the display and grow with level", () => {
  const { top, bottom } = waveColumn(-3, 3, 80);
  assert.ok(top >= 0 && bottom <= 79);
  const a = waveColumn(-0.2, 0.2, 80), b = waveColumn(-0.8, 0.8, 80);
  assert.ok(b.bottom - b.top > a.bottom - a.top);
  const up = waveColumn(0.1, 0.6, 80);
  assert.ok(up.bottom < Math.round(79 / 2)); // an all-positive column sits above the zero line
});
