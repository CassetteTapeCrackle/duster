"""Prove a folder of Duster renders against the Mathys Lenne pack, kick by kick.

    python3 tools/proof_pack.py RENDER_DIR [--rate 44k|48k|both] [--kicks 01,05,...] [--skip NAME ...]

Pairs `RENDER_DIR/NN <name>_<rate>.wav` with the pack's Kick_NN (files without an `NN ` prefix, such
as INIT, are skipped, and so is each --skip name). Each full report goes to RENDER_DIR/proof/ (it
holds the pack's measured values, so it stays out of the repo). Prints a markdown table of verdicts
and misses, which only name differences.
"""

import argparse
import glob
import json
import os
import re
import sys
from dataclasses import asdict

import kickproof

PACK_DIR = os.environ.get("DUSTER_PACK_DIR",
                          "/Users/Matt/LOCAL/Samples Triés/ONESHOTS/Drums/Kick/Mathys Lenne Kicks")
RENDER_NAME = re.compile(r"^(\d\d) (.+)_(44k|48k)\.wav$")


def pack_kick(nn):
    found = glob.glob(os.path.join(glob.escape(PACK_DIR), f"*Kick_{nn}.wav"))
    if len(found) != 1:
        sys.exit(f"proof_pack: expected one Kick_{nn}.wav in {PACK_DIR}, found {len(found)}")
    return found[0]


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("render_dir")
    parser.add_argument("--rate", choices=("44k", "48k", "both"), default="both")
    parser.add_argument("--kicks", help="comma-separated kick numbers, e.g. 01,05")
    parser.add_argument("--skip", nargs="*", default=[], help="render names to leave out, e.g. '05 TAPS'")
    args = parser.parse_args()
    rates = ("44k", "48k") if args.rate == "both" else (args.rate,)
    wanted = set(args.kicks.split(",")) if args.kicks else None

    renders = {}                  # "NN name" -> {rate: path}
    for path in sorted(glob.glob(os.path.join(glob.escape(args.render_dir), "*.wav"))):
        m = RENDER_NAME.match(os.path.basename(path))
        if not m or m[3] not in rates or f"{m[1]} {m[2]}" in args.skip or (wanted and m[1] not in wanted):
            continue
        renders.setdefault(f"{m[1]} {m[2]}", {})[m[3]] = path
    if not renders:
        sys.exit(f"proof_pack: no 'NN name_<rate>.wav' renders in {args.render_dir}")

    out_dir = os.path.join(args.render_dir, "proof")
    os.makedirs(out_dir, exist_ok=True)
    targets = {}
    print("| kick | " + " | ".join(rates) + " | score | misses |")
    print("|---|" + "---|" * len(rates) + "---|---|")
    for name, paths in renders.items():
        nn = name[:2]
        if nn not in targets:
            ref_path = pack_kick(nn)
            ref = kickproof.load_mono(ref_path)
            targets[nn] = (kickproof.Target(ref), ref, round(kickproof.side_db(ref_path), 1))
        target, ref, side = targets[nn]
        reports = {}
        for rate in rates:
            if rate not in paths:
                continue
            render = kickproof.load_mono(paths[rate])
            report = kickproof.compare(render, target)
            reports[rate] = report
            with open(os.path.join(out_dir, f"{name}_{rate}.json"), "w") as f:
                json.dump({"verdict": report.verdict, "score": report.score, "reference side dB": side,
                           "polarity render/ref": [kickproof.polarity(render), kickproof.polarity(ref)],
                           "measures": [asdict(m) for m in report.measures]}, f, indent=2)
        worst = max(reports.values(), key=lambda r: r.score)
        print(f"| {name} | " + " | ".join(reports[r].verdict if r in reports else "-" for r in rates)
              + f" | {worst.score:.2f} | " + "; ".join(worst.misses()) + " |")
    return 0


if __name__ == "__main__":
    sys.exit(main())
