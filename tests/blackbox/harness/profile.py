"""CPU profiles of the benchmark's processes, with perf, one per measured phase.

`perf record` samples the process (all its threads, DWARF call chains) for the
duration of a phase; the samples are then folded into one line per call stack,
root first, as flamegraph.pl and speedscope read them, and summarized: the
functions that take the most CPU themselves (self) and with what they call
(inclusive).

As a command, it summarizes folded files:

    python3 harness/profile.py work/<test>/<phase>.syncd.folded [--top 25] [--match REGEX]...
"""

import argparse
import collections
import os
import re
import shutil
import signal
import subprocess
import sys
import time

# perf script -F comm,tid,time,period,event,ip,sym,dso: a sample's header,
# then one line per frame, leaf first, then a blank line
HEADER = re.compile(r"^\s*(\S.*?)\s+(\d+)\s+(\d+)\.(\d+):\s+(\d+)\s+\S+:")
FRAME = re.compile(r"^\s+[0-9a-f]+\s+(.*?)\s+\((.*)\)\s*$")
OFFSET = re.compile(r"\+0x[0-9a-f]+$")


def find_flamegraph():
    for d in filter(None, [os.environ.get("FLAMEGRAPH_DIR"), "/opt/FlameGraph"]):
        path = os.path.join(d, "flamegraph.pl")
        if os.path.exists(path):
            return path
    return shutil.which("flamegraph.pl")


def check_perf():
    """Why perf cannot profile here, or None."""
    if shutil.which("perf") is None:
        return "perf is not installed"
    probe = subprocess.run(["perf", "record", "-q", "-e", "cpu-clock", "-o", "/dev/null", "--", "true"],
                           stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, universal_newlines=True)
    if probe.returncode != 0:
        return "perf record fails, the container needs CAP_SYS_ADMIN and CAP_IPC_LOCK: {}".format(
            probe.stderr.strip()[-200:])
    return None


def frame_name(sym, dso):
    sym = OFFSET.sub("", sym)
    if sym in ("[unknown]", ""):
        return "[{}]".format(os.path.basename(dso)) if dso and dso != "unknown" else "[unknown]"
    return sym


def fold(script_output, window_ns=None):
    """{folded stack: CPU ns}, samples, from perf script's output; only the
    samples of the window (start, end), in perf's timestamps, when given."""
    stacks = collections.Counter()
    samples = 0
    comm = None
    period = 0
    frames = []

    def flush():
        nonlocal samples
        if comm is not None:
            stacks[";".join([comm] + frames[::-1])] += period
            samples += 1

    for line in script_output.splitlines():
        if not line.strip():
            flush()
            comm, frames = None, []
            continue
        header = HEADER.match(line)
        if header:
            flush()
            comm, period, frames = header.group(1), int(header.group(5)), []
            if window_ns:
                ns = int(header.group(3)) * 10 ** 9 + int(header.group(4).ljust(9, "0")[:9])
                if not window_ns[0] <= ns <= window_ns[1]:
                    comm = None
            continue
        frame = FRAME.match(line)
        if frame and comm is not None:
            frames.append(frame_name(frame.group(1), frame.group(2)).replace(";", ":"))
    flush()
    return stacks, samples


def read_folded(path):
    stacks = collections.Counter()
    with open(path) as f:
        for line in f:
            stack, _, weight = line.rstrip("\n").rpartition(" ")
            if stack:
                stacks[stack] += int(weight)
    return stacks


def summarize(stacks, top=15):
    """The functions with the most CPU, themselves and inclusive, in % of all."""
    total = sum(stacks.values()) or 1
    own = collections.Counter()
    inclusive = collections.Counter()
    for stack, weight in stacks.items():
        frames = stack.split(";")[1:]
        if not frames:
            continue
        own[frames[-1]] += weight
        for name in set(frames):
            inclusive[name] += weight

    def ranked(counter):
        return [{"function": name, "percent": round(100.0 * weight / total, 2)}
                for name, weight in counter.most_common(top)]

    return {"cpu_s": round(total / 1e9, 4), "self": ranked(own), "inclusive": ranked(inclusive)}


