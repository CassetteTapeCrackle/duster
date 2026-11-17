"""Compare a Duster render with a reference kick, metric by metric, against the spec's gate.

    python3 -I tools/compare_kick.py <render.wav> <reference.wav> [--python-rebuild <wav>]

Both files are decoded and resampled to 44.1 kHz mono by ffmpeg, then measured the way
docs/research/resynth05.py measured Kick_05. Levels must land within 2 dB of the reference and
pitch within 5 %; the attack bands are best effort (3 dB target) and never fail the run. Exits 1
when a must-pass metric fails.
"""

import argparse
import subprocess
import sys

import numpy as np

SR = 44100
LEVEL_DB = 2.0
ATTACK_DB = 3.0
PITCH_SHARE = 0.05
PITCH_MS = (3.5, 9.0, 20.0, 35.0)


def load(path):
    raw = subprocess.run(["ffmpeg", "-loglevel", "error", "-i", path, "-f", "f32le", "-ac", "1", "-ar", str(SR), "-"],
                         capture_output=True, check=True).stdout
    return np.frombuffer(raw, dtype=np.float32).astype(np.float64)


def band_db(seg, lo, hi, ref_lo=30, ref_hi=150):
    spec = np.abs(np.fft.rfft(seg * np.hanning(len(seg)), 16384))
    f = np.fft.rfftfreq(16384, 1 / SR)
    rms = lambda a, b: np.sqrt(np.mean(spec[(f >= a) & (f < b)] ** 2))
    return 20 * np.log10(rms(lo, hi) / rms(ref_lo, ref_hi))


def pitch_at(x, ms):
    """Pitch from the period between upward zero crossings, read at each crossing pair's midpoint.

    A centred 15-tap Hann smoother (zero phase) goes first: the crusher's hold puts every crossing
    on a 0.1 ms grid, which alone moves a 3 ms period by about 3 %.
    """
    kernel = np.hanning(15)
    x = np.convolve(x, kernel / kernel.sum(), mode="same")
    zc = np.where((x[:-1] < 0) & (x[1:] >= 0))[0]
    zc = zc + (-x[zc] / (x[zc + 1] - x[zc]))
    mid_ms = (zc[:-1] + zc[1:]) / 2 / SR * 1000
    hz = SR / np.diff(zc)
    return float(np.interp(ms, mid_ms, hz))


def metrics(x):
    """Name -> (value, kind): kind is 'level' (dB), 'attack' (dB, best effort) or 'pitch' (Hz)."""
    x = x[:int(.40 * SR)]
    tail = x[int(.10 * SR):int(.36 * SR)]
    n = 1 << 18
    spec = np.abs(np.fft.rfft(tail * np.blackman(len(tail)), n))
    f = np.fft.rfftfreq(n, 1 / SR)
    ref = spec.max()
    near = lambda c, w=40: 20 * np.log10(spec[np.abs(f - c) < w].max() / ref)
    low = (f > 30) & (f < 80)
    f0 = f[low][np.argmax(spec[low])]
    attack, body = x[:int(.01 * SR)], x[int(.15 * SR):int(.35 * SR)]
    m = {"f0 Hz": (f0, "pitch")}
    for k in (2, 3, 5):
        m[f"h{k} dB"] = (near(k * f0, 3), "level")
    m["line 9600-f0 dB"] = (near(9600 - f0), "level")
    m["line 19200-f0 dB"] = (near(19200 - f0), "level")
    for lo, hi in ((1000, 3000), (3000, 8000), (8000, 16000)):
        m[f"attack {lo // 1000}-{hi // 1000}k dB"] = (band_db(attack, lo, hi), "attack")
    for lo, hi in ((1000, 3000), (3000, 8000)):
        m[f"body {lo // 1000}-{hi // 1000}k dB"] = (band_db(body, lo, hi), "level")
    for ms in PITCH_MS:
        m[f"pitch @{ms:g} ms Hz"] = (pitch_at(x, ms), "pitch")
    return m


def verdict(value, ref, kind):
    if kind == "pitch":
        ok = abs(value / ref - 1) <= PITCH_SHARE
        return "PASS" if ok else "FAIL", ok
    limit = ATTACK_DB if kind == "attack" else LEVEL_DB
    ok = abs(value - ref) <= limit
    if kind == "attack":
        return ("ok" if ok else "short"), True  # best effort: reported, never fails the run
    return "PASS" if ok else "FAIL", ok


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("render")
    parser.add_argument("reference")
    parser.add_argument("--python-rebuild", help="also show docs/research/resynth05.py's render for context")
    args = parser.parse_args()

    render, reference = metrics(load(args.render)), metrics(load(args.reference))
    rebuild = metrics(load(args.python_rebuild)) if args.python_rebuild else None
    header = f"{'metric':22s} {'render':>9s} {'reference':>10s} {'diff':>7s}  verdict"
    print(header + (f" {'python':>9s}" if rebuild else ""))
    all_ok = True
    for name, (value, kind) in render.items():
        ref = reference[name][0]
        word, ok = verdict(value, ref, kind)
        all_ok &= ok
        diff = f"{(value / ref - 1) * 100:+6.1f}%" if kind == "pitch" else f"{value - ref:+6.1f} "
        line = f"{name:22s} {value:9.1f} {ref:10.1f} {diff:>7s}  {word}"
        if rebuild:
            line += f" {rebuild[name][0]:9.1f}"
        print(line)
    print("gate:", "PASS" if all_ok else "FAIL")
    return 0 if all_ok else 1


if __name__ == "__main__":
    sys.exit(main())
