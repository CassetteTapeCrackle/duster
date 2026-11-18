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
- Tasks 1 to 10 done on branch `v1` (draft PR #1 into `dev`): engine, plugin, render and comparison tools, web UI.
- Pack clones plan (2026-11-18): ring level fixed, proof, settings render and fitting tools done; baseline below.
- 05 CRUSHED passes the Kick_05 gate at 44.1 and 48 kHz (`tools/compare_kick.py`). The 3-8 kHz attack stays about 19 dB short.
- Task 11 first pass: 01 ROOM RUMBLE, 02 HIGH TONE, 06 METAL RING, 05 TAPS. Waiting on the ear pass.
- Added on request: a DIVISION control for TAPS (1/4 to 1/32 with dotted and triplet steps).

## Baseline (2026-11-19)
All 20 pack kicks fitted with today's 14 knobs (`tools/fit_kick.py`, modes OFF and ROOM, each kick started from
the closest existing preset), rendered from `docs/research/fits/baseline.tsv` and proven at 44.1 and 48 kHz
(`tools/proof_pack.py`). Verdicts: 0 PASS, 0 CLOSE, 20 MISS at both rates. No limit changed from the spec.

Misses at 48 kHz, as differences from each kick (the three worst energy-map regions only):

| kick | 44k | 48k | misses |
|---|---|---|---|
| 01 | MISS | MISS | map: high bands in the first 20 ms 28 dB loud, high bands in the body 20 dB loud, mid bands in the body 16 dB loud; level 5.7 dB off louder from 185 to 430 ms; glide 40% off, low; peak-to-loudness 2.0 dB lower |
| 02 | MISS | MISS | map: high bands in the body 18 dB loud, mid bands in the ending 14 dB quiet, mid bands in the body 12 dB quiet; level 5.5 dB off louder from 175 to 420 ms; glide 50% off, low; attack 8-16 kHz 7 dB short; peak-to-loudness 2.2 dB lower |
| 03 | MISS | MISS | map: low bands in the ending 18 dB loud, mid bands in the ending 14 dB loud, mid bands in the body 11 dB loud; level 19.5 dB off louder from 150 to 340 ms; glide 68% off, low; peak-to-loudness 4.0 dB lower |
| 04 | MISS | MISS | map: high bands in the body 19 dB loud, mid bands in the body 12 dB loud, low bands in the body 11 dB quiet; glide 52% off, low; attack 1-3 kHz 6 dB loud; attack 8-16 kHz 4 dB short; peak-to-loudness 2.4 dB lower |
| 05 | MISS | MISS | map: low bands in the first 20 ms 10 dB quiet, low bands in the body 9 dB quiet, high bands in the first 20 ms 9 dB quiet; level 4.4 dB off louder from 95 to 395 ms; glide 11% off, low; attack 3-8 kHz 5 dB short; peak-to-loudness 3.0 dB lower |
| 06 | MISS | MISS | map: mid bands in the body 31 dB loud, high bands in the body 31 dB loud, high bands in the first 20 ms 25 dB loud; level 10.0 dB off louder from 10 to 425 ms; glide 26% off, high; attack 1-3 kHz 6 dB loud |
| 07 | MISS | MISS | map: high bands in the body 20 dB loud, high bands in the first 20 ms 17 dB loud, low bands in the body 15 dB quiet; level 11.9 dB off louder from 135 to 425 ms; glide 33% off, high; attack 1-3 kHz 6 dB loud; peak-to-loudness 3.8 dB lower |
| 08 | MISS | MISS | map: high bands in the first 20 ms 12 dB loud, high bands in the body 12 dB loud, mid bands in the body 11 dB loud; glide 50% off, low; peak-to-loudness 1.3 dB lower |
| 09 | MISS | MISS | map: high bands in the ending 55 dB quiet, high bands in the body 42 dB quiet, high bands in the first 20 ms 21 dB loud; level 5.4 dB off quieter from 85 to 850 ms; glide 37% off, low; 245 cents sharp; 17% long |
| 10 | MISS | MISS | map: high bands in the body 28 dB loud, high bands in the first 20 ms 25 dB loud, mid bands in the body 22 dB loud; level 37.2 dB off quieter from 0 to 430 ms; glide 14% off, high; attack 8-16 kHz 4 dB short; peak-to-loudness 2.2 dB lower |
| 11 | MISS | MISS | map: high bands in the body 44 dB quiet, low bands in the ending 34 dB quiet, mid bands in the body 31 dB quiet; level 25.1 dB off quieter from 190 to 430 ms; glide 52% off, low |
| 12 | MISS | MISS | map: high bands in the ending 49 dB quiet, mid bands in the ending 29 dB quiet, high bands in the body 25 dB loud; level 14.1 dB off quieter from 0 to 575 ms; glide 25% off, high; peak-to-loudness 1.4 dB lower |
| 13 | MISS | MISS | map: high bands in the body 14 dB loud, high bands in the first 20 ms 12 dB quiet, mid bands in the body 10 dB loud; level 5.8 dB off louder from 215 to 430 ms; glide 32% off, low; attack 3-8 kHz 9 dB short; peak-to-loudness 3.0 dB lower |
| 14 | MISS | MISS | map: mid bands in the body 29 dB loud, high bands in the body 28 dB loud, high bands in the first 20 ms 14 dB loud; level 4.4 dB off louder from 230 to 440 ms; glide 31% off, low; attack 3-8 kHz 5 dB short; peak-to-loudness 2.7 dB lower |
| 15 | MISS | MISS | map: high bands in the first 20 ms 19 dB quiet, high bands in the body 14 dB loud, mid bands in the first 20 ms 13 dB loud; level 4.8 dB off louder from 35 to 615 ms; glide 66% off, low; peak-to-loudness 4.7 dB lower |
| 16 | MISS | MISS | map: mid bands in the body 20 dB loud, mid bands in the ending 16 dB loud, low bands in the body 14 dB loud; level 13.3 dB off louder from 0 to 410 ms; glide 45% off, low; attack 1-3 kHz 9 dB loud; attack 8-16 kHz 7 dB short; peak-to-loudness 3.6 dB lower |
| 17 | MISS | MISS | map: high bands in the first 20 ms 21 dB quiet, mid bands in the first 20 ms 13 dB quiet, mid bands in the body 12 dB quiet; level 10.6 dB off louder from 165 to 405 ms; glide 12% off, low; peak-to-loudness 3.0 dB lower |
| 18 | MISS | MISS | map: mid bands in the body 15 dB loud, low bands in the first 20 ms 14 dB loud, high bands in the body 12 dB loud; level 5.6 dB off louder from 185 to 210 ms; glide 57% off, low |
| 19 | MISS | MISS | map: mid bands in the body 17 dB loud, high bands in the body 12 dB loud, mid bands in the first 20 ms 11 dB loud; level 5.4 dB off louder from 140 to 330 ms; glide 22% off, low; peak-to-loudness 2.6 dB lower |
| 20 | MISS | MISS | map: mid bands in the body 17 dB loud, high bands in the body 10 dB quiet, low bands in the body 9 dB loud; level 5.9 dB off louder from 105 to 420 ms; glide 47% off, low; peak-to-loudness 3.3 dB lower |

