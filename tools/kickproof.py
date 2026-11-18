"""Prove a Duster render against a reference kick: the whole sound over time, with every miss named.

    python3 tools/kickproof.py RENDER REF [--json]

Both files are decoded to 44.1 kHz, mixed to mono as (L+R)/2, aligned on their onset and measured
relative to their own peak, so the proof judges shape and balance, not absolute level. Exits 0 on
PASS or CLOSE, 1 on MISS. Measures and limits: docs/superpowers/specs/2026-11-18-duster-pack-clones-design.md.
"""

import argparse
import json
import subprocess
import sys
from dataclasses import asdict, dataclass

import numpy as np

SR = 44100

ONSET_DB = -50.0           # with no swing to find, the first sample this far under the peak
LENGTH_DB = -60.0          # the kick ends when its level curve stays under this
BODY_END_DB = -20.0        # the ending starts when the level curve stays under this
ATTACK_SPAN_S = 0.020      # the energy map's first time span
ENV_WINDOW_S = 0.024       # the level curve is the peak over this window: one 42 Hz cycle
ENV_STEP_S = 0.005

MAP_LOUD_DB, MAP_IGNORE_DB = -40.0, -80.0
MAP_LOUD_LIMIT_DB, MAP_QUIET_LIMIT_DB = 3.0, 6.0
MAP_SHARE = 0.9            # in every region, this share of cells must be within its limit
LEVEL_LIMIT_DB, LEVEL_SHARE = 3.0, 0.9
GLIDE_LIMIT, GLIDE_SHARE = 0.08, 0.9
SETTLED_LIMIT_CENTS = 15.0
ATTACK_LIMIT_DB = 3.0
ATTACK_BANDS = ((1000, 3000), (3000, 8000), (8000, 16000))
ATTACK_S = 0.010
PLR_LIMIT_DB = 1.0
LENGTH_LIMIT = 0.05
LINE_FLOOR_DB, LINE_PROMINENCE_DB, LINE_LIMIT_DB, LINE_WIDTH = -70.0, 15.0, 3.0, 0.01
LINE_LOWEST_HZ, LINE_COUNT = 500.0, 12

BAND_GROUPS = (("low bands", 0, 200), ("mid bands", 200, 2000), ("high bands", 2000, 1e9))
SPANS = ("the first 20 ms", "the body", "the ending")


def load_mono(path):
    """44.1 kHz float64, (L+R)/2, via ffmpeg (a mono file is duplicated first, so it comes back as is)."""
    return load_stereo(path).mean(axis=1)


def load_stereo(path):
    raw = subprocess.run(["ffmpeg", "-loglevel", "error", "-i", path, "-f", "f64le", "-ac", "2", "-ar", str(SR), "-"],
                         capture_output=True, check=True).stdout
    return np.frombuffer(raw, dtype=np.float64).reshape(-1, 2)


def side_db(path):
    """Stereo width: side (L-R) RMS under mid (L+R) RMS, in dB. Reported, never judged."""
    x = load_stereo(path)
    mid, side = np.sqrt(np.mean((x[:, 0] + x[:, 1]) ** 2)), np.sqrt(np.mean((x[:, 0] - x[:, 1]) ** 2))
    return 20 * np.log10(side / mid + 1e-12)


@dataclass
class Measure:
    name: str
    passed: bool
    close: bool
    worst: float
    limit: float
    detail: str
    mean: float = 0.0          # mean error over limit across the measure's points: its share of the score


@dataclass
class Report:
    verdict: str              # "PASS" | "CLOSE" | "MISS"
    measures: list
    score: float              # each measure's mean error over its limit, averaged over the measures

    def misses(self):
        """Each failed measure, phrased as a difference from the reference, never its values."""
        return [f"{m.name}: {m.detail}" for m in self.measures if not m.passed]


def verdict_of(measures):
    if all(m.passed for m in measures):
        return "PASS"
    return "CLOSE" if all(m.close for m in measures) else "MISS"


def _db(power_or_amp, floor, power=False):
    return (10 if power else 20) * np.log10(np.maximum(power_or_amp, floor))


