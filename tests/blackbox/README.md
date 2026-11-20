# syncd black-box benchmark

Measures syncd's own performance, independent of any vendor SDK: how fast it
programs routes and other objects, creates a switch, forwards notifications and
polls counters, and what each costs in CPU of syncd, redis and the client.

syncd runs unmodified, as its own process, against a real redis. In place of the
vendor's SAI library it gets a mock SDK, `libsaimock.so`, that implements every
SAI API at almost no cost and counts every call syncd makes. A load client,
`saibench`, stands in for orchagent: it drives syncd through libsairedis, as
orchagent does. The benchmarks are pytest tests that run them and measure each
phase from all sides at once.

```text
 saibench (orchagent stand-in)       syncd (unmodified)               libsaimock.so (LD_PRELOAD)
 commands on stdin, JSON replies     ------------------               --------------------------
 libsairedis client  --redis/ZMQ-->  ASIC_DB, VID/RID translation --> every sai_* API: object store,
   callbacks         <--notify----   notification thread          <-- switch model, call statistics,
                                     flex counter threads         --> event threads, control socket
         ^                                    ^                                ^
         |                                    |                                |
 pytest harness: redis-server, /dev/log sink, /proc CPU of each process, redis INFO, mock statistics
```

## Running

`run.sh` builds and runs everything in containers of the sairedis build
environment (`build-env/`, image `build-env-build:latest`, built when missing):

```sh
tests/blackbox/run.sh                                  # every benchmark, about 4 minutes
tests/blackbox/run.sh bench/test_routes.py -k 1000     # some of them
tests/blackbox/run.sh --bench-rounds 1 --bench-scale 0.25           # a quick pass
tests/blackbox/run.sh --bench-comm zmq_sync --bench-label zmq       # orchagent to syncd over ZMQ
```

The first container builds as the caller: the tree, if syncd is not built yet
(`./configure --with-sai=vs`, optimized, without coverage), then the mock and the
client (`make -C tests/blackbox`). A tree built with `--enable-code-coverage` or
`--enable-asan` benchmarks an instrumented syncd: `run.sh` warns about it. The
second container runs the benchmarks as root, since they put redis and `/dev/log`
at their SONiC paths. Arguments go to pytest. Reports go to
`tests/blackbox/report`, logs of each benchmark (syslog, syncd output, the mock's
profile) to `tests/blackbox/work`.

To run the benchmarks without `run.sh`, in a throwaway container or machine with
redis-server, python3-redis and pytest, as root:

```sh
make -C tests/blackbox
cd tests/blackbox && SYNCD_BB_ENABLE=1 python3 -m pytest
```

Without `SYNCD_BB_ENABLE=1` every benchmark is skipped. Give pytest the options
below as `--bench-x=value`: before it loads `conftest.py`, which defines them,
pytest takes a separate value that is a path outside this directory, as in
`--bench-syncd /usr/bin/syncd`, for a test path, and then misses `conftest.py`
(`run.sh` passes them so).

