"""CPU time and memory of the processes under measurement, from /proc.

A process's CPU time is its process CPU clock, in nanoseconds, which counts the
threads that exited too: the mock's notification threads run syncd's callbacks,
and live and die within a phase. The clock ticks of /proc/<pid>/stat (10 ms)
are too coarse for phases of a few hundred ms. The busiest threads come from
each thread's schedstat.
"""

import os
import time


def process_cpu(pid):
    """Seconds on CPU of a process, its exited threads included."""
    # the clock id clock_getcpuclockid() gives: CPUCLOCK_SCHED of the pid
    return time.clock_gettime_ns(((~pid) << 3) | 2) / 1e9


def _comm(task_dir):
    try:
        with open(os.path.join(task_dir, "comm")) as f:
            return f.read().strip()
    except OSError:
        return "?"


def thread_cpu(pid):
    """{tid: (name, seconds on CPU)} of the live threads of a process."""
    threads = {}
    task_dir = "/proc/{}/task".format(pid)
    for tid in os.listdir(task_dir):
        path = os.path.join(task_dir, tid)
        try:
            with open(os.path.join(path, "schedstat")) as f:
                ns = int(f.read().split()[0])
        except (OSError, ValueError, IndexError):
            continue
        threads[int(tid)] = (_comm(path), ns / 1e9)
    return threads


def memory(pid):
    """RSS and peak RSS of a process, in MiB."""
    values = {}
    try:
        with open("/proc/{}/status".format(pid)) as f:
            for line in f:
                key, _, rest = line.partition(":")
                if key in ("VmRSS", "VmHWM"):
                    values[key] = int(rest.split()[0]) / 1024.0
    except OSError:
        pass
    return {"rss_mb": round(values.get("VmRSS", 0), 1), "peak_rss_mb": round(values.get("VmHWM", 0), 1)}


class CpuSnapshot(object):
    """CPU time of named processes, and of each syncd thread, at one moment."""

    def __init__(self, pids, threads_of=None):
        self.cpu = {}
        self.threads = {}
        for name, pid in pids.items():
            try:
                self.cpu[name] = process_cpu(pid)
                if pid == threads_of:
                    self.threads = thread_cpu(pid)
            except OSError:
                self.cpu[name] = None

    def delta(self, before):
        """CPU seconds each process used since before, and the busiest
        threads, [{tid, name, cpu_s}]."""
        cpu = {name: round(value - before.cpu[name], 6)
               for name, value in self.cpu.items()
               if value is not None and before.cpu.get(name) is not None}
        threads = []
        for tid, (name, value) in self.threads.items():
            used = value - before.threads.get(tid, (name, 0.0))[1]
            if used > 0:
                threads.append({"tid": tid, "name": name, "cpu_s": round(used, 6)})
        threads.sort(key=lambda t: -t["cpu_s"])
        return cpu, threads
