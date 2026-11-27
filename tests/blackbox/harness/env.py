"""The benchmark's environment: redis-server for the SONiC databases and a
syslog sink at /dev/log.

Both take over system paths (/var/run/redis, /dev/log), so the benchmark runs
only in a throwaway container, as root (see conftest.py).
"""

import json
import os
import re
import socket
import subprocess
import threading
import time

try:
    import redis
except ImportError:
    # conftest.py imports this module even when the benchmarks are disabled:
    # check_prerequisites() skips them instead
    redis = None

DB_CONFIG = "/var/run/redis/sonic-db/database_config.json"
REDIS_SOCKET = "/var/run/redis/redis.sock"
REDIS_PORT = 6379
DEV_LOG = "/dev/log"

# name: (id, separator), as in a SONiC device's database_config.json
DATABASES = {
    "APPL_DB": (0, ":"),
    "ASIC_DB": (1, ":"),
    "COUNTERS_DB": (2, ":"),
    "LOGLEVEL_DB": (3, ":"),
    "CONFIG_DB": (4, "|"),
    "PFC_WD_DB": (5, ":"),
    "FLEX_COUNTER_DB": (5, ":"),
    "STATE_DB": (6, "|"),
    "SNMP_OVERLAY_DB": (7, "|"),
    "RESTAPI_DB": (8, "|"),
    "GB_ASIC_DB": (9, ":"),
    "GB_COUNTERS_DB": (10, ":"),
    "GB_FLEX_COUNTER_DB": (11, ":"),
    "CHASSIS_APP_DB": (12, "|"),
    "CHASSIS_STATE_DB": (13, "|"),
    "APPL_STATE_DB": (14, ":"),
}

ASIC_DB = 1
COUNTERS_DB = 2
FLEX_COUNTER_DB = 5

SEVERITIES = ("EMERG", "ALERT", "CRIT", "ERR", "WARNING", "NOTICE", "INFO", "DEBUG")

# a datagram as syslog(3) sends it: <PRI>Mmm dd hh:mm:ss ident[pid]: message
SYSLOG_LINE = re.compile(r"^<(\d+)>\w{3}\s+\d+\s+[\d:]+\s+([^\s:\[]+)")


def parse_syslog(line):
    """(severity name, ident) of a syslog datagram, or (None, None)."""
    match = SYSLOG_LINE.match(line)
    if match is None:
        return None, None
    return SEVERITIES[int(match.group(1)) % 8], match.group(2)


def database_config():
    return {
        "INSTANCES": {
            "redis": {"hostname": "127.0.0.1", "port": REDIS_PORT, "unix_socket_path": REDIS_SOCKET},
        },
        "DATABASES": {name: {"id": db, "separator": sep, "instance": "redis"}
                      for name, (db, sep) in DATABASES.items()},
        "VERSION": "1.0",
    }


def check_prerequisites():
    """Why the benchmark cannot run here, or None."""
    if os.geteuid() != 0:
        return "the benchmark needs root, to own /var/run/redis and /dev/log"
    if subprocess.call(["which", "redis-server"], stdout=subprocess.DEVNULL) != 0:
        return "redis-server is not installed"
    if redis is None:
        return "the redis Python module (python3-redis) is not installed"
    return None


def write_database_config():
    """Uses the image's database_config.json when it has the databases syncd
    needs on our redis, writes one otherwise."""
    try:
        with open(DB_CONFIG) as f:
            config = json.load(f)
        instance = config["INSTANCES"]["redis"]
        dbs = config["DATABASES"]
        if (instance.get("unix_socket_path") == REDIS_SOCKET and instance.get("port") == REDIS_PORT and
                all(dbs.get(name, {}).get("id") == db and dbs[name].get("instance") == "redis"
                    for name, (db, _) in DATABASES.items()
                    if name in ("ASIC_DB", "COUNTERS_DB", "FLEX_COUNTER_DB", "STATE_DB", "CONFIG_DB"))):
            return
    except (OSError, ValueError, KeyError):
        pass
    os.makedirs(os.path.dirname(DB_CONFIG), exist_ok=True)
    with open(DB_CONFIG, "w") as f:
        json.dump(database_config(), f, indent=4)


