"""The load client, saibench (saibench/saibench.cpp), as a subprocess."""

import json
import os
import select
import subprocess


class ClientError(Exception):
    pass


class Client(object):
    def __init__(self, cmd, workdir, env=None, cpus=None):
        self.cmd = list(cmd)
        if cpus:
            self.cmd = ["taskset", "-c", cpus] + self.cmd
        self.workdir = workdir
        self.env = env
        self.proc = None
        self.stderr = None

    def start(self):
        self.stderr = open(os.path.join(self.workdir, "saibench.err"), "w")
        self.proc = subprocess.Popen(self.cmd, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                     stderr=self.stderr, cwd=self.workdir, env=self.env,
                                     universal_newlines=True, bufsize=1)

    @property
    def pid(self):
        return self.proc.pid

    def run(self, line, timeout=600, check=True):
        """Executes one command, returns its JSON reply."""
        if self.proc.poll() is not None:
            raise ClientError("saibench exited with {}".format(self.proc.returncode))
        self.proc.stdin.write(line + "\n")
        self.proc.stdin.flush()
        ready, _, _ = select.select([self.proc.stdout], [], [], timeout)
        if not ready:
            raise ClientError("{}: no reply in {} s".format(line, timeout))
        reply_line = self.proc.stdout.readline()
        if not reply_line:
            raise ClientError("{}: saibench exited with {}".format(line, self.proc.wait()))
        reply = json.loads(reply_line)
        if check and not reply.get("ok"):
            raise ClientError("{}: {}".format(line, reply))
        return reply

    def stop(self):
        if self.proc is None:
            return
        try:
            if self.proc.poll() is None:
                self.proc.stdin.write("quit\n")
                self.proc.stdin.flush()
                self.proc.wait(timeout=10)
        except (OSError, subprocess.TimeoutExpired):
            self.proc.kill()
            self.proc.wait()
        self.proc = None
        if self.stderr is not None:
            self.stderr.close()
            self.stderr = None