def _onset(x):
    """Where the body starts: the zero crossing that opens the first swing of the 1.2 kHz low-passed
    signal reaching -20 dB of its peak. A soft lead-in (13 has 6 ms of it at -25 dB) or a click is not
    the body, and a render that has none must still line up with it."""
    y = _lowpass(x, 1200)
    first = int(np.argmax(np.abs(y) > np.max(np.abs(y)) * 10 ** (BODY_END_DB / 20)))
    flips = np.nonzero(np.sign(y[:first]) != np.sign(y[first]))[0]
    if len(flips):
        return int(flips[-1]) + 1
    peak = np.max(np.abs(x))
    return int(np.argmax(np.abs(x) > peak * 10 ** (ONSET_DB / 20)))


def _lowpass_shape(f, cutoff):
    """Flat to 2/3 of cutoff, then a raised cosine to zero at cutoff."""
    return np.where(f < cutoff * 2 / 3, 1.0, np.where(f < cutoff, 0.5 * (1 + np.cos(np.pi * (3 * f / cutoff - 2))), 0.0))


def _lowpass(x, cutoff):
    """Zero phase low-pass."""
    n = 1 << int(np.ceil(np.log2(2 * len(x))))
    return np.fft.irfft(np.fft.rfft(x, n) * _lowpass_shape(np.fft.rfftfreq(n, 1 / SR), cutoff), n)[:len(x)]


def _prepare(x):
    """Onset-aligned and peak-normalised."""
    x = np.asarray(x, dtype=np.float64)
    x = x[_onset(x):]
    return x / np.max(np.abs(x))


def _level_curve(x, n_points):
    """dB under the peak of the peak over ENV_WINDOW_S, centred, every ENV_STEP_S."""
    half, step = int(ENV_WINDOW_S * SR / 2), int(ENV_STEP_S * SR)
    a = np.abs(np.concatenate([x, np.zeros(n_points * step + half)]))
    centres = np.arange(n_points) * step
    return np.array([_db(a[max(0, c - half):c + half].max(), 1e-6) for c in centres])


def _bands():
    centres = 1000 * 2 ** (np.arange(-17, 14) / 3)    # third octaves from 20 Hz to 20 kHz
    return [(c * 2 ** (-1 / 6), c * 2 ** (1 / 6)) for c in centres]


def _band_power(spec, f, n, lo, hi):
    """Instant power of the band [lo, hi) as the squared magnitude of its analytic signal.

    The band's shape is a raised cosine on a log scale reaching zero at the neighbouring bands'
    centres, not a brick wall: a brick wall smears a click over the whole kick.
    """
    centre, third = np.sqrt(lo * hi), np.log2(hi / lo)
    with np.errstate(divide="ignore"):
        d = np.abs(np.log2(np.maximum(f, 1e-9) / centre)) / (2 * third)
    shape = np.where(d < 1, np.cos(np.pi / 2 * d) ** 2, 0.0)
    one_sided = np.zeros(n, dtype=complex)
    one_sided[:len(f)] = 2 * spec * shape
    return np.abs(np.fft.ifft(one_sided)) ** 2


class _Analysis:
    """Everything measured on one prepared signal over a fixed number of samples."""

    def __init__(self, x, n_samples):
        self.x = np.concatenate([x, np.zeros(max(0, n_samples - len(x)))])[:n_samples]
        n = 1 << int(np.ceil(np.log2(2 * n_samples)))
        self.n = n
        self.spec = np.fft.rfft(self.x, n)
        self.f = np.fft.rfftfreq(n, 1 / SR)

    def band_power(self, lo, hi):
        return _band_power(self.spec, self.f, self.n, lo, hi)[:len(self.x)]


class Target:
    """A reference kick, measured once, so a fitter can compare many renders against it cheaply."""

    def __init__(self, ref):
        x = _prepare(ref)
        n_env = int(len(x) / (ENV_STEP_S * SR)) + 1
        level = _level_curve(x, n_env)
        loud = np.nonzero(level > LENGTH_DB)[0]
        self.length_points = int(loud[-1]) + 1 if len(loud) else n_env
        self.length_s = self.length_points * ENV_STEP_S
        self.n = min(len(x), int(self.length_s * SR))
        self.level = level[:self.length_points]
        body = np.nonzero(self.level > BODY_END_DB)[0]
        self.body_end_s = (int(body[-1]) + 1) * ENV_STEP_S if len(body) else ATTACK_SPAN_S
        self.analysis = _Analysis(x, self.n)
        self.map = _energy_map(self.analysis)
        self.lines = _find_lines(self.analysis.x, self.body_end_s)
        self.pitch = pitch_curve(self.analysis.x, self.body_end_s + 0.05)
        self.settled_hz = _settled(self.analysis.x, self.body_end_s)
        self.attack = _attack(self.analysis)
        self.plr = _plr(self.analysis.x)


