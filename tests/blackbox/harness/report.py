"""Results of a benchmark run: a JSON document with everything measured, and
a Markdown summary, one table per kind of benchmark."""

import datetime
import json
import os
import platform
import statistics
import subprocess


def git_revision(path):
    try:
        return subprocess.check_output(["git", "-C", path, "describe", "--always", "--dirty"],
                                       stderr=subprocess.DEVNULL, universal_newlines=True).strip()
    except (OSError, subprocess.CalledProcessError):
        return None


def cpu_model():
    try:
        with open("/proc/cpuinfo") as f:
            for line in f:
                if line.startswith("model name"):
                    return line.split(":", 1)[1].strip()
    except OSError:
        pass
    return platform.processor()


def median_summary(summaries):
    """Median of every numeric value over rounds; other values from the
    first round."""
    merged = {}
    for key, value in summaries[0].items():
        values = [s.get(key) for s in summaries]
        if all(isinstance(v, (int, float)) and not isinstance(v, bool) for v in values):
            median = statistics.median(values)
            merged[key] = round(median, 3) if isinstance(median, float) else median
        else:
            merged[key] = value
    return merged


class Report(object):
    def __init__(self, label, meta):
        self.label = label
        self.meta = meta
        self.results = []

    def add(self, kind, scenario, summary, rounds=None):
        self.results.append({"kind": kind, "scenario": scenario, "summary": summary, "rounds": rounds or []})

    def write(self, out_dir):
        if not self.results:
            return None
        os.makedirs(out_dir, exist_ok=True)
        base = os.path.join(out_dir, "bench-{}".format(self.label))
        with open(base + ".json", "w") as f:
            json.dump({"label": self.label, "meta": self.meta, "results": self.results}, f, indent=2)
        with open(base + ".md", "w") as f:
            f.write(markdown(self.label, self.meta, self.results))
        return base


def fmt(value):
    if value is None:
        return ""
    if isinstance(value, bool):
        return "yes" if value else "no"
    if isinstance(value, float):
        if value >= 1000:
            return "{:,.0f}".format(value)
        if value >= 100:
            return "{:.1f}".format(value)
        return "{:.3g}".format(value)
    if isinstance(value, int):
        return "{:,}".format(value)
    return str(value)


def markdown(label, meta, results):
    lines = ["# syncd benchmark: {}".format(label), ""]
    for key in ("date", "syncd", "syncd_args", "comm", "revision", "cpu", "cpus", "rounds", "scale"):
        if meta.get(key) not in (None, ""):
            lines.append("- {}: `{}`".format(key, meta[key]))
    kinds = []
    for result in results:
        if result["kind"] not in kinds:
            kinds.append(result["kind"])
    for kind in kinds:
        rows = [r for r in results if r["kind"] == kind]
        keys = []
        for row in rows:
            for key in row["summary"]:
                if key not in keys:
                    keys.append(key)
        lines += ["", "## {}".format(kind), "",
                  "| scenario | " + " | ".join(keys) + " |",
                  "|---|" + "---:|" * len(keys)]
        for row in rows:
            lines.append("| {} | ".format(row["scenario"]) +
                         " | ".join(fmt(row["summary"].get(key)) for key in keys) + " |")
    lines += ["", LEGEND]
    return "\n".join(lines) + "\n"


LEGEND = """Columns, medians over the rounds:

- e2e_per_s: objects per second as the client sees them, request to reply
- sdk_per_s: objects per second syncd drove into the SAI, first call to last return
- p50_us, p99_us: latency of one client call (one object, or one bulk)
- syncd_us, redis_us, client_us: CPU microseconds per object of each process
- syncd_busy: CPU time of syncd's main thread over wall time; near 1, syncd is the bottleneck"""


def run_meta(options, top):
    return {
        "date": datetime.datetime.now().isoformat(timespec="seconds"),
        "syncd": options.syncd,
        "syncd_args": " ".join(options.syncd_args),
        "comm": options.comm,
        "revision": git_revision(top),
        "cpu": "{} x {}".format(os.cpu_count(), cpu_model()),
        "cpus": ",".join("{}={}".format(k, v) for k, v in options.cpus.items()),
        "rounds": options.rounds,
        "scale": options.scale,
    }
