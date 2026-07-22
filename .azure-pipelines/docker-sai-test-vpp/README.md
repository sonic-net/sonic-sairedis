# docker-sai-test-vpp — VPP SAI Unit-Test Framework

A self-contained, single-container framework for validating the **SAI API implementation of the VPP virtual-switch backend** (`libsaivs.so`) by running the OpenComputeProject (OCP) `sai_test` PTF suite against a real VPP dataplane.

## a) Purpose and design

### What it does

The framework exercises SAI operations (create/set/get/remove of ports, RIFs, routes, neighbors, VLANs, FDB, LAGs, ECMP, …) through a Thrift RPC interface and verifies the resulting **data-plane** behavior by injecting and capturing packets on virtual interfaces. It is the test vehicle for finding gaps between the SAI contract and the VPP backend, and for producing a per-test **compatibility matrix**.

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

- **VPP** is the dataplane; `OEthernetX` are the VPP-facing kernel veth ends and `OEthX_peer` are the PTF-facing ends. `OEthernetX` represents an out-facing (wire) interface; the inside `EthernetX` (linux_cp TAP) faces the SONiC control plane.
- **saiserver** is a thin Thrift→SAI shim (port 9092) linked against `libsaivs.so`.
- **PTF** runs the OCP `sai_test` Python suite and does packet I/O on the `OEthX_peer` ends.
- **`run_test.sh`** is the container entrypoint and orchestrator: Redis → veth topology → VPP → saiserver → PTF. PortChannel netdevs and LAG/SVI connected IPs are set up in sai_test `setUp()` via sai_test's `SIMULATE_SONIC` helper (`config/simulate_sonic.py`), not in `run_test.sh`.

### Key design point — config-signature grouping

The VPP SAI backend can build the switch + host-interfaces **once per saiserver process**. The OCP framework is built to configure a common T0 setup once and have subsequent tests reuse it (`common_configured=true`). But different test classes ask `T0TestBase.setUp` for *different* common configs (e.g. ECMP tests need next-hop groups). So `run_test.sh`:

1. parses each requested test class's `setUp` (resolving config kwargs through the class **inheritance chain**) to compute its common-config "signature";
2. groups tests by signature;
3. for each group: **restarts the backend** (fresh saiserver), the first test builds + persists that group's config, the rest reuse it.

This lets one container run any mix of tests correctly in a single invocation.

### Supported tests

The table below lists OCP `sai_test` classes that **pass** on the current VPP SAI backend (last strict validation **2026-07-21** against `sai_route_test sai_rif_test sai_neighbor_test sai_ecmp_test`). It is the published substitute for a full compatibility matrix: only passing tests are listed. After a local matrix run, update this section when the pass set changes (see **Collecting results** below).

| Module | Passing test classes |
|---|---|
| `sai_ecmp_test` | `EcmpLagDisableTestV4`, `EcmpLagDisableTestV6`, `EcmpReuseLagRouteV4`, `EcmpReuseLagRouteV6`, `RemoveAllNextHopMemeberTestV4`, `RemoveNexthopGroupTestV4` |
| `sai_neighbor_test` | `AddHostRouteTestV6`, `NhopDiffPrefixRemoveLonger`, `NhopDiffPrefixRemoveLongerV6`, `NhopDiffPrefixRemoveShorter`, `NhopDiffPrefixRemoveShorterV6` |
| `sai_rif_test` | — |
| `sai_route_test` | `LagMultipleRoutev6Test`, `RemoveRouteV4Test`, `RouteDiffPrefixAddThenDeleteLongerV4Test`, `RouteDiffPrefixAddThenDeleteLongerV6Test`, `RouteDiffPrefixAddThenDeleteShorterV4Test`, `RouteDiffPrefixAddThenDeleteShorterV6Test`, `RouteSameSipDipv6Test`, `StaicSviMacFloodingTest` |

**19** classes passing (of 91 JUnit rows in the last strict full matrix run).

