"""Other objects orchagent creates in numbers: neighbors and FDB entries
(non-object-id entries, like routes), and next hops, objects with an id: for
those syncd allocates a virtual id and maps it to the SDK's in ASIC_DB."""

import pytest

from harness.report import median_summary
from harness.scenarios import compact, ops_of, phase_summary, scaled

KINDS = {
    "neighbors": "SAI_OBJECT_TYPE_NEIGHBOR_ENTRY",
    "nexthops": "SAI_OBJECT_TYPE_NEXT_HOP",
    "fdb": "SAI_OBJECT_TYPE_FDB_ENTRY",
}


@pytest.mark.parametrize("kind,bulk", [
    ("neighbors", 1), ("neighbors", 100),
    ("nexthops", 1), ("nexthops", 100),
    ("fdb", 1), ("fdb", 100),
])
def test_objects(bench, record, options, kind, bulk):
    """Create and remove `count` objects of a kind, in bulks of `bulk`.

    Steps, per round:
    1. Create the objects (neighbors and next hops on 4 router interfaces,
       static FDB entries on the default VLAN's bridge ports).
    2. Remove them.
    """
    count = scaled(2000, options)
    bench.start()
    bench.client.run("setup_l3 ports=4")
    object_type = KINDS[kind]
    summaries = {"create": [], "remove": []}
    rounds = {"create": [], "remove": []}
    for _ in range(options.rounds):
        for op in ("create", "remove"):
            result = bench.measure("{} {}".format(kind, op),
                                   "{} op={} count={} bulk={}".format(kind, op, count, bulk),
                                   ops=ops_of(op, object_type))
            summary = phase_summary(result, bulk)
            assert summary["errors"] == 0, result
            summaries[op].append(summary)
            rounds[op].append(compact(result))
    for op in ("create", "remove"):
        record("objects", "{} bulk {} {}".format(kind, bulk, op), median_summary(summaries[op]), rounds[op])
