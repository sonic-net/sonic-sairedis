# docker-sai-test-vpp — VPP SAI Unit-Test Framework

A self-contained, single-container framework for validating the **SAI API
implementation of the VPP virtual-switch backend** (`libsaivs.so`) by running the
OpenComputeProject (OCP) `sai_test` PTF suite against a real VPP dataplane.

---

## a) Purpose and design

### What it does
The framework exercises SAI operations (create/set/get/remove of ports, RIFs,
routes, neighbors, VLANs, FDB, LAGs, ECMP, …) through a Thrift RPC interface and
verifies the resulting **data-plane** behavior by injecting and capturing packets
on virtual interfaces. It is the test vehicle for finding gaps between the SAI
contract and the VPP backend, and for producing a per-test **compatibility
matrix**.

### Architecture (one privileged container)
```
┌──────────────────────────────────────────────────────────────────────┐
│ docker-sai-test-vpp  (--privileged)                                    │
│                                                                        │
│  ┌───────────┐     ┌──────────────┐      ┌────────────────────────┐    │
│  │ VPP       │     │ saiserver    │      │ PTF test runner         │    │
│  │ af_packet │◄───►│ libsaivs.so  │◄────►│ sai_test/*.py           │    │
│  │ linux_cp  │ VPP │ (VPP SAI)    │Thrift│ sai_thrift adapter      │    │
│  └─────┬─────┘ API └──────────────┘ :9092└───────────┬────────────┘    │
│        │ AF_PACKET                          raw socket │ (AF_PACKET)    │
│   OEthernet0 ◄════ veth ════► OEth0_peer ──────────────┘                │
│   …            (32 pairs)                                               │
└──────────────────────────────────────────────────────────────────────┘
  EXISTING: VPP, libsaivs.so, sai_test, sai_thrift, PTF, af_packet/linux_cp
  THIS HARNESS: Dockerfile, run_test.sh, sai.profile, *-map.ini
```

- **VPP** is the dataplane; `OEthernetX` are the VPP-facing kernel veth ends and
  `OEthX_peer` are the PTF-facing ends. `OEthernetX` represents an out-facing
  (wire) interface; the inside `EthernetX` (linux_cp TAP) faces the SONiC control
  plane.
- **saiserver** is a thin Thrift→SAI shim (port 9092) linked against `libsaivs.so`.
- **PTF** runs the OCP `sai_test` Python suite and does packet I/O on the
  `OEthX_peer` ends.
- **`run_test.sh`** is the container entrypoint and orchestrator: Redis →
  veth/PortChannel topology → VPP → saiserver → PTF.

### Key design point — config-signature grouping
The VPP SAI backend can build the switch + host-interfaces **once per saiserver
process**. The OCP framework is built to configure a common T0 setup once and have
subsequent tests reuse it (`common_configured=true`). But different test classes
ask `T0TestBase.setUp` for *different* common configs (e.g. ECMP tests need
next-hop groups). So `run_test.sh`:

1. parses each requested test class's `setUp` (resolving config kwargs through the
   class **inheritance chain**) to compute its common-config "signature";
2. groups tests by signature;
3. for each group: **restarts the backend** (fresh saiserver), the first test
   builds + persists that group's config, the rest reuse it.

This lets one container run any mix of tests correctly in a single invocation.

### Supported tests

The table below lists OCP `sai_test` classes that **pass** on the current VPP SAI
backend (last validated **2026-06-19** against
`sai_route_test sai_rif_test sai_neighbor_test sai_ecmp_test`). It is the
published substitute for a full compatibility matrix: only passing tests are
listed. After a local matrix run, update this section when the pass set changes
(see **Collecting results** below).

| Module | Passing test classes |
|---|---|
| `sai_ecmp_test` | `EcmpLagDisableTestV4`, `EcmpLagDisableTestV6`, `EcmpReuseLagRouteV4`, `EcmpReuseLagRouteV6`, `RemoveAllNextHopMemeberTestV4`, `RemoveNexthopGroupTestV4` |
| `sai_neighbor_test` | `AddHostRouteTest`, `AddHostRouteTestV6`, `NhopDiffPrefixRemoveLonger`, `NhopDiffPrefixRemoveLongerV6`, `NhopDiffPrefixRemoveShorter`, `NhopDiffPrefixRemoveShorterV6`, `NoHostRouteTestV6` |
| `sai_rif_test` | `IngressDisableTestV4`, `IngressDisableTestV6` |
| `sai_route_test` | `DefaultRouteV4Test`, `DefaultRouteV6Test`, `DropRouteTest`, `DropRoutev6Test`, `LagMultipleRouteTest`, `LagMultipleRoutev6Test`, `RemoveRouteV4Test`, `RouteDiffPrefixAddThenDeleteLongerV4Test`, `RouteDiffPrefixAddThenDeleteLongerV6Test`, `RouteDiffPrefixAddThenDeleteShorterV4Test`, `RouteDiffPrefixAddThenDeleteShorterV6Test`, `RouteRifTest`, `RouteRifv6Test`, `RouteSameSipDipv4Test`, `RouteSameSipDipv6Test`, `RouteUpdateTest`, `RouteUpdatev6Test`, `StaicSviMacFloodingTest`, `StaicSviMacFloodingV6Test` |