| Option | Meaning |
| --- | --- |
| `--bench-syncd` | syncd binary (default: `syncd/.libs/syncd` of this tree) |
| `--bench-syncd-args` | syncd arguments (default `-u -l`: temp view, SAI bulk API); `-p` is added, and `-z <mode>` unless `-s` or `-z` is given |
| `--bench-comm` | `redis_sync` (default), `redis_async` or `zmq_sync`, between the client and syncd |
| `--bench-mock-lib`, `--bench-saibench` | the mock and the client (default: `build/` here) |
| `--bench-ld-library-path` | `LD_LIBRARY_PATH` of syncd and the client (default: the tree's `.libs`; empty: the system's libraries, whatever the caller's `LD_LIBRARY_PATH`) |
| `--bench-scale` | multiplies the number of objects of every benchmark (default 1) |
| `--bench-rounds` | rounds of each measured phase; the report has their medians (default 3) |
| `--bench-cpus` | CPUs to pin processes to, as `syncd=2,redis=3,client=4` |
| `--bench-label` | name of the report (default: the communication mode) |
| `--bench-report-dir`, `--bench-workdir` | where reports and logs go |
| `--bench-perf` | processes to profile during each measured phase, among `syncd`, `redis` and `client`, comma separated (default: none) |
| `--bench-perf-freq`, `--bench-perf-event`, `--bench-perf-callgraph` | sampling frequency (default 999 Hz), event (`cpu-clock`, the default, or `task-clock`) and call graph (default `dwarf,32768`) of the profiles |
| `SYNCD_BB_IMAGE` | image `run.sh` uses (default `build-env-build:latest`) |
| `SYNCD_BB_PERF_IMAGE` | image `run.sh` profiles in (default `syncd-bench-perf:latest`, built from `SYNCD_BB_IMAGE` with perf when missing) |

### Another syncd

The mock replaces the SAI library by symbol interposition: `LD_PRELOAD` puts its
`sai_*` functions ahead of those of the library syncd links, `libsaivs` or a
vendor's `libsai`, whose code then never runs. So any syncd binary runs on it, as
long as the mock is built from the same SAI headers, and with a sairedis close
enough that its libsaimeta and libsaimetadata, which the mock uses, have the same
interface. For instance, the syncd of a `docker-sonic-vs` image, with the mock and
the client built here:

```sh
make -C tests/blackbox          # in the build environment, as run.sh does
docker run --rm --entrypoint bash -v "$PWD:/workspace" -w /workspace/tests/blackbox docker-sonic-vs:latest -c '
    pip3 install -q --break-system-packages pytest
    SYNCD_BB_ENABLE=1 python3 -m pytest --bench-syncd=/usr/bin/syncd --bench-ld-library-path= --bench-label=image
    chown -R '"$(id -u):$(id -g)"' report'
```

This ran on an image of sonic-buildimage master.1272499 (sairedis 80371965,
with the SAI headers of this tree).

### Comparing two builds

Run the benchmark on each, with a label, and compare the reports:

```sh
tests/blackbox/run.sh --bench-label before
# change syncd, make
tests/blackbox/run.sh --bench-label after
tests/blackbox/compare.py tests/blackbox/report/bench-before.json tests/blackbox/report/bench-after.json
```

Compare runs on the same host only, and with the same options. Several runs show
how much a host's numbers vary; `--bench-cpus` makes them steadier.

### Profiles

With `--bench-perf`, each measured phase also records a CPU profile of the given
processes, all their threads, with `perf record`. A profile keeps the samples
taken between the phase's CPU snapshots, by their monotonic timestamps: it
covers what the phase's numbers measure.
`run.sh` then runs the benchmarks in an image with perf and FlameGraph, with
`CAP_SYS_ADMIN`, which perf needs in a container, and `CAP_IPC_LOCK`, so its
buffers do not count against the locked memory of the host's root. Per phase
and process, the benchmark's directory in `tests/blackbox/work` gets the folded stacks,
`NN-<phase>.<process>.folded` (root first, weighted in nanoseconds of CPU, as
flamegraph.pl and speedscope read them), and their flame graph, `.svg`; the JSON
report has the top functions of each profile, by self and inclusive CPU. To dig
into one:

```sh
tests/blackbox/run.sh --bench-perf syncd --bench-rounds 1 bench/test_routes.py -k 1000-v4
python3 tests/blackbox/harness/profile.py tests/blackbox/work/test_routes_1000-v4_/01-routes_create.syncd.folded \
    --top 30 --match RedisClient --match sai_deserialize
```

`--match` prints the share of CPU of the stacks with a frame matching a regular
expression. Profiling slows down what it samples: take numbers from runs without
it. The DWARF call graph copies 32 KB of stack per sample, which unwinding through
hiredis's 16 KB read buffer needs; kernel frames are `[unknown]` unless the host
lets perf resolve kernel symbols (`kernel.kptr_restrict`).

## Reports

`bench-<label>.md` has a table per kind of benchmark, `bench-<label>.json` the
same values and every round's raw measurements: per phase, what the client saw
(wall time, per call latency percentiles), the SDK calls the mock received, CPU
seconds of syncd, redis and the client, the busiest syncd threads, redis commands
and syncd's memory.

| Column | Meaning |
| --- | --- |
| `e2e_per_s` | objects per second as the client sees them, from request to reply |
| `sdk_per_s` | objects per second syncd drove into the SDK, from its first call to the return of its last |
| `p50_us`, `p99_us` | latency of one client call: one object, or one bulk |
| `syncd_us`, `redis_us`, `client_us` | CPU microseconds per object of syncd, redis and the client |
| `syncd_busy` | CPU time of syncd's main thread over the phase's wall time: near 1, syncd is the bottleneck |
| `sdk_calls` | SAI calls the mock received for the phase's objects |

In the synchronous modes the client waits for each reply, so a phase's rate is
bound by the round trip through the client, redis and syncd, which `syncd_busy`
below 1 shows; `syncd_us` is syncd's cost whatever the mode. Times are
`CLOCK_MONOTONIC`, which the harness, the client and the mock share. CPU times are
the processes' CPU clocks, in nanoseconds, which count threads that exited during
a phase too; the busiest syncd threads come from their `schedstat`.

For instance, routes in `redis_sync` mode on a 16 vCPU Xeon Platinum 8473C VM
(medians of 3 rounds, this tree):

| scenario | objects | bulk | e2e_per_s | sdk_per_s | p50_us | syncd_us | redis_us | client_us | syncd_busy |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| bulk 1 v4 create | 2,000 | 1 | 3,591 | 3,596 | 277.3 | 90.5 | 134.8 | 75.5 | 0.318 |
| bulk 1000 v4 create | 20,000 | 1,000 | 68,882 | 72,713 | 13,515 | 6.5 | 5.45 | 7.04 | 0.502 |
| bulk 1000 v4 remove | 20,000 | 1,000 | 120,099 | 126,448 | 8,348 | 3.66 | 2.94 | 4.05 | 0.416 |
| bulk 1000 v4, SDK without bulk create | 20,000 | 1,000 | 69,518 | 74,168 | 14,163 | 7.9 | 4.93 | 6.64 | 0.502 |
| bulk 1000 v4, SDK 50 us/route create | 20,000 | 1,000 | 15,753 | 15,939 | 62,691 | 56.6 | 5.31 | 6.76 | 0.889 |

## The benchmarks

| Module | Benchmark | What it shows |
| --- | --- | --- |
| `test_switch.py` | `create_switch` with 32 and 128 ports | discovery of what the SDK created with the switch (20 queues, 11 scheduler groups, 8 priority groups, a bridge port and a VLAN member per port), and its save to ASIC_DB |
| `test_routes.py` | create, set and remove 20,000 routes in bulks of 100 and 1000, IPv4 and IPv6, and 2,000 one at a time | route programming, orchagent's main load |
| | the same on an SDK without bulk route API | syncd's fallback to one SDK call per route |
| | the same on an SDK that takes 10 or 50 µs per route | how the SDK's time adds to syncd's own |
| `test_objects.py` | create and remove 2,000 neighbors, next hops and FDB entries, alone and in bulks of 100 | non-object-id entries, and objects with an id, for which syncd allocates a virtual id and maps it in ASIC_DB |
| `test_notifications.py` | 20,000 FDB learn events of new MACs, alone and 100 per callback, and 2,000 port state changes, down and up of 1,000 ports | translation of SDK ids to virtual ids, ASIC_DB updates and forwarding to the client |
| `test_counters.py` | polling every counter of 32 ports, their 640 queues or 256 priority groups, every second, on an SDK with and without bulk stats | syncd's steady load: SDK reads, COUNTERS_DB writes |

## How it works

### The mock SDK: saimock/

`stub.pl`, which generates libsaivs's and libsairedis's API tables, generates the
mock's too (`build/sai_stub.cpp`): every function of every SAI API, and the
global ones, calls a method of `saimock::MockSai`, a `sairedis::SaiInterface`
(`MockSai.cpp`). The Makefile also forwards the bulk stats globals, which
stub.pl leaves unimplemented.

- **Objects.** Object ids encode their type (bits 63-48) and switch (bits 47-40),
  so `sai_object_type_query` and `sai_switch_id_query` decode them. Entries
  (routes, neighbors, FDB entries) are kept by their key. Attribute values are
  kept: primitive values in place, lists in a deep copy (`SaiAttrWrapper`);
  `SAI_MOCK_STORE_ATTRIBUTES=0` keeps only the objects. Creating an existing entry,
  or removing a missing one, fails as on an SDK.
- **Gets** return the kept value, else the attribute's default from the SAI
  metadata, else zero: a null object id, an empty list.
- **The switch** (`MockSwitch.cpp`) comes with what syncd discovers on a real
  one: a CPU port and front panel ports with lanes, queues, scheduler groups and
  priority groups, the default virtual router, VLAN, 1Q bridge with bridge ports,
  STP instance, trap group and hashes. A port the client creates gets its queues
  and groups too.
- **Bulk** calls go one object after the other and honor the error mode.
  `SAI_MOCK_BULK=0` answers them not supported, as an SDK without bulk API does.
- **Stats** return values that grow with every read. `queryStatsCapability`
  reports every stat of the type, with the bulk modes when bulk stats are on:
  syncd polls counters in bulk only for those (`SAI_MOCK_BULK_STATS=0`: none).
- **Statistics.** Every call is counted per operation and object type: calls,
  objects, errors, calls answered not supported, time inside the mock, injected
  delay, the time of the first call and of the return of the last, the largest
  bulk. syncd makes its SAI calls one at a time; the mock holds a mutex anyway,
  for its control socket and event threads.
- **Delays** emulate an SDK's time: per call and per object, for an operation
  and object type, spinning (the time counts as syncd's CPU, like an SDK busy
  with its hardware) or sleeping.
- **Notifications** come from a thread of the mock, as from an SDK's event
  thread: FDB events on the default VLAN's bridge ports (the mock learns or ages
  the entries it reports), port oper status changes (the port's status flips).

The mock reads its configuration from syncd's profile (`-p`), and from the
environment, which wins:

| Key | Default | Meaning |
| --- | --- | --- |
| `SAI_MOCK_CONTROL_SOCKET` | `/var/run/saimock.sock` | the control socket |
| `SAI_MOCK_PORT_COUNT` | 32 | front panel ports |
| `SAI_MOCK_LANES_PER_PORT` | 4 | lanes per port |
| `SAI_MOCK_PORT_SPEED` | 100000 | port speed |
| `SAI_MOCK_QUEUES_PER_PORT` | 20 | queues per port, half unicast, half multicast |
| `SAI_MOCK_CPU_QUEUES` | 32 | queues of the CPU port |
| `SAI_MOCK_PGS_PER_PORT` | 8 | ingress priority groups per port |
| `SAI_MOCK_DEFAULT_BRIDGE_PORTS` | 1 | a bridge port per port in the default 1Q bridge |
| `SAI_MOCK_DEFAULT_VLAN_MEMBERS` | 1 | each bridge port a member of VLAN 1 |
| `SAI_MOCK_PORTS_UP` | 1 | ports start up |
| `SAI_MOCK_STORE_ATTRIBUTES` | 1 | keep the attributes of objects the client creates |
| `SAI_MOCK_BULK` | 1 | bulk create, remove, set and get |
| `SAI_MOCK_BULK_STATS` | 1 | bulk stats |
| `SAI_MOCK_SRC_MAC` | `00:11:22:33:44:55` | the switch's MAC |
| `SAI_MOCK_DELAYS` | | delays from the start, `op:type:per_call_ns[:per_object_ns[:sleep]]` separated by `;` |

The control socket (`MockControl.cpp`) takes one command per line and answers
each with one line of JSON:

| Command | Answer |
| --- | --- |
| `ping` | the process id |
| `stats`, `stats reset` | the statistics, and the live objects per type; `reset` clears the statistics after reading them |
| `reset` | clears the statistics |
| `objects` | live objects per type |
| `config` | the configuration and delays |
| `delay <op> <type> <per call ns> [per object ns] [sleep\|spin]`, `delay clear` | sets or clears a delay; `*` for any op or type, as `delay bulk_create ROUTE_ENTRY 0 10000` |
| `notify fdb count=N [batch=B] [rate=R] [event=learned\|aged\|move] [start=S]` | raises N FDB events for MACs number S onwards (default 0), B per callback, R per second (default: as fast as syncd takes them) |
| `notify port_state count=N [batch=B] [rate=R]` | flips the ports' oper status N times |
| `notify status` | how many notification threads still run |

Ops are `create`, `remove`, `set`, `get`, `bulk_create`, `bulk_remove`,
`bulk_set`, `bulk_get`, `get_stats`, `get_stats_ext`, `clear_stats`,
`bulk_get_stats`, `bulk_clear_stats`, `object_type_query`, `switch_id_query`,
`query` (capability queries, API version, log levels), `flush_fdb` and `notify`.

### The load client: saibench/

`saibench` initializes libsairedis as orchagent does (communication mode,
recording off, pipeline in asynchronous mode, INIT_VIEW, `create_switch` with
the notification callbacks, APPLY_VIEW), then executes commands from stdin, one
JSON reply per line:

| Command | What it does |
| --- | --- |
| `init [comm=redis_sync\|redis_async\|zmq_sync] [temp_view=1] [apply_view=1] [record=0]` | creates the switch; reads its ports, default virtual router, VLAN and bridge ports |
| `setup_l3 [ports=4]` | a router interface, a neighbor and a next hop on each of the first ports, and an ECMP group of those next hops |
| `routes op=create\|set\|remove count=N [bulk=B] [family=v4\|v6] [target=nhg\|nh\|drop] [start=0]` | routes 100.0.0.0/24 or fc00::/64 onwards; set moves them to the other kind of target |
| `neighbors op=create\|remove count=N [bulk=B]` | neighbors on the router interfaces |
| `nexthops op=create\|remove count=N [bulk=B]` | next hops on the router interfaces |
| `fdb op=create\|remove count=N [bulk=B]` | static FDB entries on the default VLAN's bridge ports |
| `flex_group name=G [interval_ms=1000] [op=enable] [stats_mode=STATS_MODE_READ]` | sets a flex counter group |
| `flex_counters type=port\|queue\|pg [op=add\|remove] [ports=N] [counters=K]` | registers the ports, or their queues or priority groups, with the first K stats of their type (default all) |
| `wait_notifications type=fdb\|port_state count=N [timeout_ms=60000]` | waits until the callbacks received N events |
| `reset_notifications` | clears the received events |

A phase's reply has its objects, errors, wall time, objects per second, the
latency percentiles of its calls, and its start and end times. A phase ends with
a get: syncd executes requests in order, and answers the get once it executed
everything before it. In the asynchronous mode the calls did not wait for syncd;
in the synchronous modes syncd answers a call before it records it in ASIC_DB.
In the redis modes, syncd has recorded it when it answers the get. In zmq_sync,
a thread of syncd records it afterwards, so the harness waits until redis has
been idle for 60 ms, and ends the phase when it went idle.

### The harness: harness/, conftest.py

- `env.py`: redis-server at `/var/run/redis/redis.sock` and port 6379, the SONiC
  `database_config.json` (the image's, or one it writes), and a syslog sink at
  `/dev/log`: syncd logs through syslog, as on a switch, and the sink keeps what
  it logged.
- `syncd.py`: starts syncd with the mock preloaded and its profile, and waits
  until the mock's control socket answers and syncd logged its main loop start.
- `client.py`, `mock.py`: saibench as a subprocess, and the control socket.
- `bench.py`: one benchmark's device. `measure()` runs a client command, or a
  function, and returns what the client, the mock, `/proc` and redis saw during
  it. Each benchmark fails if syncd logged an error or died.
- `report.py`, `scenarios.py`: the reports, medians over rounds, and summaries.

## Writing a benchmark

```python
from harness.report import median_summary
from harness.scenarios import compact, ops_of, phase_summary


def test_next_hop_churn(bench, record, options):
    """Next hops created and removed in bulks of 500."""
    bench.start(profile={"SAI_MOCK_PORT_COUNT": 64})
    bench.client.run("setup_l3 ports=8")
    summaries, rounds = [], []
    for _ in range(options.rounds):
        result = bench.measure("nexthops create", "nexthops op=create count=5000 bulk=500",
                               ops=ops_of("create", "SAI_OBJECT_TYPE_NEXT_HOP"))
        summaries.append(phase_summary(result, 500))
        rounds.append(compact(result))
        bench.client.run("nexthops op=remove count=5000 bulk=500")
    record("objects", "next hop churn, bulk 500", median_summary(summaries), rounds)
```

`bench.start()` takes the mock's profile, syncd's arguments and the
communication mode; `bench.mock` sets delays and raises notifications while a
benchmark runs.

## Limits

- The mock is no switch: it forwards nothing, checks little beyond what an SDK
  must (existing objects and entries), and answers gets of attributes nobody set
  with defaults. It is there to cost nothing, so that what is measured is syncd.
- The numbers are those of syncd, redis and libsairedis on the host the
  benchmark runs on, for comparing builds and options, not of a switch.
- syncd runs without the vendor's processes and kernel interfaces (no host
  interfaces, no SDK threads), so what they cost on a switch is not measured.
