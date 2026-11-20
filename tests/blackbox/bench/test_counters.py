"""Counter polling: syncd's steady load. orchagent registers objects in
flex counter groups; syncd's flex counter threads read their stats from the
SDK every poll interval and write them to COUNTERS_DB."""

import pytest

from harness.report import median_summary
from harness.scenarios import compact

GROUPS = {
    "port": "PORT_STAT_COUNTER",
    "queue": "QUEUE_STAT_COUNTER",
    "pg": "PG_WATERMARK_STAT_COUNTER",
}

WINDOW_S = 10


@pytest.mark.parametrize("kind,bulk_stats", [("port", True), ("port", False), ("queue", True), ("pg", True)])
def test_flex_counters(bench, record, options, kind, bulk_stats):
    """Poll every counter of the 32 ports, or of their queues or priority
    groups, every second.

    Steps:
    1. Enable the group at a 1 s interval and add the objects with every
       stat of their type; the SDK has bulk stats or not.
    2. Wait 2 s, then measure `WINDOW_S` s windows of polling.
    """
    bench.start(profile={"SAI_MOCK_BULK_STATS": "1" if bulk_stats else "0"})
    group = GROUPS[kind]
    bench.client.run("flex_group name={} interval_ms=1000 op=enable".format(group))
    added = bench.client.run("flex_counters type={} op=add".format(kind))
    assert added["objects"] and added["counters"], "no {} counters registered: {}".format(kind, added)
    bench.measure_window("warm up", 2)
    summaries = []
    rounds = []
    for _ in range(options.rounds):
        result = bench.measure_window("{} counters".format(kind), WINDOW_S,
                                      ops=[("get_stats", "*"), ("get_stats_ext", "*"), ("bulk_get_stats", "*")])
        sdk = result["sdk"]
        reads = sdk["objects"]
        # a stalled or failing poll would show as less CPU, not as an error
        assert reads, "no {} counters read in {} s".format(kind, WINDOW_S)
        assert not sdk["errors"], "{} of {} stats reads failed".format(sdk["errors"], kind)
        summaries.append({
            "objects": added["objects"],
            "counters": added["counters"],
            "interval_ms": 1000,
            "object_reads_per_s": round(reads / result["wall_s"], 1),
            "sdk_calls_per_s": round(sdk["calls"] / result["wall_s"], 1),
            "syncd_percent": result["cpu"]["syncd_percent"],
            "redis_percent": result["cpu"]["redis_percent"],
            "syncd_us_per_read": round(result["cpu"]["syncd_s"] / reads * 1e6, 2),
            "redis_cmds_per_s": round(result["redis_commands_total"] / result["wall_s"], 1),
        })
        rounds.append(compact(result))
    scenario = "{} x{} counters, {} stats".format(kind, added["counters"], "bulk" if bulk_stats else "per object")
    record("counters", scenario, median_summary(summaries), rounds)