**34** classes passing (of 89 executed in the last full matrix run).

---

## b) Building the framework

The image bundles pre-built `.deb` packages from `debs/`. You only need to
regenerate `.deb`s when the corresponding source changes.

### Required `.deb` packages (validated by the Dockerfile)
| Package glob | Source repo / how produced |
|---|---|
| `libvppinfra_*`, `vpp_*`, `vpp-plugin-core_*`, `vpp-plugin-dpdk_*` | VPP packages (from the `sonic-platform-vpp` build) |
| `libsaivs_*` | `sonic-sairedis` — the VPP SAI backend under test |
| `libsairedis_*`, `libsaimetadata_*` | `sonic-sairedis` |
| `libswsscommon_*` | `sonic-swss-common` |
| `libyang_*` | `sonic-buildimage` (libyang) |
| `saiserver_*` / `saiserverv2_*` | `sonic-sairedis` SAI Thrift server |
| `python-saithrift_*` / `python-saithriftv2_*` | `sonic-sairedis` SAI Thrift Python client |

All of these are staged in [`debs/`](debs/). The Dockerfile installs every
runtime `.deb` it finds there (skipping `-dbg`/`-dev`/`-dbgsym`).

### Regenerating the SAI `.deb`s (only when `vslib/` C++ changes)
From the **buildimage repo root** (`sonic-buildimage`):
```bash
cd <sonic-buildimage>

# Force re-generation by removing the stale targets first
rm -f target/debs/bookworm/libsairedis_*.deb target/debs/bookworm/libsairedis-dev_*.deb \
      target/debs/bookworm/libsaivs_*.deb     target/debs/bookworm/libsaivs-dev_*.deb

# Build libsaivs / libsairedis
NOTRIXIE=1 make target/debs/bookworm/libsairedis_1.0.0_amd64.deb

# Build saiserver + saithrift client (if those changed)
NOTRIXIE=1 BLDENV=bookworm make -f Makefile.work target/debs/bookworm/libsaithrift-dev_0.9.4_amd64.deb

# Stage the fresh packages into the harness build context
cp target/debs/bookworm/libsaivs_*.deb     target/debs/bookworm/libsaivs-dev_*.deb \
   target/debs/bookworm/libsairedis_*.deb  target/debs/bookworm/libsairedis-dev_*.deb \
   target/debs/bookworm/saiserver_*.deb    target/debs/bookworm/python-saithrift_*.deb \
   src/sonic-sairedis/.azure-pipelines/docker-sai-test-vpp/debs/
```
> Behind a corporate proxy, prefix `make` with your Docker build credentials,
> e.g. `DOCKER_CONFIG=<dir>`. VPP `.deb`s come from the `sonic-platform-vpp`
> pipeline; drop new ones into `debs/` to upgrade VPP.

### Building the image
From the **`sonic-sairedis`** directory:
```bash
cd <sonic-buildimage>/src/sonic-sairedis

docker build --no-cache \
  -f .azure-pipelines/docker-sai-test-vpp/Dockerfile \
  -t docker-sai-test-vpp:phase1 .
```
If you are behind a proxy, pass it through (both lower- and upper-case):
```bash
docker build --no-cache \
  --build-arg http_proxy=$http_proxy   --build-arg https_proxy=$https_proxy \
  --build-arg HTTP_PROXY=$http_proxy   --build-arg HTTPS_PROXY=$https_proxy \
  --build-arg no_proxy=$no_proxy       --build-arg NO_PROXY=$no_proxy \
  -f .azure-pipelines/docker-sai-test-vpp/Dockerfile \
  -t docker-sai-test-vpp:phase1 .
```

> Rebuild scope: editing only `run_test.sh` / `sai_test/**` needs an **image
> rebuild** only (fast). Editing `vslib/` C++ needs a **`.deb` rebuild** (above)
> first, then the image.

---

## c) Running tests

The container entrypoint is `run_test.sh`. Test selectors are PTF targets:
`module` (e.g. `sai_route_test`) or `module.Class`
(e.g. `sai_route_test.RouteRifTest`). `PORT_COUNT` sets the port/veth count
(use 32 for the standard T0 topology).

