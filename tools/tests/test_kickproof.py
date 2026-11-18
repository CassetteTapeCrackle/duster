"""Self-checks for tools/kickproof.py on synthetic kicks (numpy only, no files)."""

import os
import sys
import unittest

import numpy as np

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))
import kickproof  # noqa: E402
from kickproof import SR, Measure  # noqa: E402

LINE_HZ = 9600.0


def sweep_hz(t):
    """The synthetic body's pitch: 600 Hz falling to 47 Hz in two stages, like Duster's sweep."""
    span = 12 * np.log2(600 / 47)
    return 47 * 2 ** (span * (0.85 * 2 ** (-t / 0.005) + 0.15 * 2 ** (-t / 0.025)) / 12)


def high_pass(x, hz):
    spec = np.fft.rfft(x)
    spec[np.fft.rfftfreq(len(x), 1 / SR) < hz] = 0
    return np.fft.irfft(spec, len(x))


def band_noise(lo, hi, rms, seconds, seed):
    noise = np.random.default_rng(seed).standard_normal(int(seconds * SR))
    spec = np.fft.rfft(noise)
    f = np.fft.rfftfreq(len(noise), 1 / SR)
    spec[(f < lo) | (f >= hi)] = 0
    out = np.fft.irfft(spec, len(noise))
    return out * rms / np.sqrt(np.mean(out ** 2))


def kick(held=True, line=True, seconds=0.6):
    """A sine sweep with a held-then-falling (or falling) level, a 10 ms noise burst and a 9.6 kHz line."""
    t = np.arange(int(seconds * SR)) / SR
    body = np.sin(2 * np.pi * np.cumsum(sweep_hz(t)) / SR)
    if held:
        amp = np.where(t < 0.25, 1.0, 0.5 * (1 + np.cos(np.pi * np.clip((t - 0.25) / 0.15, 0, 1))))
    else:
        amp = 10 ** (-t / 0.2)  # 20 dB down by 200 ms
    burst = np.random.default_rng(1).standard_normal(len(t)) * 0.1 * np.clip(1 - t / 0.01, 0, 1)
    tone = 10 ** (-55 / 20) * np.sin(2 * np.pi * LINE_HZ * t) if line else 0.0
    return amp * (body + tone) + burst


def measure(report, name):
    return next(m for m in report.measures if m.name == name)


class KickProofTests(unittest.TestCase):
    def test_self_passes(self):
        report = kickproof.compare(kick(), kick())
        self.assertEqual(report.verdict, "PASS")
        self.assertEqual(report.score, 0.0)

    def test_different_families_fail(self):
        self.assertEqual(kickproof.compare(kick(held=False), kick()).verdict, "MISS")

    def test_attack_region_cannot_hide(self):
        ref = kick()
        louder = ref + (10 ** (8 / 20) - 1) * high_pass(ref, 2000) * (np.arange(len(ref)) < 0.02 * SR)
        within = np.concatenate([r for r in kickproof.energy_map_ratios(louder, ref).values()])
        self.assertGreater(np.mean(within <= 1), 0.9)
        self.assertFalse(measure(kickproof.compare(louder, ref), "energy map").passed)

    def test_quiet_cells_get_6_db(self):
        quiet = band_noise(3200, 4000, 10 ** (-50 / 20) * np.sqrt(0.5), 0.6, seed=2)
        ref = kick() + quiet
        for shift_db, passes in ((4, True), (8, False)):
            render = kick() + quiet * 10 ** (shift_db / 20)
            with self.subTest(shift_db=shift_db):
                self.assertEqual(measure(kickproof.compare(render, ref), "energy map").passed, passes)

    def test_missing_line_fails(self):
        report = kickproof.compare(kick(line=False), kick())
        self.assertFalse(measure(report, "tonal lines").passed)

    def test_pitch_holds_on_a_ringy_kick(self):
        # Within 3 % of the true sweep, except inside the -6 dB noise burst's 20 ms, where the glide limit
        # (8 %) holds: no crossing method reads a 600 Hz glide to 3 % through that much noise.
        t = np.arange(int(0.6 * SR)) / SR
        ringy = kick() + 10 ** (-10 / 20) * np.sin(2 * np.pi * 13470 * t)
        ringy += np.random.default_rng(3).standard_normal(len(t)) * 10 ** (-6 / 20) * (t < 0.01)
        at = np.arange(2, 201)
        error = lambda x: np.abs(np.interp(at, *kickproof.pitch_curve(x)) / sweep_hz(at / 1000) - 1)
        self.assertLess(error(kick()).max(), 0.03)
        self.assertLess(error(ringy)[at < 20].max(), 0.08)
        self.assertLess(error(ringy)[at >= 20].max(), 0.03)

    def test_alignment_ignores_preroll_and_soft_onset(self):
        clean = kick()
        preroll = np.zeros(int(0.005 * SR))
        self.assertEqual(kickproof.compare(np.concatenate([preroll, clean]), clean).verdict, "PASS")
        # A 3 ms fade-in really changes the attack; what must hold is everything a drift would break.
        soft = clean * np.clip(np.arange(len(clean)) / (0.003 * SR), 0, 1)
        report = kickproof.compare(np.concatenate([preroll, soft]), clean)
        for name in ("level curve", "pitch glide", "settled note", "length"):
            self.assertTrue(measure(report, name).passed, name)

    def test_close_verdict(self):
        ok = Measure("length", True, True, 0.01, 0.05, "")
        for worst, verdict in ((1.5, "CLOSE"), (3.0, "MISS")):
            missed = Measure("attack", False, worst <= 2, worst * 3.0, 3.0, "")
            with self.subTest(worst=worst):
                self.assertEqual(kickproof.verdict_of([ok, missed]), verdict)


if __name__ == "__main__":
    unittest.main()
