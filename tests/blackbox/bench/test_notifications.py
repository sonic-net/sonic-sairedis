"""Notifications: events the SDK raises, which syncd translates (SDK ids to
virtual ids), records in ASIC_DB where it must (learned FDB entries), and
forwards to orchagent. The mock raises them from its own thread, as an SDK's
event thread does, as fast as syncd's callback takes them."""

import pytest

from harness.report import median_summary
from harness.scenarios import compact, scaled


def run_events(bench, record, options, kind, ntf_type, object_type, count, batch, scenario):
    summaries = []
    rounds = []
    for round_index in range(options.rounds):
        bench.client.run("reset_notifications")
        # every round learns new MACs, as the first did
        extra = {"start": round_index * count} if kind == "fdb" else {}

        def storm():
            bench.mock.notify(kind, count, batch=batch, **extra)
            reply = bench.client.run("wait_notifications type={} count={} timeout_ms=600000".format(ntf_type, count),
                                     timeout=660)
            # the client can get the last event before the mock accounted for
            # its callback
            assert bench.mock.wait_notifiers(), "the mock's notifiers did not finish"
            return reply

        result = bench.measure(scenario, storm, ops=[("notify", object_type)], objects=count)
        reply = result["reply"]
        sdk = result["sdk"]
        deliver = (reply["last_ns"] - sdk["first_ns"]) / 1e9
        summaries.append({
            "events": count,
            "batch": batch,
            "send_s": sdk["window_s"],
            "deliver_s": round(deliver, 6),
            "e2e_per_s": round(count / deliver, 1) if deliver > 0 else None,
            "first_us": round((reply["first_ns"] - sdk["first_ns"]) / 1e3, 1),
            "syncd_us": result["cpu"]["syncd_us_per_object"],
            "redis_us": result["cpu"]["redis_us_per_object"],
            "client_us": result["cpu"]["client_us_per_object"],
            "received": reply["received"],
        })
        rounds.append(compact(result))
    record("notifications", scenario, median_summary(summaries), rounds)


@pytest.mark.parametrize("batch", [1, 100])
def test_fdb_events(bench, record, options, batch):
    """A burst of FDB learn events, `batch` per callback.

    Steps, per round:
    1. The mock reports `count` learned MACs on the default VLAN's bridge
       ports.
    2. Wait until the client received them all.
    """
    bench.start()
    run_events(bench, record, options, "fdb", "fdb", "SAI_OBJECT_TYPE_FDB_ENTRY",
               scaled(20000, options), batch, "fdb learned, batch {}".format(batch))


def test_port_state_events(bench, record, options):
    """A burst of port oper status changes, one port per callback.

    Steps, per round:
    1. The mock flips the oper status of `count` / 2 ports, each down and
       up. No change repeats within the burst: the client's notification
       queue merges a change with an earlier copy still pending (LruDedup),
       so fewer would arrive. The ports have few queues and priority
       groups, which these events do not touch, so the switch starts fast.
    2. Wait until the client received every change.
    """
    count = scaled(2000, options)
    bench.start(profile={"SAI_MOCK_PORT_COUNT": (count + 1) // 2, "SAI_MOCK_QUEUES_PER_PORT": 2,
                         "SAI_MOCK_PGS_PER_PORT": 1})
    run_events(bench, record, options, "port_state", "port_state", "SAI_OBJECT_TYPE_PORT",
               count, 1, "port state, batch 1")
