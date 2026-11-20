"""syncd under test, with the mock SAI in place of the vendor's.

LD_PRELOAD puts the mock's sai_* functions ahead of those of whatever SAI
library syncd links (libsaivs, or a vendor's libsai), so any syncd binary runs
on the mock, as long as it was built with the same SAI headers.
"""

import os
import signal
import subprocess
import time

from .mock import MockControl

READY = r"syncd listening for events|starting main loop"


class SyncdError(Exception):
    pass


class Syncd(object):
    def __init__(self, cmd, args, mock_lib, workdir, syslog, ld_library_path=None, profile=None, cpus=None):
        self.cmd = cmd
        self.args = list(args)
        self.mock_lib = mock_lib
        self.workdir = workdir
        self.syslog = syslog
        self.ld_library_path = ld_library_path
        self.profile = dict(profile or {})
        self.cpus = cpus
        self.socket = os.path.join(workdir, "saimock.sock")
        self.profile_path = os.path.join(workdir, "sai.profile")
        self.proc = None
        self.out = None
        self.log_mark = 0
        self.mock = MockControl(self.socket)

    def start(self, timeout=60):
        profile = {"SAI_MOCK_CONTROL_SOCKET": self.socket}
        profile.update(self.profile)
        with open(self.profile_path, "w") as f:
            for key, value in profile.items():
                f.write("{}={}\n".format(key, value))
        env = dict(os.environ)
        env["LD_PRELOAD"] = self.mock_lib
        # an empty path means the system's libraries, not the caller's path
        if self.ld_library_path:
            env["LD_LIBRARY_PATH"] = self.ld_library_path
        else:
            env.pop("LD_LIBRARY_PATH", None)
        cmd = [self.cmd] + self.args + ["-p", self.profile_path]
        if self.cpus:
            cmd = ["taskset", "-c", self.cpus] + cmd
        self.log_mark = self.syslog.mark()
        self.out = open(os.path.join(self.workdir, "syncd.out"), "w")
        self.proc = subprocess.Popen(cmd, stdout=self.out, stderr=subprocess.STDOUT, cwd=self.workdir, env=env)
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            if self.proc.poll() is not None:
                raise SyncdError("syncd exited with {} at start: {}".format(self.proc.returncode, self.output()))
            if self.mock.ping() and self.syslog.since(self.log_mark, READY):
                return
            time.sleep(0.05)
        raise SyncdError("syncd not ready in {} s: {}".format(timeout, self.output()))

    @property
    def pid(self):
        return self.proc.pid

    def alive(self):
        return self.proc is not None and self.proc.poll() is None

    def output(self, lines=20):
        try:
            with open(os.path.join(self.workdir, "syncd.out")) as f:
                return " | ".join(f.read().splitlines()[-lines:])
        except OSError:
            return ""

    def log(self, pattern=None):
        """What syncd logged since it started."""
        return self.syslog.since(self.log_mark, pattern)

    def stop(self, timeout=10):
        if self.proc is None:
            return
        if self.proc.poll() is None:
            self.proc.send_signal(signal.SIGTERM)
            try:
                self.proc.wait(timeout=timeout)
            except subprocess.TimeoutExpired:
                self.proc.kill()
                self.proc.wait()
        self.proc = None
        if self.out is not None:
            self.out.close()
            self.out = None
