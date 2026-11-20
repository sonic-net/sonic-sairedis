#!/usr/bin/env python3
"""Compares two benchmark reports (bench-<label>.json), as of two syncd builds.

    compare.py report/bench-before.json report/bench-after.json

Prints a Markdown table per kind of benchmark: every numeric summary value of
both runs and the change. Object rates are better higher; CPU, latencies, calls
and commands lower.
"""

import json
import sys

# values that are better when higher; the others are better when lower, but
# for utilizations, which are neither
HIGHER_IS_BETTER = ("e2e_per_s", "sdk_per_s", "object_reads_per_s", "received")
NEITHER = ("syncd_busy",)


def load(path):
    with open(path) as f:
        return json.load(f)


def better(key, delta):
    if delta == 0 or key in NEITHER:
        return ""
    return " +" if (delta > 0) == (key in HIGHER_IS_BETTER) else " -"


def main(argv):
    if len(argv) != 3:
        sys.stderr.write(__doc__)
        return 2
    base, new = load(argv[1]), load(argv[2])
    new_results = {(r["kind"], r["scenario"]): r for r in new["results"]}
    print("# {} vs {}".format(base["label"], new["label"]))
    kind = None
    for result in base["results"]:
        other = new_results.get((result["kind"], result["scenario"]))
        if other is None:
            continue
        if result["kind"] != kind:
            kind = result["kind"]
            print("\n## {}\n\n| scenario | value | {} | {} | change |\n|---|---|---:|---:|---:|".format(
                kind, base["label"], new["label"]))
        for key, value in result["summary"].items():
            value2 = other["summary"].get(key)
            if not isinstance(value, (int, float)) or isinstance(value, bool) or not isinstance(value2, (int, float)):
                continue
            if key in ("objects", "bulk", "ports", "batch", "events", "counters", "interval_ms"):
                continue
            # from zero, a change has no percentage
            if value:
                change = "{:+.1f}%".format((value2 - value) / value * 100)
            else:
                change = "n/a" if value2 else "+0.0%"
            print("| {} | {} | {:.4g} | {:.4g} | {}{} |".format(
                result["scenario"], key, value, value2, change, better(key, value2 - value)))
    print("\n+: better, -: worse")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
