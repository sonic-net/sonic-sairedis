"""Summarizes Bench.measure results for the benchmarks' reports."""

ROUTE = "SAI_OBJECT_TYPE_ROUTE_ENTRY"


def scaled(count, options):
    return max(1, int(count * options.scale))


def ops_of(op, object_type):
    """The SAI calls an operation of the client can become: syncd executes a
    bulk request with the SDK's bulk API, or one object at a time when the
    SDK has none."""
    return [(op, object_type), ("bulk_" + op, object_type)]


def phase_summary(result, bulk=None):
    client = result["client"]
    sdk = result["sdk"]
    cpu = result["cpu"]
    latency = client.get("latency", {})
    return {
        "objects": result["objects"],
        "bulk": bulk,
        "e2e_per_s": client.get("objects_per_s"),
        "sdk_per_s": sdk["objects_per_s"],
        "p50_us": latency.get("p50_us"),
        "p99_us": latency.get("p99_us"),
        "syncd_us": cpu["syncd_us_per_object"],
        "redis_us": cpu["redis_us_per_object"],
        "client_us": cpu["client_us_per_object"],
        "syncd_busy": cpu["syncd_main_busy"],
        "sdk_calls": sdk["calls"],
        "errors": client.get("errors", 0) + sdk["errors"],
    }


def compact(result):
    """What a round keeps in the JSON report, besides its summary."""
    return {k: result[k] for k in ("name", "objects", "wall_s", "client", "sdk", "sdk_all", "cpu",
                                   "syncd_threads", "redis_commands_total", "redis_commands", "syncd_memory",
                                   "profile") if k in result}