## b) Building the framework

The image bundles pre-built `.deb` packages from `debs/` (git-ignored locally). You only need to regenerate `.deb`s when the corresponding source changes.

### Use cases

The framework is designed to support three distinct deployment and testing scenarios:

#### **Use case 1: Local dev in a `sonic-buildimage` workspace**
- **What changes:** `vslib/` C++ backend, `SAI/test/sai_test` Python tests, VPP packages, or harness scripts.
- **How dependencies are supplied:** Copy runtime `.deb`s from `target/debs/<suite>/` into `docker-sai-test-vpp/debs/`, then build the image. The default suite today is **trixie** (see Dockerfile).
- **Status:** Supported today via `build_harness.sh` (below). Assumes the workspace layout `sonic-buildimage/src/sonic-sairedis`. VPP packages can come from `target/debs/trixie/` after a platform-vpp build, from a `sonic-platform-vpp` pipeline download, or from `VPP_DEB_DIR` when running the build script.

#### **Use case 2: `sonic-sairedis` PR CI**
- **What changes:** `vslib/` C++ backend, harness files, or OCP tests.
- **How dependencies are supplied:** The pipeline's **Build** stage compiles and produces fresh `libsairedis` / `libsaivs` / `saiserver` / `python-saithrift` artifacts. Other runtime `.deb`s (`libswsscommon`, `libyang`, VPP) must be downloaded from existing pipeline artifacts — following the same pattern as `.azure-pipelines/build-docker-sonic-vs-template.yml` (swss-common pipeline, sonic-platform-vpp `vpp-trixie`, buildimage common libs).
- **Status:** Documented intent; CI wiring is follow-up work for this PR (Phase 3).
- **Required runtime packages and typical artifact sources:**

