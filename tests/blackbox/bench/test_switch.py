"""Switch creation: syncd creates the switch and discovers what the SDK
created with it (ports, queues, scheduler groups, priority groups, bridge
ports, VLAN members), then saves it all to ASIC_DB."""

import pytest

from harness.report import median_summary
from harness.scenarios import compact


@pytest.mark.parametrize("ports", [32, 128])
def test_switch_init(bench, record, options, ports):
    """create_switch time as the client sees it, against the switch's size.

    Steps, per round:
    1. Start syncd on a mock switch of `ports` ports (20 queues, 11 scheduler
       groups, 8 priority groups, a bridge port and a VLAN member each).
    2. Create the switch from the client (INIT_VIEW, create, APPLY_VIEW), and
       measure it.
    """
    summaries = []
    rounds = []
    for _ in range(options.rounds):
        bench.start(profile={"SAI_MOCK_PORT_COUNT": ports}, init=False)
        result = bench.measure("init", "init comm={}".format(options.comm), timeout=600)
        objects = sum(bench.mock.objects().values())
        reply = result["reply"]
        summaries.append({
            "ports": ports,
            "objects": objects,
            # in redis_async mode create_switch returns at once, and apply view waits
            "init_s": round(reply["create_switch_s"] + reply["apply_view_s"], 6),
            "create_switch_s": reply["create_switch_s"],
            "apply_view_s": reply["apply_view_s"],
            "sdk_calls": result["sdk_all"]["calls"],
            "syncd_cpu_s": result["cpu"]["syncd_s"],
            "redis_cpu_s": result["cpu"]["redis_s"],
            "syncd_rss_mb": result["syncd_memory"]["rss_mb"],
        })
        rounds.append(compact(result))
        errors = bench.stop()
        assert not errors, errors
    record("switch init", "{} ports".format(ports), median_summary(summaries), rounds)
