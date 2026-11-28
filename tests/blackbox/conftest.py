"""pytest wiring of the syncd black-box benchmark: options, fixtures, report.

The benchmark takes over the machine it runs on (redis at the SONiC paths,
/dev/log), so it runs only where SYNCD_BB_ENABLE=1 says that is fine, as root
in a throwaway container; run.sh sets that up.
"""

import os
import types

import pytest

from harness.bench import Bench
from harness.env import BenchEnv, check_prerequisites
from harness.profile import check_perf
from harness.report import Report, run_meta

HERE = os.path.dirname(os.path.abspath(__file__))
TOP = os.path.abspath(os.path.join(HERE, "..", ".."))

IN_TREE_LIBS = [os.path.join(TOP, d, ".libs") for d in ("meta", "lib", "vslib")]


def pytest_addoption(parser):
    group = parser.getgroup("syncd black-box benchmark")
    group.addoption("--bench-syncd", default=os.path.join(TOP, "syncd", ".libs", "syncd"),
                    help="syncd binary under test (default: the one built in this tree)")
    group.addoption("--bench-syncd-args", default="-u -l",
                    help="syncd arguments; -p is added, and -z with the communication mode "
                         "unless -s or -z is given (default: temp view, SAI bulk API)")
    group.addoption("--bench-mock-lib", default=os.path.join(HERE, "build", "libsaimock.so"),
                    help="mock SAI library, preloaded into syncd")
    group.addoption("--bench-saibench", default=os.path.join(HERE, "build", "saibench"),
                    help="load client")
    group.addoption("--bench-ld-library-path", default=None,
                    help="LD_LIBRARY_PATH of syncd and the client (default: the tree's .libs, if built; "
                         "empty: the system's libraries, whatever the caller's LD_LIBRARY_PATH)")
    group.addoption("--bench-comm", default="redis_sync", choices=("redis_sync", "redis_async", "zmq_sync"),
                    help="communication mode between the client and syncd")
    group.addoption("--bench-scale", type=float, default=1.0, help="multiplies object counts")
    group.addoption("--bench-rounds", type=int, default=3, help="rounds of each measured phase")
    group.addoption("--bench-cpus", default="",
                    help="CPUs to pin processes to, as syncd=2,redis=3,client=4")
    group.addoption("--bench-label", default="", help="report name (default: the communication mode)")
    group.addoption("--bench-report-dir", default=os.path.join(HERE, "report"), help="where reports go")
    group.addoption("--bench-workdir", default="/tmp/syncd-bench", help="scratch directory")
    group.addoption("--bench-perf", default="",
                    help="profile these processes with perf during each measured phase: syncd, redis, client, "
                         "comma separated (default: none); needs perf, CAP_SYS_ADMIN and CAP_IPC_LOCK")
    group.addoption("--bench-perf-freq", type=int, default=999, help="perf samples per second")
    group.addoption("--bench-perf-event", default="cpu-clock", choices=("cpu-clock", "task-clock"),
                    help="perf event: a clock, as the profiles weigh samples in nanoseconds of CPU")
    group.addoption("--bench-perf-callgraph", default="dwarf,32768",
                    help="perf call graph mode (32 KB of stack: hiredis reads into a 16 KB stack buffer)")


def pytest_collection_modifyitems(config, items):
    reason = None
    if os.environ.get("SYNCD_BB_ENABLE") != "1":
        reason = "SYNCD_BB_ENABLE=1 is not set: the benchmark takes over /var/run/redis and /dev/log"
    else:
        reason = check_prerequisites()
    if reason:
        for item in items:
            # pytest may collect other tests with the benchmarks
            if os.path.realpath(str(item.fspath)).startswith(os.path.realpath(HERE) + os.sep):
                item.add_marker(pytest.mark.skip(reason=reason))


def _abspath(path):
    # syncd and the client run in the benchmark's work directory
    return os.path.abspath(path) if path else path


def _options(config):
    cpus = {}
    for part in filter(None, config.getoption("--bench-cpus").split(",")):
        name, _, value = part.partition("=")
        cpus[name.strip()] = value.strip()
    ld_path = config.getoption("--bench-ld-library-path")
    if ld_path is None:
        ld_path = ":".join(d for d in IN_TREE_LIBS if os.path.isdir(d))
    ld_path = ":".join(_abspath(d) for d in ld_path.split(":") if d)
    return types.SimpleNamespace(
        syncd=_abspath(config.getoption("--bench-syncd")),
        syncd_args=config.getoption("--bench-syncd-args").split(),
        mock_lib=_abspath(config.getoption("--bench-mock-lib")),
        saibench=_abspath(config.getoption("--bench-saibench")),
        ld_library_path=ld_path,
        comm=config.getoption("--bench-comm"),
        scale=config.getoption("--bench-scale"),
        rounds=max(1, config.getoption("--bench-rounds")),
        cpus=cpus,
        workdir=_abspath(config.getoption("--bench-workdir")),
        report_dir=_abspath(config.getoption("--bench-report-dir")),
        label=config.getoption("--bench-label") or config.getoption("--bench-comm"),
        perf=[p.strip() for p in config.getoption("--bench-perf").split(",") if p.strip()],
        perf_freq=config.getoption("--bench-perf-freq"),
        perf_event=config.getoption("--bench-perf-event"),
        perf_callgraph=config.getoption("--bench-perf-callgraph"),
    )


@pytest.fixture(scope="session")
def options(request):
    opts = _options(request.config)
    for path in (opts.syncd, opts.mock_lib, opts.saibench):
        if not os.path.exists(path):
            pytest.exit("{} does not exist: build the tree, then make -C tests/blackbox".format(path), 2)
    unknown = set(opts.perf) - {"syncd", "redis", "client"}
    if unknown:
        pytest.exit("--bench-perf: unknown processes {}".format(", ".join(sorted(unknown))), 2)
    if opts.perf:
        reason = check_perf()
        if reason:
            pytest.exit("--bench-perf: {}".format(reason), 2)
    return opts


@pytest.fixture(scope="session")
def bench_env(options):
    env = BenchEnv(options.workdir, redis_cpus=options.cpus.get("redis"))
    env.start()
    yield env
    env.stop()


@pytest.fixture(scope="session")
def report(options):
    rep = Report(options.label, run_meta(options, TOP))
    yield rep
    base = rep.write(options.report_dir)
    if base:
        print("\nbenchmark report: {}.md, {}.json".format(base, base))


@pytest.fixture
def bench(request, bench_env, options):
    """A fresh syncd on the mock SAI and a load client; the test starts them
    with the switch shape it needs. Fails the test if syncd logged errors."""
    b = Bench(bench_env, options, request.node.name)
    yield b
    errors = b.stop()
    if errors:
        pytest.fail("syncd logged errors:\n  " + "\n  ".join(errors[:20]))


@pytest.fixture
def record(report):
    """record(kind, scenario, summary, rounds): adds a result to the report."""
    return report.add