def _cells(analysis):
    """Per third-octave band: (cell start times in s, cell power). Steps grow at low frequencies."""
    out = []
    for lo, hi in _bands():
        step = int(np.clip(1 / (hi - lo), 0.005, 0.020) * SR)
        p = analysis.band_power(lo, hi)
        starts = np.arange(0, len(p), step)
        out.append((lo, starts / SR, np.add.reduceat(p, starts) / np.diff(np.append(starts, len(p)))))
    return out


def _energy_map(analysis):
    cells = _cells(analysis)
    top = max(c.max() for _, _, c in cells)
    return [(lo, t, _db(c / top, 1e-12, power=True)) for lo, t, c in cells]


def _region(lo_hz, t, body_end_s):
    group = next(i for i, (_, a, b) in enumerate(BAND_GROUPS) if a <= lo_hz * 2 ** (1 / 6) < b)
    span = np.where(t < ATTACK_SPAN_S, 0, np.where(t < body_end_s, 1, 2))
    return group, span


def _map_ratios(target, analysis):
    """Region (group, span) -> error over limit for every judged cell."""
    out = {}
    for (lo, t, ref_db), (_, _, got_db) in zip(target.map, _energy_map(analysis)):
        judged = ref_db >= MAP_IGNORE_DB
        limit = np.where(ref_db > MAP_LOUD_DB, MAP_LOUD_LIMIT_DB, MAP_QUIET_LIMIT_DB)
        ratio = np.abs(got_db - ref_db) / limit
        signed = got_db - ref_db
        group, span = _region(lo, t, target.body_end_s)
        for s in range(3):
            keep = judged & (span == s)
            if keep.any():
                r, d = out.setdefault((group, s), ([], []))
                r.extend(ratio[keep])
                d.extend(signed[keep])
    return {k: (np.array(r), np.array(d)) for k, (r, d) in out.items()}


def energy_map_ratios(render, ref):
    """Region name -> error over limit for every judged cell of the energy map."""
    target = Target(ref)
    ratios = _map_ratios(target, _Analysis(_prepare(render), target.n))
    return {f"{BAND_GROUPS[g][0]} in {SPANS[s]}": r for (g, s), (r, _) in ratios.items()}


def _find_lines(x, body_end_s):
    """The strongest narrow peaks of the body's spectrum: (Hz, dB under the spectrum's top)."""
    spec_db, f = _body_spectrum(x, body_end_s)
    if spec_db is None:
        return []
    width = max(1, int(round(0.1 * LINE_LOWEST_HZ / (f[1] - f[0]))))
    peaks = []
    for k in np.nonzero((f >= LINE_LOWEST_HZ) & (spec_db > LINE_FLOOR_DB))[0]:
        if k + 1 >= len(spec_db) or spec_db[k] < spec_db[k - 1] or spec_db[k] < spec_db[k + 1]:
            continue
        half = max(width, int(0.1 * k))
        around = np.median(spec_db[max(0, k - half):k + half])
        if spec_db[k] - around >= LINE_PROMINENCE_DB:
            peaks.append((f[k], spec_db[k]))
    peaks.sort(key=lambda p: -p[1])
    chosen = []
    for hz, _ in peaks:           # one line per 1 %: a line's own skirt is not a second line
        if all(abs(hz / c - 1) > LINE_WIDTH for c, _ in chosen):
            chosen.append((hz, _line_level(spec_db, f, hz)))
        if len(chosen) == LINE_COUNT:
            break
    return chosen


def _line_level(spec_db, f, hz):
    """The loudest point within 1 % of hz: a render's line may sit up to 1 % away."""
    return spec_db[np.abs(f - hz) <= LINE_WIDTH * hz].max()