| Package glob | PR build produces? | Typical CI download source |
|---|---|---|
| `libsaivs_*`, `libsairedis_*`, `libsaimetadata_*`, `saiserverv2_*`, `python-saithriftv2_*` | Yes (this repo's Build stage) | Current pipeline job artifacts |
| `libswsscommon_*` | No | `Azure.sonic-swss-common` pipeline artifact |
| `libyang_*` | No | `sonic-buildimage` common-lib / VS build artifact |
| `libvppinfra_*`, `vpp_*`, `vpp-plugin-*` | No | `sonic-net.sonic-platform-vpp` artifact `vpp-trixie` |

#### **Use case 3: `sonic-platform-vpp` PR CI**
- **What changes:** VPP `.deb`s only.
- **How dependencies are supplied:** Pull a pre-built `docker-sai-test-vpp` image from the `sonic-sairedis` pipeline artifact, then rebuild/replace only the VPP packages inside `debs/` (or `docker build` with updated VPP debs on top of the cached layers).
- **Status:** Documented intent; requires Use Case 2 image publish first.
- **Workflow:** After the harness image is published as a pipeline artifact in Use Case 2, a platform-vpp job can `docker load` that image, overlay freshly built VPP `.deb`s into `debs/`, and rebuild only the package-install layer (or run tests in a container with VPP packages swapped in).

### Required `.deb` packages (validated by the Dockerfile)

| Package glob | Source repo / how produced |
|---|---|
| `libvppinfra_*`, `vpp_*`, `vpp-plugin-core_*`, `vpp-plugin-dpdk_*` | VPP packages (from the `sonic-platform-vpp` build) |
| `libsaivs_*` | `sonic-sairedis` — the VPP SAI backend under test |
| `libsairedis_*`, `libsaimetadata_*` | `sonic-sairedis` |
| `libswsscommon_*` | `sonic-swss-common` |
| `libyang_*` / `libyang3_*` | `sonic-buildimage` (libyang3 on trixie) |
| `saiserver_*` / `saiserverv2_*` | `sonic-sairedis` SAI Thrift server |
| `python-saithrift_*` / `python-saithriftv2_*` | `sonic-sairedis` SAI Thrift Python client |

All of these are staged in [`debs/`](debs/) (local-only). The Dockerfile installs every runtime `.deb` it finds there (skipping `-dbg`/`-dev`/`-dbgsym`).

### Build script (use case 1)

`build_harness.sh` automates staging from `sonic-buildimage` and building the image. It validates that all required runtime `.deb`s are present in `debs/` before `docker build`. If sonic-sairedis packages are missing, it runs the trixie `make` targets automatically (disable with `--no-auto-build`). VPP, `libswsscommon`, and `libyang`/`libyang3` are not auto-built — the script fails with hints if they are absent.

```bash
cd <sonic-buildimage>/src/sonic-sairedis/.azure-pipelines/docker-sai-test-vpp
./build_harness.sh
```

Useful options: `--build-sairedis` (force-rebuild sairedis debs even when present), `--no-auto-build` (never invoke `make` for missing sairedis debs), `--no-stage-debs` (image rebuild only), `--vpp-deb-dir <path>`, `--image-tag <tag>`. Run `./build_harness.sh --help` for the full list.

### Regenerating the SAI `.deb`s manually (only when `vslib/` C++ changes)

From the **buildimage repo root** (`sonic-buildimage`):

```bash
cd <sonic-buildimage>

# Force re-generation by removing the stale targets first
rm -f target/debs/trixie/libsairedis_*.deb target/debs/trixie/libsairedis-dev_*.deb \
      target/debs/trixie/libsaivs_*.deb     target/debs/trixie/libsaivs-dev_*.deb

# Build libsaivs / libsairedis
make target/debs/trixie/libsairedis_1.0.0_amd64.deb

# Build saiserver + saithrift client (if those changed)
BLDENV=trixie make -f Makefile.work target/debs/trixie/libsaithrift-dev_0.9.4_amd64.deb

# Stage the fresh packages into the harness build context
cp target/debs/trixie/libsaivs_*.deb     target/debs/trixie/libsaivs-dev_*.deb \
   target/debs/trixie/libsairedis_*.deb  target/debs/trixie/libsairedis-dev_*.deb \
   target/debs/trixie/saiserver_*.deb    target/debs/trixie/python-saithrift_*.deb \
   src/sonic-sairedis/.azure-pipelines/docker-sai-test-vpp/debs/
```

Then run `./build_harness.sh` (or `./build_harness.sh --no-stage-debs` if `debs/` is already up to date).

> Behind a corporate proxy, prefix `make` with your Docker build credentials, e.g. `DOCKER_CONFIG=<dir>`. VPP `.deb`s come from the `sonic-platform-vpp` pipeline; drop new ones into `debs/` or pass `--vpp-deb-dir`. `build_harness.sh` forwards proxy env vars to `docker build`.

### Building the image manually

Equivalent to `./build_harness.sh --no-stage-debs` after `debs/` is populated. From the **`sonic-sairedis`** directory:

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

> Rebuild scope: editing only `run_test.sh` / `sai_test/**` needs an **image rebuild** only (fast). Editing `vslib/` C++ needs a **`.deb` rebuild** (above) first, then the image.

## c) Running tests

The container entrypoint is `run_test.sh`. Test selectors are PTF targets: `module` (e.g. `sai_route_test`) or `module.Class` (e.g. `sai_route_test.RouteRifTest`). `PORT_COUNT` sets the port/veth count (use 32 for the standard T0 topology).

### Where the tests come from

The OCP `sai_test` suite is baked into the image at `/sai_test` (copied from `SAI/test/sai_test/` in the repo). PTF and the SAI Thrift client come from `SAI/test/ptf` and the `python-saithrift` package. `run_test.sh` discovers test classes under `/sai_test` automatically.

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

PTF writes one JUnit-XML file per test into `/test-results`. By convention these land in the local-only `results/` tree (git-ignored; not published to the remote). Run from the `docker-sai-test-vpp/` directory and bind-mount `results/xml` out:

```bash
cd <sonic-buildimage>/src/sonic-sairedis/.azure-pipelines/docker-sai-test-vpp
mkdir -p results/xml
rm -f results/xml/TEST-*.xml
docker run --rm --privileged -e PORT_COUNT=32 \
  -v "$PWD/results/xml:/test-results" \
  docker-sai-test-vpp:phase1 \
  sai_route_test sai_rif_test sai_neighbor_test sai_ecmp_test \
  2>&1 | tee results/run.log
```

Then build a local compatibility matrix with the bundled generator (defaults to the `results/` tree). The generator parses JUnit XML with `defusedxml` (hardened against XXE), so install it once if needed:

```bash
pip install defusedxml
python3 gen_compatibility_matrix.py        # writes results/compatibility-matrix.md
```

`gen_compatibility_matrix.py` walks `results/xml/TEST-*.xml` and writes a PASS/FAIL/ERROR/SKIP table with a count summary. You can also pass an explicit `<xml_dir> [output.md]` to point it elsewhere.

**Publishing pass results:** when the set of passing tests changes, update the **Supported tests** section in this `README.md` from the local matrix (list only classes with PASS; do not commit the matrix itself). Working notes and deep-dive logs may be kept under `devdocs/` (also local-only and git-ignored).

## d) Additional information

Debug hold mode, environment variables, in-container log paths, and common pitfalls.

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

In `--debug` the container leaves the dataplane running after the test so you can use `vppctl`; remember to `docker rm -f officesai-debug` when done.

### Environment knobs

| Variable | Default | Meaning |
|---|---|---|
| `PORT_COUNT` | 32 | number of `OEthernetX`/`OEthX_peer` veth pairs |
| `COMMON_CONFIGURED_REUSE` | 1 | 1 = config-signature grouping + reuse; 0 = legacy single ptf invocation |
| `KEEP_VETHS_UP_SECONDS` | 120 | how long the per-group watchdog keeps VPP host-interfaces/veths up |
| `LAG_RIF_IPS` | 1 | enable LAG RIF connected-IP assignment in sai_test setUp (`SIMULATE_SONIC`) |
| `SVI_RIF_IPS` | 1 | enable SVI RIF connected-IP assignment in sai_test setUp |
| `SIMULATE_SONIC` | 1 | set by `run_test.sh`; enables sai_test's SONiC control-plane simulation (PortChannel netdevs + LAG/SVI RIF IPs) |
| `TEST_FILTER` | — | alternative way to pass a single selector via env |

These are read by `run_test.sh` at container start (defaults shown).

### Logs inside the container

- `/var/log/saiserver.log` — saiserver stdout/stderr **plus** `libsaivs` `SWSS_LOG_*` lines. After the SAI change that removed `swss::Logger` setup from `saiserver.cpp`, the harness routes `SWSS_LOG_*` to stdout via an `LD_PRELOAD` shim (`swss_log_stdout_preload.cpp` → `/usr/local/lib/libswss_log_stdout.so`) so SAI backend traces still land in this file. `sai_log_set()` API-level notices from saiserver itself are also configured there.
- `/var/log/vpp.log`, `/var/log/vpp-startup.log` — VPP CLI history / stdout (crash backtraces).
- `/var/log/vpp-api-trace.txt` — decoded VPP binary-API trace (dumped at teardown).
- `/test-results/TEST-*.xml` — per-test JUnit results (bind-mount to the local `results/xml/` directory on the host).

### Gotchas

- The container must be `--privileged` (raw AF_PACKET sockets on veths/TAPs).
- The VPP SAI backend can build the switch once per saiserver process; the per-group backend restart in `run_test.sh` handles this automatically — do not expect to re-run a full config build inside a single long-lived saiserver.
- A benign `buffer: numa[1] falling back to non-hugepage backed buffer pool` line at teardown is a host hugepage-availability warning, not a test failure.
