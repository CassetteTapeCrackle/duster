# Duster notes

## Intent
A MIDI-played kick synth that generates the whole kick: hard/industrial techno kicks whose ringy, noisy
top end is made from the kick itself, never a layered transient. It nails Kick_05 of the Mathys Lenne
pack and reaches the territory of the pack's other kicks. Small window (Batteur voice or Kilohearts
size), UI exactly in Kikrame's style. Name: DUSTER (2026-11-17).

## Decisions (2026-11-17)
- An instrument (MIDI in, generates the kick). Not an effect, not a Batteur patch.
- Controls approved: TUNE, SWEEP, SNAP, LENGTH, DRIVE, BITS, RATE, TONE; RING + RING PITCH;
  RUMBLE mode OFF/ROOM/TAPS + AMOUNT + LENGTH.
- Ship a preset that sounds like Kick_05.
- JUCE 8 + WebView, built on Kikrame's setup (CPM, CI, pkg, mock backend). Window about 560x340.
- No plugin code until the design doc is approved.

## Kick_05 measurements (the preset target)
- Pitch: about 600 Hz at the start (one sharp negative spike in the first 0.1 ms), 323 Hz at 3.5 ms,
  133 at 9 ms, 72 at 20 ms, 57 at 35 ms, 51 at 54 ms, settles at 46.9 Hz (F#1 +24 cents) by 100 ms.
  Sweep law (fits 1-110 ms within 2.6%): st(t) = 50 * (0.85 * 2^(-t/5 ms) + 0.15 * 2^(-t/25 ms)).
  A single exponential misses by 10%+. Root note: MIDI 30 (F#0 in Live) +0.2 st.
- Amp: starts near 60%, full level by 5-8 ms, holds, sags to about 70% at 340 ms, silent by 390 ms.
  First swing goes negative. Mono.
- Saturation: light and odd-heavy. h2 -28 dB, h3 -21, h5 -39 (tanh drive 1.5 with bias 0.1 matches).
- Crusher: 8 bits, sample-and-hold at 9600 Hz = 48 kHz / 5. The whole-number factor matters: a
  fractional hold adds a ladder of extra lines. Ring line at 9600 - f0, images at 19.2k and a folded 15.25k.
- Tone: 2-pole low-pass around 7.5 kHz after the crusher.
- Rebuild (`research/resynth05.py`) matched: ring -52 vs -53 dB, body noise bed -67/-72 vs -68/-73 dB.
  Listening test: "sounds exactly like the original". Gap: the original attack has more 3-8 kHz in the
  first 10 ms (-44 vs -57 dB).

## The pack's other kicks (territory to reach)
- 06 and 07: strong comb ring, partials every ~900 Hz (about a 1.1 ms delay).
- Fixed high tones that do not follow the note: 13.47 kHz (02, 04), 17.37 kHz (11, 12, 16, 20),
  15.69 kHz (06, 07).
- Most: a 10-25 ms broadband burst, then low-level grit pulsing once per body cycle (noise going
  into a clipper together with the body).
- 01: a second event around 110 ms (rumble).
- Samples: `~/LOCAL/Samples Triés/ONESHOTS/Drums/Kick/Mathys Lenne Kicks` (not in the repo)

## Current state
- Mockup v1 and the design are approved. The implementation plan lives in the gitignored `docs/superpowers/`.
- Task 1 done: build, CI, packaging and a silent instrument on `main`. Work continues on branch `v1` (off `dev`).