def _body_spectrum(x, body_end_s):
    a, b = int(ATTACK_SPAN_S * SR), int(body_end_s * SR)
    if b - a < int(0.02 * SR):
        return None, None
    seg = x[a:b] * np.blackman(b - a)
    spec = np.abs(np.fft.rfft(seg, 1 << 16))
    return _db(spec / spec.max(), 1e-9), np.fft.rfftfreq(1 << 16, 1 / SR)


def _line_errors(target, x):
    spec_db, f = _body_spectrum(x, target.body_end_s)
    errors = []
    for hz, db in target.lines:
        errors.append((hz, (_line_level(spec_db, f, hz) if spec_db is not None else -180.0) - db))
    return errors


def pitch_curve(x, until_s=None):
    """The body's pitch as (ms, Hz) every 0.5 ms.

    x is low-passed at 1.2 kHz (zero phase), so rings and high tones go. Its zero crossings are the
    skeleton: one counts only after the signal has swung 30 % of its local peak the other way, so
    noise and weak partials cannot add crossings, and each is half a cycle of phase. The pitch at
    each crossing is the slope of a cubic fit of crossing time against phase over seven crossings.
    Where the analytic signal's phase advances exactly as the crossings say (no harmonic winds it),
    a cubic fit of that phase over 1.5 cycles refines it (within 20 %), which a fast glide needs.
    """
    x = np.asarray(x, dtype=np.float64)
    n = 1 << int(np.ceil(np.log2(2 * len(x))))
    spec = np.fft.rfft(x, n)
    f = np.fft.rfftfreq(n, 1 / SR)
    one_sided = np.zeros(n, dtype=complex)
    one_sided[:len(f)] = 2 * spec * _lowpass_shape(f, 1200)
    z = np.fft.ifft(one_sided)[:len(x)]
    t, kinds = _crossings(z.real)
    if len(t) < 4:
        return np.array([0.0, 1.0]), np.array([0.0, 0.0])
    cycles = np.where(kinds[1:] != kinds[:-1], 0.5, 1.0)   # a repeated kind means a missed crossing
    skeleton = np.concatenate([[0.0], np.cumsum(cycles)])
    crossing_hz = np.empty(len(t))
    for i in range(len(t)):
        a, b = max(0, i - 3), min(len(t), i + 4)
        mean_hz = (skeleton[b - 1] - skeleton[a]) / (t[b - 1] - t[a])
        fit = np.polyfit(skeleton[a:b] - skeleton[i], t[a:b], min(3, b - a - 2))
        hz_i = 1 / np.polyval(np.polyder(fit), 0)
        crossing_hz[i] = hz_i if 0.5 < hz_i / mean_hz < 2 else mean_hz  # a short edge fit can run wild
    phase = np.unwrap(np.angle(z)) / (2 * np.pi)
    advance = np.diff(np.interp(t * SR, np.arange(len(x)), phase))
    trusted = np.abs(advance - cycles) < 0.15
    end_ms = 1000 * min(t[-1], until_s if until_s else t[-1])
    grid = np.arange(0.5, end_ms, 0.5)
    hz = np.interp(grid, t * 1000, crossing_hz)
    for i, ms in enumerate(grid):
        half = max(0.0005, 0.75 / hz[i])
        lo, hi = np.searchsorted(t, ms / 1000 - half), np.searchsorted(t, ms / 1000 + half)
        if not trusted[max(0, lo - 1):min(len(trusted), hi)].all():
            continue
        idx = np.arange(max(0, int((ms / 1000 - half) * SR)), min(len(x), int((ms / 1000 + half) * SR) + 1), 2)
        fine = _cubic_slope(idx / SR - ms / 1000, phase[idx])
        if abs(fine / hz[i] - 1) < 0.2:  # a refinement, never a different reading
            hz[i] = fine
    return grid, hz


def _cubic_slope(t, y):
    """Slope at t = 0 of the least-squares cubic through (t, y): np.polyfit's answer, a few times faster
    (a 4 x 4 solve on t scaled to about 1)."""
    scale = np.max(np.abs(t)) or 1.0
    v = np.vander(t / scale, 4, increasing=True)
    return np.linalg.solve(v.T @ v, v.T @ y)[1] / scale


