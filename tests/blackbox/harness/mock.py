"""Client of the mock SAI's control socket (saimock/MockControl.cpp)."""

import json
import socket
import time


class MockError(Exception):
    pass


class MockControl(object):
    def __init__(self, path):
        self.path = path

    def cmd(self, line, timeout=30):
        sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        sock.settimeout(timeout)
        try:
            sock.connect(self.path)
            sock.sendall((line + "\n").encode())
            data = b""
            while not data.endswith(b"\n"):
                chunk = sock.recv(1 << 20)
                if not chunk:
                    break
                data += chunk
        finally:
            sock.close()
        reply = json.loads(data.decode())
        if not reply.get("ok"):
            raise MockError("{}: {}".format(line, reply.get("error")))
        return reply

    def ping(self):
        try:
            return self.cmd("ping", timeout=2)
        except (OSError, ValueError, MockError):
            return None

    def stats(self, reset=False):
        return self.cmd("stats reset" if reset else "stats")

    def reset(self):
        return self.cmd("reset")

    def objects(self):
        return self.cmd("objects")["objects"]

    def config(self):
        return self.cmd("config")

    def delay(self, op, object_type, per_call_ns, per_object_ns=0, sleep=False):
        return self.cmd("delay {} {} {} {} {}".format(op, object_type, int(per_call_ns), int(per_object_ns),
                                                      "sleep" if sleep else "spin"))

    def clear_delays(self):
        return self.cmd("delay clear")

    def notify(self, kind, count, batch=1, rate=0, **extra):
        args = " ".join("{}={}".format(k, v) for k, v in extra.items())
        return self.cmd("notify {} count={} batch={} rate={} {}".format(kind, count, batch, rate, args).strip())

    def wait_notifiers(self, timeout=120):
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            if self.cmd("notify status")["running"] == 0:
                return True
            time.sleep(0.005)
        return False


def select(stats, op=None, object_type=None):
    """Entries of a stats reply for an op and object type ("*" or None: any)."""
    found = []
    for entry in stats["stats"]:
        if op not in (None, "*") and entry["op"] != op:
            continue
        if object_type not in (None, "*") and entry["object_type"] != object_type:
            continue
        found.append(entry)
    return found


def summarize(entries):
    """Totals of stats entries: calls, objects, errors, time inside the mock,
    and the window from the first call to the return of the last. Calls the
    mock refused as unsupported (bulk on an SDK without it, which syncd then
    does one object at a time) count apart."""
    unsupported = sum(e.get("unsupported", 0) for e in entries)
    entries = [e for e in entries if e.get("unsupported", 0) < e["calls"]]
    if not entries:
        return {"calls": 0, "objects": 0, "errors": 0, "unsupported": unsupported, "sdk_s": 0.0, "delay_s": 0.0,
                "first_ns": 0, "last_ns": 0, "window_s": 0.0, "objects_per_s": 0.0}
    first = min(e["first_ns"] for e in entries)
    last = max(e["last_ns"] for e in entries)
    objects = sum(e["objects"] for e in entries)
    window = (last - first) / 1e9
    return {
        "calls": sum(e["calls"] for e in entries),
        "objects": objects,
        "errors": sum(e["errors"] for e in entries),
        "unsupported": unsupported,
        "sdk_s": round(sum(e["sdk_ns"] for e in entries) / 1e9, 6),
        "delay_s": round(sum(e["delay_ns"] for e in entries) / 1e9, 6),
        "first_ns": first,
        "last_ns": last,
        "window_s": round(window, 6),
        "objects_per_s": round(objects / window, 1) if window > 0 else 0.0,
        "max_objects": max(e["max_objects"] for e in entries),
    }
