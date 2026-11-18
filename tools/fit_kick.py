"""Fit Duster's knobs to a pack kick by driving the real engine through DusterRender.

    python3 tools/fit_kick.py NN [--from FILE] [--seed 1] [--budget 1500] [--modes off,room] [--out FILE]

Starts from the line in --from whose name begins with "NN " (INIT when there is none), searches the
knobs with an evolution strategy scored by kickproof.compare at 48 kHz, and prints the best settings
line (named "NN FIT", DusterRender's settings format) and its proof report. RUMBLE MODE is searched
as one run per mode in --modes; TAPS is never fitted and DIVISION stays at its default. The line is
appended to --out when given. Repeatable for a seed.
"""

import argparse
import os
import subprocess
import sys
import tempfile
from concurrent.futures import ProcessPoolExecutor
from dataclasses import dataclass

import numpy as np

import kickproof
import proof_pack

RENDER_BIN = os.environ.get("DUSTER_RENDER", os.path.join(os.path.dirname(__file__), "..", "build", "default",
                                                          "DusterRender"))
MODES = {"off": 0, "room": 1}
ROOM_ONLY = ("rumbleAmount", "rumbleLength")
NEVER_FITTED = ("rumbleMode", "rumbleDivision")
PITCH_MEASURES = ("pitch glide", "settled note")
BASE_HZ = 46.249303       # F#0, Duster's pitch at TUNE 0 once the sweep has settled (KickSettings.h)


@dataclass
class Param:
    id: str
    lo: float
    hi: float
    default: float
    step: float           # 0 means continuous

    @property
    def logarithmic(self):
        """Wide positive ranges (SNAP, LENGTH, TONE, RING PITCH) are searched on a log scale."""
        return self.lo > 0 and self.hi / self.lo >= 8


def param_table(render_bin=RENDER_BIN):
    out = subprocess.run([render_bin, "--list-params"], capture_output=True, text=True, check=True).stdout
    table = {}
    for line in out.splitlines():
        fields = line.split("\t")
        if len(fields) == 5:
            table[fields[0]] = Param(fields[0], *map(float, fields[1:]))
    return table


def _to_unit(x, params):
    return np.array([(np.log(v / p.lo) / np.log(p.hi / p.lo)) if p.logarithmic else (v - p.lo) / (p.hi - p.lo)
                     for v, p in zip(x, params)])


def _from_unit(u, params):
    """Unit cube to knob values, clipped into range and snapped to each knob's step."""
    out = np.empty(u.shape)
    for i, p in enumerate(params):
        v = np.clip(u[..., i], 0.0, 1.0)
        v = p.lo * (p.hi / p.lo) ** v if p.logarithmic else p.lo + v * (p.hi - p.lo)
        if p.step:
            v = np.round(v / p.step) * p.step
        out[..., i] = np.clip(v, p.lo, p.hi)
    return out