def _crossings(y):
    """Zero crossings of y as (times in s, +1 up / -1 down), sorted."""
    half, hop = int(0.010 * SR), 64
    padded = np.abs(np.concatenate([y, np.zeros(half)]))
    local = np.array([padded[max(0, i - half):i + half].max() for i in range(0, len(y), hop)])
    threshold = 0.3 * np.repeat(local, hop)[:len(y)]
    found = []
    for sign in (1, -1):
        s = sign * y
        last = None
        for i in np.nonzero((s[:-1] < 0) & (s[1:] >= 0))[0]:
            if last is None or (s[last:i + 1] < -threshold[i]).any():
                found.append((i + s[i] / (s[i] - s[i + 1]), sign))
                last = i
    found.sort()
    return np.array([c[0] for c in found]) / SR, np.array([c[1] for c in found])


def _glide_points(target):
    end_ms = min(100.0, target.body_end_s * 1000)
    return np.concatenate([np.arange(1, 20), np.arange(20, end_ms + 1, 5)])


def _settled(x, body_end_s):
    """The settled note: the strongest spectral peak from 20 to 200 Hz from 100 ms to 40 ms before the
    body ends (the second half of a shorter body). A tail rumble adds zero crossings there and beats
    with the body, so this window is as long as the body allows."""
    a, b = int(0.1 * SR), int((body_end_s - 0.04) * SR)
    if b - a < int(0.08 * SR):
        a, b = int(body_end_s / 2 * SR), int(body_end_s * SR)
    seg = x[a:b] * np.blackman(b - a)
    spec = np.abs(np.fft.rfft(seg, 1 << 18))
    f = np.fft.rfftfreq(1 << 18, 1 / SR)
    band = (f >= 20) & (f <= 200)
    return float(f[band][np.argmax(spec[band])])


def _attack(analysis):
    n = int(ATTACK_S * SR)
    return [_db(np.mean(analysis.band_power(lo, hi)[:n]), 1e-15, power=True) for lo, hi in ATTACK_BANDS]


def _plr(x):
    return float(-_db(np.sqrt(np.mean(x ** 2)), 1e-9))   # peak is 0 dB after _prepare


def _share_worst(errors, share):
    """The error that `share` of the points stay within."""
    return float(np.quantile(errors, share)) if len(errors) else 0.0