Candidates, ranked by the kicks whose misses each could fix:

| Candidate | Kicks | Which |
|---|---|---|
| GLIDE (pitch glide off by 11 to 68 %, Duster always too low but 06, 07, 10, 12) | 20 | all |
| DECAY (5 or more level-curve misses during the body) | 17 | 01 02 03 05 06 07 09 10 11 12 13 14 15 16 17 19 20 |
| NOISE (attack short, or the top quiet in the first 20 ms) | 10 | 02 04 05 10 13 14 15 16 17 19 |
| The ending (level misses or energy misses in the ending) | 7 | 02 03 09 11 12 16 18 |
| Rumble (low bands quiet in the body or tail while the level curve passes) | 2 | 04 08 |
| The ring (tonal lines) | 0 | none |

Misses that fit no row:
- High bands too loud during the body (16 kicks) and mid bands too loud during the body (13): the fitter reaches
  for DRIVE, BITS, RATE and RING to get some top end, and they fill the whole body. NOISE (top end from the
  start) and DECAY (less body) are the likely cure; to be checked after each.
- Peak-to-loudness too low (16 kicks): Duster's body holds its level where the pack's falls, so DECAY should
  close most of these.
- 09 settles 245 cents sharp and runs 17 % long, and 10 runs 11 % long. 09's pitch rises again around 50 ms
  (a named exception in the spec).

Rumble reaches only 2 kicks, under the three-kick bar, so Task 11 is skipped unless ROOM misses tails after
DECAY. The ring has no tonal-line misses, so it needs no work.
