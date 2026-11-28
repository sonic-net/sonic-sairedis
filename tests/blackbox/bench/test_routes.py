"""Route programming: what orchagent does most, from BGP and config reloads.

The client creates routes to an ECMP group, moves them to a next hop (set),
and removes them, in bulks or one at a time, as orchagent's EntityBulker
does. syncd executes bulks with the SDK's bulk API (-l).
"""

import pytest

from harness.report import median_summary
from harness.scenarios import ROUTE, compact, ops_of, phase_summary, scaled

OPS = ("create", "set", "remove")


def run_routes(bench, record, options, scenario, count, bulk, family="v4"):
    bench.client.run("setup_l3 ports=4")
    summaries = {op: [] for op in OPS}
    rounds = {op: [] for op in OPS}
    for _ in range(options.rounds):
        for op in OPS:
            result = bench.measure(
                "routes " + op,
                "routes op={} count={} bulk={} family={} target=nhg".format(op, count, bulk, family),
                ops=ops_of(op, ROUTE))
            summary = phase_summary(result, bulk)
            assert summary["errors"] == 0, result
            summaries[op].append(summary)
            rounds[op].append(compact(result))
    for op in OPS:
        record("routes", "{} {}".format(scenario, op), median_summary(summaries[op]), rounds[op])


@pytest.mark.parametrize("bulk,family", [(1, "v4"), (100, "v4"), (1000, "v4"), (1000, "v6")])
def test_routes(bench, record, options, bulk, family):
    """Route create, set and remove, per bulk size.

    Steps, per round:
    1. Create `count` routes to a 4-way ECMP group, in bulks of `bulk`.
    2. Point them at a single next hop.
    3. Remove them.
    """
    count = scaled(2000 if bulk == 1 else 20000, options)
    bench.start()
    run_routes(bench, record, options, "bulk {} {}".format(bulk, family), count, bulk, family)


def test_routes_sdk_without_bulk(bench, record, options):
    """Bulks of 1000 routes on an SDK without bulk route API: syncd falls
    back to one SDK call per route.
    """
    bench.start(profile={"SAI_MOCK_BULK": "0"})
    run_routes(bench, record, options, "bulk 1000 v4, SDK without bulk", scaled(20000, options), 1000)


@pytest.mark.parametrize("per_route_us", [10, 50])
def test_routes_sdk_latency(bench, record, options, per_route_us):
    """Bulks of 1000 routes on an SDK that takes per_route_us to program a
    route: how the SDK's time adds to syncd's own.
    """
    bench.start()
    for op in OPS:
        bench.mock.delay("bulk_" + op, ROUTE, 0, per_route_us * 1000)
    run_routes(bench, record, options, "bulk 1000 v4, SDK {} us/route".format(per_route_us),
               scaled(20000, options), 1000)
