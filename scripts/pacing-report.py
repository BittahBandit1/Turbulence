#!/usr/bin/env python3
"""Summarise a pacing-N.csv from the frame pacing recorder.

On-glass intervals are what you see; submit latency is how late Dolphin
presented each frame against its intended time (input lag + stutter source).
"""
import statistics
import sys


def main(path):
    glass, late = [], []
    for line in open(path):
        parts = line.strip().split(",")
        if parts[0] == "glass":
            glass.append(float(parts[1]))
        elif parts[0].isdigit():
            intended, actual = float(parts[4]), float(parts[5])
            if intended >= 0 and actual >= 0:
                late.append(actual - intended)
    shown = [t for t in glass if t >= 0]
    dropped = len(glass) - len(shown)
    iv = [b - a for a, b in zip(shown, shown[1:])]
    if not iv:
        print(path, "no on-glass samples")
        return
    frame = 1000 / 59.94
    on = sum(1 for d in iv if abs(d - frame) <= 1.0)
    short = sum(1 for d in iv if d < frame - 1.0)
    long_ = sum(1 for d in iv if d > frame + 1.0)
    hitch = sum(1 for d in iv if d > 2 * frame - 1.0)
    q = statistics.quantiles(iv, n=100)
    print(f"{path}: {len(iv)} intervals, dropped {dropped}")
    print(f"  on cadence (16.68 +/- 1 ms): {on} ({100 * on / len(iv):.1f}%)  "
          f"short: {short}  long: {long_}  hitches (>=2 frames): {hitch}")
    print(f"  interval ms: mean {statistics.mean(iv):.3f}  sd {statistics.pstdev(iv):.3f}  "
          f"p1 {q[0]:.2f}  p50 {q[49]:.2f}  p99 {q[98]:.2f}  max {max(iv):.2f}")
    if late:
        lq = statistics.quantiles(late, n=100)
        print(f"  submit lateness ms: p50 {lq[49]:.2f}  p99 {lq[98]:.2f}  max {max(late):.2f}")


if __name__ == "__main__":
    for p in sys.argv[1:]:
        main(p)
