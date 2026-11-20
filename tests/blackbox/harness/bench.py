"""One benchmark's device: syncd on the mock SAI, driven by the load client,
measured from every side at once.

For a phase (one client command) the result has
- client: wall time, objects per second, per call latency (what orchagent sees)
- sdk: the calls the mock received, and the window from the first to the
  return of the last (how fast syncd drives the SDK)
- cpu: CPU seconds of syncd, redis and the client, per object, and how busy
  syncd's main thread was over the phase (near 1: syncd is the bottleneck)
- redis: commands executed, per command
"""

import os
import re
import shutil
import time

from .client import Client
from .mock import select, summarize
from .procstat import CpuSnapshot, memory
from .profile import Profiler
from .syncd import Syncd


class Bench(object):
    def __init__(self, env, options, name):
        self.env = env
        self.options = options
        self.name = name
        self.workdir = os.path.join(options.workdir, re.sub(r"[^\w.-]+", "_", name))
        self.syncd = None
        self.client = None
        self.mock = None
        self.init_reply = None
        self.warnings = []
        self.started = False
        self.comm = options.comm
        self.phases = 0

    def start(self, profile=None, syncd_args=None, comm=None, init=True, cpus=None):
        # the first start clears what an earlier run left; a benchmark that
        # starts syncd once per round keeps the files of the rounds before
        if not self.started:
            if os.path.isdir(self.workdir):
                shutil.rmtree(self.workdir)
            os.makedirs(self.workdir)
            self.started = True
        self.env.redis.flushall()
        args = self.options.syncd_args if syncd_args is None else syncd_args
        comm = comm or self.options.comm
        self.comm = comm
        # syncd must speak the client's communication mode
        if "-z" not in args and "-s" not in args:
            args = args + ["-z", comm]
        self.syncd = Syncd(self.options.syncd, args, self.options.mock_lib, self.workdir, self.env.syslog,
                           ld_library_path=self.options.ld_library_path, profile=profile,
                           cpus=self.options.cpus.get("syncd"))
        self.syncd.start()
        self.mock = self.syncd.mock
        env = dict(os.environ)
        # as syncd's: an empty path means the system's libraries
        if self.options.ld_library_path:
            env["LD_LIBRARY_PATH"] = self.options.ld_library_path
        else:
            env.pop("LD_LIBRARY_PATH", None)
        self.client = Client([self.options.saibench], self.workdir, env=env, cpus=self.options.cpus.get("client"))
        self.client.start()
        if init:
            self.init_reply = self.client.run("init comm={}".format(comm), timeout=300)
        return self.init_reply

    def stop(self):
        """Stops the client and syncd; returns what syncd logged at ERR or
        worse, and whether it died."""
        if self.client is not None:
            self.client.stop()
            self.client = None
        errors = []
        if self.syncd is not None:
            alive = self.syncd.alive()
            errors = self.env.syslog.problems(self.syncd.log_mark, worst="ERR")
            self.warnings = self.env.syslog.problems(self.syncd.log_mark, worst="WARNING")
            self.syncd.stop()
            if not alive:
                errors.append("syncd exited during the benchmark: {}".format(self.syncd.output()))
            self.syncd = None
        return errors

    def pids(self):
        return {"syncd": self.syncd.pid, "redis": self.env.redis.proc.pid, "client": self.client.pid}

    def snapshot(self):
        return CpuSnapshot(self.pids(), threads_of=self.syncd.pid)

    def profiler(self, name):
        """A perf profiler of the processes --bench-perf names, for one phase, or None."""
        if not self.options.perf:
            return None
        self.phases += 1
        prefix = os.path.join(self.workdir, "{:02d}-{}".format(self.phases, re.sub(r"[^\w.-]+", "_", name)))
        pids = self.pids()
        return Profiler({p: pids[p] for p in self.options.perf}, prefix, freq=self.options.perf_freq,
                        event=self.options.perf_event, callgraph=self.options.perf_callgraph)

    def measure(self, name, action, ops=(("*", "*"),), objects=None, timeout=600):
        """Runs one client command, or a function returning a reply, and
        measures it."""
        def run():
            reply = action() if callable(action) else self.client.run(action, timeout=timeout)
            # in zmq_sync, syncd answered before a thread of its own wrote
            # ASIC_DB: the phase ends when redis went idle
            end_ns = self.env.redis.wait_idle() if self.comm == "zmq_sync" else None
            return reply, end_ns
        return self.measured(name, run, ops, objects)

    def measure_window(self, name, seconds, ops=(("*", "*"),), objects=None):
        """Measures an idle window: what syncd does on its own, as polling counters."""
        def run():
            time.sleep(seconds)
            return {}, None
        return self.measured(name, run, ops, objects)

    def measured(self, name, run, ops, objects):
        """Measures run(), a phase returning the client's reply and when it
        ended, or None when run returns: CPU, SDK calls and redis commands
        over the same window, and its profiles with --bench-perf."""
        profiler = self.profiler(name)
        if profiler:
            profiler.start()
        try:
            self.mock.reset()
            commands = self.env.redis.command_stats()
            w0 = time.monotonic_ns()
            before = self.snapshot()
            t0 = time.monotonic_ns()
            reply, t1 = run()
            t1 = t1 or time.monotonic_ns()
            after = self.snapshot()
            w1 = time.monotonic_ns()
            stats = self.mock.stats()
            after_commands = self.env.redis.command_stats()
        finally:
            # after the measurements, which perf's exit must not extend, and
            # also when the phase failed, so perf does not stay attached
            if profiler:
                profiler.stop()
        result = self.result(name, reply, stats, before, after, commands, after_commands, (t1 - t0) / 1e9, ops,
                             reply.get("objects") if objects is None else objects)
        if profiler:
            # the samples between the CPU snapshots, which perf's start and
            # stop are outside of
            result["profile"] = profiler.report(window_ns=(w0, w1))
        return result

    def result(self, name, reply, stats, before, after, commands, after_commands, wall_s, ops, objects):
        selected = []
        for op, object_type in ops:
            for entry in select(stats, op, object_type):
                if entry not in selected:
                    selected.append(entry)
        sdk = summarize(selected)
        sdk_all = summarize(stats["stats"])
        cpu, threads = after.delta(before)
        objects = objects or sdk["objects"] or 0
        main = sum(t["cpu_s"] for t in threads if t["tid"] == self.syncd.pid)
        redis_commands = {k: v - commands.get(k, 0) for k, v in after_commands.items() if v - commands.get(k, 0) > 0}
        # the harness's own INFO
        redis_commands.pop("info", None)
        redis_total = sum(redis_commands.values())

        def per_object(seconds):
            return round(seconds / objects * 1e6, 3) if objects and seconds is not None else None

        client = {k: reply[k] for k in ("wall_s", "objects_per_s", "errors", "latency") if k in reply}
        return {
            "name": name,
            "reply": reply,
            "objects": objects,
            "wall_s": round(wall_s, 6),
            "client": client,
            "sdk": sdk,
            "sdk_all": {k: sdk_all[k] for k in ("calls", "objects", "sdk_s", "delay_s")},
            "cpu": {
                "syncd_s": cpu.get("syncd"),
                "redis_s": cpu.get("redis"),
                "client_s": cpu.get("client"),
                "syncd_us_per_object": per_object(cpu.get("syncd")),
                "redis_us_per_object": per_object(cpu.get("redis")),
                "client_us_per_object": per_object(cpu.get("client")),
                "syncd_main_busy": round(main / wall_s, 3) if wall_s > 0 else None,
                "syncd_percent": round(100.0 * cpu["syncd"] / wall_s, 1) if wall_s > 0 and cpu.get("syncd") is not None else None,
                "redis_percent": round(100.0 * cpu["redis"] / wall_s, 1) if wall_s > 0 and cpu.get("redis") is not None else None,
            },
            "syncd_threads": threads[:6],
            "redis_commands_total": redis_total,
            "redis_commands": dict(sorted(redis_commands.items(), key=lambda kv: -kv[1])[:10]),
            "syncd_memory": memory(self.syncd.pid),
        }
