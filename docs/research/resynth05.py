"""Rebuild Kick_05 from measured pitch/amp envelopes, try processing orders, compare top-end metrics to the original."""
import sys, subprocess, wave
import numpy as np

SR = 44100
SYN_SR = 48000  # 9600 = 48000/5: the original was likely processed at 48 kHz
OUT = sys.argv[2]

def load(path):
    raw = subprocess.run(["ffmpeg", "-loglevel", "error", "-i", path, "-f", "f32le", "-ac", "1", "-ar", str(SR), "-"], capture_output=True, check=True).stdout
    return np.frombuffer(raw, dtype=np.float32).astype(np.float64)

def save(path, x):
    pcm = (np.clip(x, -1, 1) * 32767).astype("<i2")
    with wave.open(path, "wb") as w:
        w.setnchannels(1); w.setsampwidth(2); w.setframerate(SYN_SR); w.writeframes(pcm.tobytes())

def band(seg, lo, hi, ref_lo=30, ref_hi=150):
    S = np.abs(np.fft.rfft(seg * np.hanning(len(seg)), 16384)); f = np.fft.rfftfreq(16384, 1 / SR)
    r = lambda a, b: np.sqrt(np.mean(S[(f >= a) & (f < b)] ** 2))
    return 20 * np.log10(r(lo, hi) / r(ref_lo, ref_hi))

def metrics(x):
    tail = x[int(.10 * SR):int(.36 * SR)]
    n = 1 << 18
    S = np.abs(np.fft.rfft(tail * np.blackman(len(tail)), n)); f = np.fft.rfftfreq(n, 1 / SR)
    ref = S.max()
    near = lambda c: 20 * np.log10(S[np.abs(f - c) < 40].max() / ref)
    f0 = f[(f > 30) & (f < 80)][np.argmax(S[(f > 30) & (f < 80)])]
    h = [20 * np.log10(S[np.abs(f - k * f0) < 3].max() / ref) for k in (2, 3, 5)]
    att, body = x[:int(.01 * SR)], x[int(.15 * SR):int(.35 * SR)]
    return (f"h2 {h[0]:4.0f} h3 {h[1]:4.0f} h5 {h[2]:4.0f} | line9.55k {near(9600 - f0):4.0f} 19.15k {near(19200 - f0):4.0f} 15.25k {near(44100 - 28800 - f0):4.0f} | "
            f"attack 1-3k {band(att, 1000, 3000):4.0f} 3-8k {band(att, 3000, 8000):4.0f} 8-16k {band(att, 8000, 16000):4.0f} | "
            f"body 1-3k {band(body, 1000, 3000):4.0f} 3-8k {band(body, 3000, 8000):4.0f}")

# measured on Kick_05: pitch (ms, Hz), first point from the 4-sample initial spike
PITCH = [(0, 2800), (.4, 900), (1.1, 594), (3.5, 323), (8.8, 133), (20, 72), (35, 57), (54, 51), (73, 50), (110, 47.5), (400, 46.9)]
AMP = [(0, .6), (8, 1.0), (100, .95), (340, .7), (370, .5), (392, 0)]

def oscillator(n):
    t = np.arange(n) / SYN_SR * 1000
    pt, pf = zip(*PITCH)
    freq = np.exp(np.interp(t, pt, np.log(pf)))
    at, av = zip(*AMP)
    return -np.sin(2 * np.pi * np.cumsum(freq) / SYN_SR) * np.interp(t, at, av)  # original starts going negative

def saturate(x, drive, bias):
    return (np.tanh(drive * x + bias) - np.tanh(bias)) / np.tanh(drive)

def downsample(x, rate):
    # sample-and-hold at `rate`, rendered at SR with no anti-aliasing (what Redux does with filters off)
    n = np.arange(len(x))
    tick = np.floor(n * rate / SYN_SR)
    return np.interp(tick * SYN_SR / rate, n, x)

def lowpass(x, cutoff):
    # RBJ 2-pole Butterworth
    w = 2 * np.pi * cutoff / SYN_SR; a = np.sin(w) / np.sqrt(2); c = np.cos(w)
    b0, b1, b2, a0, a1, a2 = (1 - c) / 2, 1 - c, (1 - c) / 2, 1 + a, -2 * c, 1 - a
    y = np.zeros_like(x); x1 = x2 = y1 = y2 = 0.0
    for i, v in enumerate(x):
        y[i] = (b0 * v + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2) / a0
        x2, x1, y2, y1 = x1, v, y1, y[i]
    return y

def crush(x, bits):
    q = 2 ** (bits - 1)
    return np.round(x * q) / q

orig = load(sys.argv[1])
print(f"{'original':22s}", metrics(orig[:int(.40 * SR)]))
dry = oscillator(int(.40 * SYN_SR))
drive, bias = 1.5, .1
wet = saturate(dry, drive, bias)
variants = {"sat_redux_lp": lowpass(downsample(wet, 9600), 7500)}
for bits in (6, 8, 10):
    variants[f"sat_redux_{bits}bit_lp"] = lowpass(downsample(crush(wet, bits), 9600), 7500)
for name, y in variants.items():
    y = y / np.abs(y).max() * .92
    path = f"{OUT}/rebuild05_{name}.wav"
    save(path, y)
    print(f"{name:22s}", metrics(load(path)))