class SyslogSink(object):
    """Receives what syncd, the mock and the client log, at /dev/log."""

    def __init__(self, path, logfile):
        self.path = path
        self.logfile = logfile
        self.lines = []
        self.lock = threading.Lock()
        self.sock = None
        self.thread = None

    def start(self):
        if os.path.lexists(self.path):
            os.unlink(self.path)
        self.sock = socket.socket(socket.AF_UNIX, socket.SOCK_DGRAM)
        self.sock.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 8 << 20)
        self.sock.bind(self.path)
        os.chmod(self.path, 0o666)
        self.thread = threading.Thread(target=self._run, daemon=True)
        self.thread.start()

    def stop(self):
        if self.sock is not None:
            self.sock.close()
            self.sock = None

    def _run(self):
        with open(self.logfile, "a") as out:
            while True:
                try:
                    data = self.sock.recv(65536)
                except OSError:
                    return
                line = data.decode(errors="replace").rstrip("\n\x00")
                with self.lock:
                    self.lines.append(line)
                out.write(line + "\n")
                out.flush()

    def mark(self):
        with self.lock:
            return len(self.lines)

    def since(self, mark=0, pattern=None):
        with self.lock:
            lines = self.lines[mark:]
        if pattern is None:
            return lines
        regex = re.compile(pattern)
        return [line for line in lines if regex.search(line)]

    def problems(self, mark=0, ident="syncd", worst="WARNING"):
        """Lines of ident logged at severity worst or more severe."""
        limit = SEVERITIES.index(worst)
        found = []
        for line in self.since(mark):
            severity, who = parse_syslog(line)
            if who == ident and severity is not None and SEVERITIES.index(severity) <= limit:
                found.append("{} {}".format(severity, line[line.index(">") + 1:]))
        return found

    def wait_for(self, pattern, mark=0, timeout=60):
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            found = self.since(mark, pattern)
            if found:
                return found[0]
            time.sleep(0.05)
        return None


class RedisServer(object):
    def __init__(self, workdir, cpus=None):
        self.workdir = workdir
        self.cpus = cpus
        self.proc = None

    def start(self):
        os.makedirs(os.path.dirname(REDIS_SOCKET), exist_ok=True)
        cmd = ["redis-server", "--port", str(REDIS_PORT), "--bind", "127.0.0.1",
               "--unixsocket", REDIS_SOCKET, "--unixsocketperm", "777",
               "--save", "", "--appendonly", "no", "--databases", "16",
               "--dir", self.workdir, "--logfile", os.path.join(self.workdir, "redis-server.log")]
        if self.cpus:
            cmd = ["taskset", "-c", self.cpus] + cmd
        self.proc = subprocess.Popen(cmd)
        log = os.path.join(self.workdir, "redis-server.log")
        deadline = time.monotonic() + 10
        while time.monotonic() < deadline:
            # another redis on the same socket would answer too
            if self.proc.poll() is not None:
                raise RuntimeError("redis-server exited with {}, see {}".format(self.proc.returncode, log))
            try:
                if self.client().info("server")["process_id"] == self.proc.pid:
                    return
            except redis.exceptions.ConnectionError:
                pass
            time.sleep(0.05)
        raise RuntimeError("redis-server did not start, see {}".format(log))

    def stop(self):
        if self.proc is not None:
            self.proc.terminate()
            self.proc.wait(timeout=10)
            self.proc = None

    def client(self, db=0):
        return redis.Redis(unix_socket_path=REDIS_SOCKET, db=db, decode_responses=True)

    def flushall(self):
        self.client().flushall()

    def command_stats(self):
        """Calls per redis command since the server started."""
        stats = self.client().info("commandstats")
        return {name[len("cmdstat_"):]: value["calls"] for name, value in stats.items()}

    def wait_idle(self, quiet_s=0.06, timeout_s=60):
        """Waits until redis executed no command, but the INFO of these
        checks, for quiet_s; returns when it went idle, in monotonic ns."""
        deadline = time.monotonic_ns() + int(timeout_s * 1e9)
        last = None
        since = None
        while True:
            calls = sum(n for name, n in self.command_stats().items() if name != "info")
            now = time.monotonic_ns()
            if calls != last:
                last, since = calls, now
            elif now - since >= quiet_s * 1e9:
                return since
            if now > deadline:
                raise RuntimeError("redis still busy after {} s".format(timeout_s))
            time.sleep(0.005)


class BenchEnv(object):
    """redis and the syslog sink, shared by every benchmark of a run."""

    def __init__(self, workdir, redis_cpus=None):
        self.workdir = workdir
        os.makedirs(workdir, exist_ok=True)
        self.redis = RedisServer(workdir, redis_cpus)
        self.syslog = SyslogSink(DEV_LOG, os.path.join(workdir, "syslog.log"))

    def start(self):
        write_database_config()
        os.makedirs("/var/run/sswsyncd", exist_ok=True)
        self.syslog.start()
        self.redis.start()

    def stop(self):
        self.redis.stop()
        self.syslog.stop()