def compare(render, ref):
    """Prove `render` against `ref` (an array, or a Target for repeated comparisons)."""
    target = ref if isinstance(ref, Target) else Target(ref)
    x = _prepare(render)
    analysis = _Analysis(x, target.n)
    measures, points = [], []

    def add(name, worst, limit, detail, ratios):
        ratios = np.atleast_1d(ratios)
        mean = float(np.mean(ratios)) if len(ratios) else 0.0
        measures.append(Measure(name, bool(worst <= limit), bool(worst <= 2 * limit), float(worst), limit, detail, mean))
        points.append(mean)

    # Energy map: the worst region's 90th percentile of error over limit.
    regions = _map_ratios(target, analysis)
    worst, detail = 0.0, []
    all_ratios = []
    for (g, s), (ratios, signed) in sorted(regions.items()):
        all_ratios.append(ratios)
        q = _share_worst(ratios, MAP_SHARE)
        worst = max(worst, q)
        if q > 1:
            bad = signed[ratios > 1]
            word = "loud" if np.median(bad) > 0 else "quiet"
            detail.append(f"{BAND_GROUPS[g][0]} in {SPANS[s]} {np.median(np.abs(bad)):.0f} dB too {word} "
                          f"({np.mean(ratios > 1) * 100:.0f}% of cells off)")
    add("energy map", worst, 1.0, "; ".join(detail), np.concatenate(all_ratios) if all_ratios else [])

    # Tonal lines.
    lines = _line_errors(target, analysis.x)
    errs = np.array([abs(d) for _, d in lines])
    detail = "; ".join(f"line near {hz / 1000:.2f} kHz {abs(d):.0f} dB {'loud' if d > 0 else 'short'}"
                       for hz, d in lines if abs(d) > LINE_LIMIT_DB)
    add("tonal lines", errs.max() if len(errs) else 0.0, LINE_LIMIT_DB, detail, errs / LINE_LIMIT_DB)

    # Level curve.
    got_level = _level_curve(analysis.x, target.length_points)
    diff = got_level - target.level
    worst = _share_worst(np.abs(diff), LEVEL_SHARE)
    off = np.abs(diff) > LEVEL_LIMIT_DB
    when = (np.nonzero(off)[0] * ENV_STEP_S * 1000) if off.any() else np.array([0.0])
    add("level curve", worst, LEVEL_LIMIT_DB,
        f"{worst:.1f} dB off at 10% of points, {'louder' if np.median(diff[off] if off.any() else diff) > 0 else 'quieter'}"
        f" from {when.min():.0f} to {when.max():.0f} ms", np.abs(diff) / LEVEL_LIMIT_DB)

    # Pitch: the glide and the settled note.
    got_curve = pitch_curve(analysis.x, target.body_end_s + 0.05)
    at = _glide_points(target)
    ratio = np.interp(at, *got_curve) / np.interp(at, *target.pitch) - 1
    worst = _share_worst(np.abs(ratio), GLIDE_SHARE)
    add("pitch glide", worst, GLIDE_LIMIT,
        f"{worst * 100:.0f}% off, {'high' if np.median(ratio) > 0 else 'low'} from 1 to {at[-1]:.0f} ms",
        np.abs(ratio) / GLIDE_LIMIT)
    cents = 1200 * np.log2(_settled(analysis.x, target.body_end_s) / target.settled_hz)
    add("settled note", abs(cents), SETTLED_LIMIT_CENTS,
        f"{abs(cents):.0f} cents {'sharp' if cents > 0 else 'flat'}", abs(cents) / SETTLED_LIMIT_CENTS)

    # Attack bands.
    diff = np.array(_attack(analysis)) - np.array(target.attack)
    detail = "; ".join(f"attack {lo // 1000}-{hi // 1000} kHz {abs(d):.0f} dB {'loud' if d > 0 else 'short'}"
                       for (lo, hi), d in zip(ATTACK_BANDS, diff) if abs(d) > ATTACK_LIMIT_DB)
    add("attack", np.abs(diff).max(), ATTACK_LIMIT_DB, detail, np.abs(diff) / ATTACK_LIMIT_DB)

    # Peak-to-loudness ratio and length.
    d = _plr(analysis.x) - target.plr
    add("loudness", abs(d), PLR_LIMIT_DB, f"peak-to-loudness {abs(d):.1f} dB {'higher' if d > 0 else 'lower'}",
        abs(d) / PLR_LIMIT_DB)
    level = _level_curve(x, int(len(x) / (ENV_STEP_S * SR)) + 1)
    loud = np.nonzero(level > LENGTH_DB)[0]
    length_s = (int(loud[-1]) + 1) * ENV_STEP_S if len(loud) else 0.0
    r = length_s / target.length_s - 1
    add("length", abs(r), LENGTH_LIMIT, f"{abs(r) * 100:.0f}% {'long' if r > 0 else 'short'}", abs(r) / LENGTH_LIMIT)

    return Report(verdict_of(measures), measures, float(np.mean(points)) if points else 0.0)


def polarity(x):
    """+1 when the first swing goes positive, -1 when negative. Reported, never judged."""
    x = _prepare(x)
    first = x[:int(0.005 * SR)]
    big = np.nonzero(np.abs(first) > 0.1)[0]
    return int(np.sign(first[big[0]])) if len(big) else 0


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("render")
    parser.add_argument("reference")
    parser.add_argument("--json", action="store_true", help="print the full report as JSON")
    args = parser.parse_args()
    render, ref = load_mono(args.render), load_mono(args.reference)
    report = compare(render, ref)
    info = {"polarity render/ref": [polarity(render), polarity(ref)], "ref side dB": round(side_db(args.reference), 1)}
    if args.json:
        print(json.dumps({"verdict": report.verdict, "score": report.score, "info": info,
                          "measures": [asdict(m) for m in report.measures]}, indent=2))
    else:
        for m in report.measures:
            word = "PASS" if m.passed else ("close" if m.close else "MISS")
            print(f"{m.name:14s} {word:5s} worst {m.worst:8.3f} limit {m.limit:6.2f}  {m.detail}")
        print(f"info: {info}")
        print(f"verdict: {report.verdict}  score {report.score:.3f}")
    return 1 if report.verdict == "MISS" else 0


if __name__ == "__main__":
    sys.exit(main())