def search(score, params, x0, seed, budget):
    """Minimise score over the knobs in params, starting at x0 (knob values).

    score takes a population (one row of knob values per candidate, every value in range and on its
    step) and returns one score per row, so a caller can render a whole generation in one call. The
    search is an elitist evolution strategy on the unit cube (log scale for wide ranges): each
    generation samples around the best point so far, the step grows after a success and shrinks after
    a failure, and half the candidates move only one to three knobs, because the score is close to
    separable and a knob that is already right should stay right. When the step collapses it widens
    again around the best point. numpy only; repeatable for a seed.
    """
    rng = np.random.default_rng(seed)
    d = len(params)
    lam, sigma0 = 10, 0.2
    best_u = _to_unit(np.asarray(x0, dtype=float), params)
    best_x = _from_unit(best_u, params)
    best_f = float(score(best_x[None, :])[0])
    evals, sigma = 1, sigma0
    while evals + lam <= budget:
        z = rng.standard_normal((lam, d))
        for row in z[:lam // 2]:
            row[rng.permutation(d)[rng.integers(1, 4):]] = 0.0
        y = np.clip(best_u + sigma * z, 0.0, 1.0)
        candidates = _from_unit(y, params)
        f = np.asarray(score(candidates), dtype=float)
        evals += lam
        i = int(np.argmin(f))
        if f[i] < best_f:
            best_f, best_x, best_u = float(f[i]), candidates[i].copy(), y[i]
            sigma = min(0.5, sigma * 1.5)
        else:
            sigma *= 0.85
        if sigma < 1e-3:
            sigma = sigma0
    return best_x, best_f


def settings_line(name, values):
    """DusterRender's settings format; whole-number steps print as whole numbers."""
    return name + "\t" + " ".join(f"{k}={v:.0f}" if float(v).is_integer() else f"{k}={v:.6g}"
                                  for k, v in values.items())


def parse_line(line):
    name, _, rest = line.rstrip("\n").partition("\t")
    return name, {k: float(v) for k, v in (token.split("=") for token in rest.split())}


def start_values(nn, from_file, table):
    values = {k: p.default for k, p in table.items()}
    if from_file and os.path.exists(from_file):
        with open(from_file) as f:
            for line in f:
                if line.startswith(f"{nn} "):
                    values.update(parse_line(line)[1])
                    break
    return values


def render_population(lines, tmp):
    """Renders each (name, values) at 48 kHz in one DusterRender call; returns mono 44.1 kHz arrays."""
    path = os.path.join(tmp, "population.tsv")
    with open(path, "w") as f:
        f.write("\n".join(settings_line(name, values) for name, values in lines) + "\n")
    subprocess.run([RENDER_BIN, tmp, "--settings", path, "--rate", "48000"], capture_output=True, check=True)
    return [kickproof.load_mono(os.path.join(tmp, f"{name}_48k.wav")) for name, _ in lines]


def zoom_grid(score, params, x0, levels=3, points=9):
    """Minimise score over two knobs with a grid that zooms in: the first level spans both whole
    ranges, each next level a quarter of the last around the best point. A narrow curved valley (SWEEP
    against SNAP) defeats a step search; a grid cannot step over it."""
    best_x = _from_unit(_to_unit(np.asarray(x0, dtype=float), params), params)
    best_f = float(score(best_x[None, :])[0])
    centre, width = np.full(2, 0.5), 0.5
    for _ in range(levels):
        axis = np.linspace(-1, 1, points)
        u = np.clip(centre + width * np.stack(np.meshgrid(axis, axis), axis=-1).reshape(-1, 2), 0.0, 1.0)
        candidates, first = np.unique(_from_unit(u, params), axis=0, return_index=True)
        f = np.asarray(score(candidates), dtype=float)
        i = int(np.argmin(f))
        if f[i] < best_f:
            best_f, best_x = float(f[i]), candidates[i]
        centre, width = _to_unit(best_x, params), width / 4
    return best_x, best_f


def fit(start, table, mode, seed, budget, target, tmp):
    """The pitch first: TUNE from the reference's settled note (Duster settles on F#0 plus TUNE), then
    SWEEP and SNAP on a zooming grid against the pitch measures alone (nothing else moves the pitch
    much). Then every knob against the whole proof with the remaining budget."""
    values = dict(start, rumbleMode=MODES[mode])
    tune = table["tune"]
    values["tune"] = float(np.clip(np.round(12 * np.log2(target.settled_hz / BASE_HZ) / tune.step) * tune.step,
                                   tune.lo, tune.hi))
    used = [0]

    def scorer(free, measures):
        def score(population):
            lines = []
            for i, row in enumerate(population):
                line = dict(values)
                line.update({p.id: float(v) for p, v in zip(free, row)})
                lines.append((f"c{i:03d}", line))
            used[0] += len(lines)
            reports = [kickproof.compare(x, target) for x in render_population(lines, tmp)]
            if measures is None:
                return [r.score for r in reports]
            return [np.mean([m.mean for m in r.measures if m.name in measures]) for r in reports]
        return score

    glide = [table["sweep"], table["snap"]]
    best, _ = zoom_grid(scorer(glide, PITCH_MEASURES), glide, np.array([values[p.id] for p in glide]))
    values.update({p.id: float(v) for p, v in zip(glide, best)})
    free = [p for k, p in table.items() if k not in NEVER_FITTED and (mode == "room" or k not in ROOM_ONLY)]
    best, best_score = search(scorer(free, None), free, np.array([values[p.id] for p in free]), seed,
                              max(0, budget - used[0]))
    values.update({p.id: float(v) for p, v in zip(free, best)})
    return values, best_score


def fit_in_tmp(mode, job):
    start, table, seed, budget, target = job
    with tempfile.TemporaryDirectory() as tmp:
        return fit(start, table, mode, seed, budget, target, tmp)


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("nn", help="pack kick number, e.g. 05")
    parser.add_argument("--from", dest="from_file", help="settings file to start from")
    parser.add_argument("--seed", type=int, default=1)
    parser.add_argument("--budget", type=int, default=1500, help="renders per rumble mode")
    parser.add_argument("--modes", default="off,room")
    parser.add_argument("--out", help="append the best line to this file")
    parser.add_argument("--ref", help="fit to this file instead of the pack's Kick_NN")
    args = parser.parse_args()

    table = param_table()
    start = start_values(args.nn, args.from_file, table)
    ref = kickproof.load_mono(args.ref or proof_pack.pack_kick(args.nn))
    target = kickproof.Target(ref)
    modes = args.modes.split(",")
    with ProcessPoolExecutor(len(modes)) as pool:        # one process per rumble mode
        results = list(pool.map(fit_in_tmp, modes, [(start, table, args.seed, args.budget, target)] * len(modes)))
    for mode, (values, best) in zip(modes, results):
        print(f"mode {mode}: score {best:.3f}", file=sys.stderr)
    values, best = min(results, key=lambda r: r[1])
    with tempfile.TemporaryDirectory() as tmp:
        line = settings_line(f"{args.nn} FIT", values)
        report = kickproof.compare(render_population([("check", values)], tmp)[0], target)
    print(line)
    for m in report.measures:
        print(f"  {m.name:14s} {'PASS' if m.passed else ('close' if m.close else 'MISS'):5s} {m.detail}")
    print(f"  verdict {report.verdict}, score {report.score:.3f}")
    if args.out:
        with open(args.out, "a") as f:
            f.write(line + "\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())