### Where the tests come from
The OCP `sai_test` suite is baked into the image at `/sai_test` (copied from
`SAI/test/sai_test/` in the repo). PTF and the SAI Thrift client come from
`SAI/test/ptf` and the `python-saithrift` package. `run_test.sh` discovers test
classes under `/sai_test` automatically.

### Run a single test
```bash
docker run --rm --privileged -e PORT_COUNT=32 \
  docker-sai-test-vpp:phase1 sai_route_test.RouteRifTest
```

### Run several tests (auto-grouped by config, one container)
```bash
docker run --rm --privileged -e PORT_COUNT=32 \
  docker-sai-test-vpp:phase1 \
  sai_route_test.RouteRifTest sai_ecmp_test.EcmpHashFieldSportTestV4
```

### Run whole modules, or everything
```bash
# whole modules
docker run --rm --privileged -e PORT_COUNT=32 \
  docker-sai-test-vpp:phase1 sai_route_test sai_rif_test sai_neighbor_test sai_ecmp_test

# every discovered test class (no args)
docker run --rm --privileged -e PORT_COUNT=32 docker-sai-test-vpp:phase1
```

### Collecting results (JUnit XML) and updating the supported-test list
PTF writes one JUnit-XML file per test into `/test-results`. By convention these
land in the local-only `results/` tree (git-ignored; not published to the remote).
Run from the `docker-sai-test-vpp/` directory and bind-mount `results/xml` out:
```bash
cd <sonic-buildimage>/src/sonic-sairedis/.azure-pipelines/docker-sai-test-vpp
mkdir -p results/xml
docker run --rm --privileged -e PORT_COUNT=32 \
  -v "$PWD/results/xml:/test-results" \
  docker-sai-test-vpp:phase1 \
  sai_route_test sai_rif_test sai_neighbor_test sai_ecmp_test \
  2>&1 | tee results/run.log
```
Then build a local compatibility matrix with the bundled generator (defaults to
the `results/` tree). The generator parses JUnit XML with `defusedxml` (hardened
against XXE), so install it once if needed:
```bash
pip install defusedxml
python3 gen_compatibility_matrix.py        # writes results/compatibility-matrix.md
```
`gen_compatibility_matrix.py` walks `results/xml/TEST-*.xml` and writes a
PASS/FAIL/ERROR/SKIP table with a count summary. You can also pass an explicit
`<xml_dir> [output.md]` to point it elsewhere.

**Publishing pass results:** when the set of passing tests changes, update the
**Supported tests** section in this `README.md` from the local matrix (list only
classes with PASS; do not commit the matrix itself). Working notes and deep-dive
logs may be kept under `devdocs/` (also local-only and git-ignored).

---

## d) Additional information

### Debug mode (leave VPP / saiserver / veths alive for inspection)
```bash
docker rm -f officesai-debug 2>/dev/null
docker run -d --name officesai-debug --privileged -e PORT_COUNT=32 \
  docker-sai-test-vpp:phase1 --debug sai_route_test.RouteRifTest

# while the test runs, inspect VPP state:
docker exec officesai-debug vppctl show interface
docker exec officesai-debug vppctl show ip fib
docker exec officesai-debug vppctl show bond
docker cp officesai-debug:/var/log/saiserver.log ./saiserver.log
```
In `--debug` the container leaves the dataplane running after the test so you can
use `vppctl`; remember to `docker rm -f officesai-debug` when done.

### Environment knobs
| Variable | Default | Meaning |
|---|---|---|
| `PORT_COUNT` | 32 | number of `OEthernetX`/`OEthX_peer` veth pairs |
| `COMMON_CONFIGURED_REUSE` | 1 | 1 = config-signature grouping + reuse; 0 = legacy single ptf invocation |
| `KEEP_VETHS_UP_SECONDS` | 120 | how long the per-group watchdog keeps VPP host-interfaces/veths up |
| `TEST_FILTER` | — | alternative way to pass a single selector via env |

### Logs inside the container
- `/var/log/saiserver.log` — high-level SAI RPC trace (saiserver runs with SWSS
  debug logging).
- `/var/log/vpp.log`, `/var/log/vpp-startup.log` — VPP CLI history / stdout
  (crash backtraces).
- `/var/log/vpp-api-trace.txt` — decoded VPP binary-API trace (dumped at teardown).
- `/test-results/TEST-*.xml` — per-test JUnit results (bind-mount to the local
  `results/xml/` directory on the host).

### Gotchas
- The container must be `--privileged` (raw AF_PACKET sockets on veths/TAPs).
- The VPP SAI backend can build the switch once per saiserver process; the
  per-group backend restart in `run_test.sh` handles this automatically — do not
  expect to re-run a full config build inside a single long-lived saiserver.
- A benign `buffer: numa[1] falling back to non-hugepage backed buffer pool`
  line at teardown is a host hugepage-availability warning, not a test failure.
