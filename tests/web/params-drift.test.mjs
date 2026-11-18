// format.js mirrors the C++ parameters and presets by hand; this keeps them from drifting apart.
import { test } from "node:test";
import assert from "node:assert/strict";
import { readFileSync } from "node:fs";
import { DIVISIONS, PARAMS, PRESETS, RUMBLE_MODES } from "../../Source/Web/format.js";

const cpp = readFileSync(new URL("../../Source/Plugin/DusterParams.cpp", import.meta.url), "utf8");
const header = readFileSync(new URL("../../Source/Plugin/DusterParams.h", import.meta.url), "utf8");
const presetsCpp = readFileSync(new URL("../../Source/Plugin/DusterPresets.cpp", import.meta.url), "utf8");
const ids = Object.fromEntries([...header.matchAll(/(k\w+) = "(\w+)";/g)].map((m) => [m[1], m[2]]));
const num = (s) => Number(s.replace(/f$/, ""));

test("every float parameter matches the C++ range, centre and default", () => {
  const plain = [...cpp.matchAll(/addFloat\(layout, (k\w+), "[^"]+", \{([-\d.]+)f, ([-\d.]+)f, [\d.]+f\}, ([-\d.]+)f/g)];
  const skewed = [...cpp.matchAll(/addFloat\(layout, (k\w+), "[^"]+", skewed\(([-\d.]+)f, ([-\d.]+)f, [\d.]+f, ([\d.]+)f\),\s*([-\d.]+)f/g)];
  assert.equal(plain.length + skewed.length, 10); // BITS is an int and RATE a choice
  for (const [, k, min, max, def] of plain) {
    const p = PARAMS.find((q) => q.id === ids[k]);
    assert.ok(p, `missing ${ids[k]}`);
    assert.deepEqual([p.min, p.max, p.def, p.centre], [num(min), num(max), num(def), undefined], p.id);
  }
  for (const [, k, min, max, centre, def] of skewed) {
    const p = PARAMS.find((q) => q.id === ids[k]);
    assert.ok(p, `missing ${ids[k]}`);
    assert.deepEqual([p.min, p.max, p.def, p.centre], [num(min), num(max), num(def), num(centre)], p.id);
  }
});

test("BITS, RATE and the choices match the C++ layout", () => {
  const bits = cpp.match(/kBits, 1\}, "Bits", (\d+), (\d+), (\d+)\)/);
  const b = PARAMS.find((p) => p.id === "bits");
  assert.deepEqual([b.min, b.max, b.def], [Number(bits[1]), Number(bits[2]), Number(bits[3])]);
  const rateItems = Number(header.match(/kRateItems = (\d+);/)[1]);
  assert.equal(PARAMS.find((p) => p.id === "rate").max, rateItems - 1);
  assert.match(cpp, new RegExp(`"${RUMBLE_MODES.join('", "')}"`));
  const divisions = [...header.match(/kDivisionNames = \{([^}]+)\}/)[1].matchAll(/"([^"]+)"/g)].map((m) => m[1]);
  assert.deepEqual(DIVISIONS, divisions);
});

test("the dev preview's presets match the factory presets", () => {
  const order = presetsCpp.match(/\/\/ Order: ([^\n]+)\n\/\/ ([^\n]+)\n/).slice(1).join(" ").replace(/\./, "").split(/, /);
  const found = [...presetsCpp.matchAll(/preset\("([^"]+)",((?:\s*[-\d.]+f,?){14})\)/g)];
  assert.equal(found.length, PRESETS.length);
  found.forEach(([, name, values], i) => {
    const v = values.split(",").map((s) => num(s.trim()));
    assert.equal(PRESETS[i].name, name);
    order.forEach((id, k) => assert.equal(PRESETS[i].values[id.trim()], v[k], `${name} ${id}`));
  });
});
