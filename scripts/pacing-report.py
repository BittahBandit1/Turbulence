#!/usr/bin/env python3
"""Summarise a pacing-N.csv from the frame pacing recorder.

On-glass intervals measure displayed pacing. Submission lateness compares
the host deadline with submission time; it does not alone measure input lag
or prove that rendered frames are queued.
"""
from pathlib import Path
import statistics
import sys


def main(path):
    glass, late, submissions, waits, flushes = [], [], [], [], []
    counters = []
    early = []
    skipped = skip_run = longest_skip_run = 0
    for line in Path(path).read_text().splitlines():
        parts = line.strip().split(",")
        if parts[0] == "glass":
            glass.append(float(parts[1]))
        elif parts[0].isdigit() and len(parts) >= 8:
            intended, actual = float(parts[4]), float(parts[5])
            # Recovery skips and other early returns have no actual timestamp.
            # These never reach Metal, so its dropped count cannot see them.
            if actual == -1.0 and parts[7] == "3":
                skipped += 1
                skip_run += 1
                longest_skip_run = max(longest_skip_run, skip_run)
            else:
                skip_run = 0
                submissions.append(actual)
            if len(parts) >= 13:
                waits.append(float(parts[8]))
                flushes.append(float(parts[9]))
                counters.append(tuple(int(p) for p in parts[10:13]))
            if len(parts) >= 14:
                early.append(int(parts[13]))
            if intended >= 0 and actual >= 0:
                late.append(actual - intended)
    # -1 marks a dropped frame; other values may be offset (display clock).
    shown = [t for t in glass if t != -1.0]
    dropped = len(glass) - len(shown)
    iv = [b - a for a, b in zip(shown, shown[1:])]
    print(f"{path}: unsubmitted {skipped}, longest unsubmitted run {longest_skip_run}")
    if len(submissions) > 1:
        intervals = [b - a for a, b in zip(submissions, submissions[1:])]
        print(f"  CPU submission intervals (not display timing): mean {statistics.mean(intervals):.3f} ms  "
              f"sd {statistics.pstdev(intervals):.3f}  max {max(intervals):.3f}")
    if waits:
        print(f"  drawable wait ms: mean {statistics.mean(waits):.3f}  max {max(waits):.3f}; "
              f"submission work ms: mean {statistics.mean(flushes):.3f}  max {max(flushes):.3f}")
        if len(counters) > 1:
            delta = tuple(b - a for a, b in zip(counters[0], counters[-1]))
            print(f"  backend deltas: drawable requests {delta[0]}, unavailable {delta[1]}, "
                  f"display callbacks {delta[2]}")
    if len(early) > 1:
        print(f"  early render submissions: {early[-1] - early[0]}")
    if not iv:
        print(path, f"no on-glass intervals ({len(glass)} callbacks; "
              f"{dropped} reported not displayed)")
        return
    frame = 1000 / 59.94
    on = sum(1 for d in iv if abs(d - frame) <= 1.0)
    short = sum(1 for d in iv if d < frame - 1.0)
    long_ = sum(1 for d in iv if d > frame + 1.0)
    hitch = sum(1 for d in iv if d > 2 * frame - 1.0)
    q = statistics.quantiles(iv, n=100) if len(iv) > 1 else [iv[0]] * 99
    print(f"{path}: {len(iv)} intervals, dropped {dropped}")
    print(f"  on cadence (16.68 +/- 1 ms): {on} ({100 * on / len(iv):.1f}%)  "
          f"short: {short}  long: {long_}  hitches (>=2 frames): {hitch}")
    print(f"  interval ms: mean {statistics.mean(iv):.3f}  sd {statistics.pstdev(iv):.3f}  "
          f"p1 {q[0]:.2f}  p50 {q[49]:.2f}  p99 {q[98]:.2f}  max {max(iv):.2f}")
    if late:
        lq = statistics.quantiles(late, n=100) if len(late) > 1 else [late[0]] * 99
        print(f"  submit lateness ms: p50 {lq[49]:.2f}  p99 {lq[98]:.2f}  max {max(late):.2f}")


if __name__ == "__main__":
    for p in sys.argv[1:]:
        main(p)