def share(stacks, pattern):
    """% of CPU in stacks with a frame matching pattern."""
    regex = re.compile(pattern)
    total = sum(stacks.values()) or 1
    matched = sum(w for s, w in stacks.items() if any(regex.search(f) for f in s.split(";")[1:]))
    return 100.0 * matched / total


class Profiler(object):
    """perf record of some processes during one phase."""

    def __init__(self, targets, prefix, freq=999, event="cpu-clock", callgraph="dwarf,32768"):
        self.targets = targets
        self.prefix = prefix
        self.freq = freq
        self.event = event
        self.callgraph = callgraph
        self.procs = {}

    def path(self, name, ext):
        return "{}.{}.{}".format(self.prefix, name, ext)

    def start(self):
        for name, pid in self.targets.items():
            log = open(self.path(name, "perf.log"), "w")
            self.procs[name] = (subprocess.Popen(
                # monotonic timestamps, as the harness's, to keep the samples
                # of the phase
                ["perf", "record", "-q", "-e", self.event, "-F", str(self.freq), "--call-graph", self.callgraph,
                 "-k", "CLOCK_MONOTONIC", "-p", str(pid), "-o", self.path(name, "perf.data")],
                stdout=log, stderr=subprocess.STDOUT), log)
        # perf reads the processes' maps before it samples
        time.sleep(0.5)

    def stop(self):
        for proc, _ in self.procs.values():
            if proc.poll() is None:
                proc.send_signal(signal.SIGINT)
        for proc, log in self.procs.values():
            try:
                proc.wait(timeout=60)
            except subprocess.TimeoutExpired:
                proc.kill()
                proc.wait()
            log.close()

    def report(self, top=15, window_ns=None):
        """Folds each target's samples, of the window (start, end) in
        monotonic ns when given, renders its flame graph, and returns their
        summaries."""
        flamegraph = find_flamegraph()
        results = {}
        for name in self.targets:
            data = self.path(name, "perf.data")
            if not os.path.exists(data):
                with open(self.path(name, "perf.log")) as f:
                    raise RuntimeError("perf record of {} wrote no {}: {}".format(name, data, f.read()[-500:]))
            script = subprocess.run(["perf", "script", "--no-inline", "--ns", "-i", data,
                                     "-F", "comm,tid,time,period,event,ip,sym,dso"],
                                    stdout=subprocess.PIPE, stderr=subprocess.PIPE, universal_newlines=True)
            if script.returncode != 0:
                raise RuntimeError("perf script failed on {}: {}".format(data, script.stderr.strip()[-500:]))
            stacks, samples = fold(script.stdout, window_ns)
            folded = self.path(name, "folded")
            with open(folded, "w") as f:
                for stack, weight in sorted(stacks.items()):
                    f.write("{} {}\n".format(stack, weight))
            result = summarize(stacks, top)
            result.update({"samples": samples, "folded": folded})
            if flamegraph and stacks:
                svg = self.path(name, "svg")
                with open(folded) as src, open(svg, "w") as out:
                    subprocess.run(["perl", flamegraph, "--title", os.path.basename(self.prefix) + " " + name,
                                    "--countname", "ns"], stdin=src, stdout=out, stderr=subprocess.DEVNULL)
                result["svg"] = svg
            results[name] = result
        return results


def main(argv):
    parser = argparse.ArgumentParser(description="Summarizes folded stacks")
    parser.add_argument("folded", nargs="+")
    parser.add_argument("--top", type=int, default=25)
    parser.add_argument("--match", action="append", default=[],
                        help="also print the % of CPU in stacks with a frame matching this regex")
    args = parser.parse_args(argv)
    for path in args.folded:
        stacks = read_folded(path)
        summary = summarize(stacks, args.top)
        print("== {} ({} s of CPU)".format(path, summary["cpu_s"]))
        for kind in ("self", "inclusive"):
            print("-- {}".format(kind))
            for entry in summary[kind]:
                print("{:6.2f}%  {}".format(entry["percent"], entry["function"]))
        for pattern in args.match:
            print("{:6.2f}%  stacks matching {}".format(share(stacks, pattern), pattern))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
