// Small pure rules the layout follows, kept apart from the DOM so they can be tested.

// Which knobs a rumble mode (0 OFF, 1 ROOM, 2 TAPS) leaves active.
export function isKnobActive(id, rumbleMode) {
  if (id === "rumbleAmount" || id === "rumbleLength") return rumbleMode > 0;
  if (id === "rumbleDivision") return rumbleMode === 2;
  return true;
}

// Where the playhead sits (0 to 1) `nowMs` after a hit at `hitMs`, or -1 when nothing is playing.
export function playFraction(hitMs, nowMs, spanMs) {
  if (hitMs < 0 || !(spanMs > 0)) return -1;
  const f = (nowMs - hitMs) / spanMs;
  return f >= 0 && f <= 1 ? f : -1;
}
