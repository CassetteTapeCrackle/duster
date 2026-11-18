// Exercises bridge.js's real plugin path against JUCE's own frontend module with a stubbed backend.
import { test } from "node:test";
import assert from "node:assert/strict";
import { existsSync } from "node:fs";
import { pathToFileURL } from "node:url";
import { PARAMS } from "../../Source/Web/format.js";

const candidates = [process.env.DUSTER_JUCE_JS, "build/default/web/juce/index.js", "build/release/web/juce/index.js"];
const jucePath = candidates.find((p) => p && existsSync(p));

function stubBackend() {
  const listeners = new Map();
  const sent = [];
  const backend = {
    addEventListener: (id, fn) => {
      if (!listeners.has(id)) listeners.set(id, []);
      listeners.get(id).push(fn);
      return [id, fn];
    },
    removeEventListener: () => {},
    emitEvent: (id, payload) => sent.push({ id, payload }),
    emitByBackend: (id, payload) => (listeners.get(id) || []).forEach((fn) => fn(payload)),
  };
  globalThis.window = {
    __JUCE__: {
      backend,
      initialisationData: {
        __juce__platform: ["macos"],
        __juce__sliders: PARAMS.map((p) => p.id),
        __juce__toggles: [],
        __juce__comboBoxes: ["rumbleMode", "rumbleDivision"],
        __juce__functions: ["getPresets", "setPreset", "resizeTo", "getPreview", "audition"],
      },
    },
  };
  return { backend, sent };
}

// JUCE's module binds to window.__JUCE__ once, at first import, so every test shares one stub.
const stub = jucePath ? stubBackend() : null;

test("plugin path: double-click reset is wrapped in a host gesture", { skip: !jucePath && "plugin not built" }, async () => {
  const { sent } = stub;
  const Juce = await import(pathToFileURL(jucePath).href);
  const { connect } = await import("../../Source/Web/bridge.js");
  const b = await connect({ juceModule: Juce });
  sent.length = 0;
  b.param("drive").reset();
  const types = sent.filter((s) => s.id === "__juce__sliderdrive").map((s) => s.payload.eventType);
  assert.deepEqual(types, ["sliderDragStarted", "valueChanged", "sliderDragEnded"]);
});

test("plugin path: native functions and events are wired by name", { skip: !jucePath && "plugin not built" }, async () => {
  const { backend, sent } = stub;
  const Juce = await import(pathToFileURL(jucePath).href);
  const { connect } = await import("../../Source/Web/bridge.js");
  const b = await connect({ juceModule: Juce });
  let hits = 0;
  const previews = [];
  b.onHit(() => hits++);
  b.onPreview((p) => previews.push(p));
  backend.emitByBackend("hit", {});
  backend.emitByBackend("preview", { lows: [-0.5], highs: [0.5], pitchSt: [12], spanMs: 390, noteHz: 46.25 });
  assert.equal(hits, 1);
  assert.deepEqual(previews[0].highs, [0.5]);

  sent.length = 0;
  const pending = b.preview();
  const call = sent.find((s) => s.id === "__juce__invoke");
  assert.equal(call.payload.name, "getPreview");
  backend.emitByBackend("__juce__complete", { promiseId: call.payload.resultId, result: { lows: [-0.1, -0.2], highs: [0.1, 0.2], pitchSt: [1, 0], spanMs: 400, noteHz: 46 } });
  assert.deepEqual((await pending).highs, [0.1, 0.2]);

  sent.length = 0;
  b.audition();
  assert.equal(sent.find((s) => s.id === "__juce__invoke").payload.name, "audition");
  b.rumbleMode.set(2);
  assert.ok(sent.some((s) => s.id === "__juce__comboBoxrumbleMode"));
});
