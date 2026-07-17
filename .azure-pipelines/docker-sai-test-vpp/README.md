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
- **`run_test.sh`** is the container entrypoint and orchestrator: Redis → veth topology → VPP → saiserver → PTF. It writes the SONiC-to-VPP interface map to `/usr/share/sonic/hwsku/sonic_vpp_ifmap.ini`. PortChannel netdevs and LAG/SVI connected IPs are set up in sai_test `setUp()` via sai_test's `SIMULATE_SONIC` helper (`config/simulate_sonic.py`), not in `run_test.sh`.

### Key design point — per-test isolation (default) and config-signature grouping

The VPP SAI backend can build the switch + host-interfaces **once per saiserver process**. The OCP framework is built to configure a common T0 setup once and have subsequent tests reuse it (`common_configured=true`). But different test classes ask `T0TestBase.setUp` for *different* common configs (e.g. ECMP tests need next-hop groups), and rebuilding the full T0 config twice in one saiserver process can crash the backend.

**Default (`ISOLATE_EACH_TEST=1`):** every requested test class runs in its **own** config group. Before each test, `run_test.sh` tears down and restarts Redis + VPP + saiserver, waits for the old VPP process to be fully reaped, then starts a fresh backend. Each test rebuilds its own common config (`common_configured=false`). This eliminates cross-test state contamination (ECMP `-6`/`-7` reuse artifacts, ordering-dependent flakes) at the cost of ~45–50 minutes for the full 4-module matrix (87 tests × ~30s recycle+rebuild each).

**Grouped mode (`ISOLATE_EACH_TEST=0`):** `run_test.sh` parses each test class's `setUp` (resolving config kwargs through the class **inheritance chain**) to compute a common-config "signature", groups tests by signature, and for each group restarts the backend once: the first test in the group builds + persists that group's config, the rest reuse it (`common_configured=true`). This is ~9× faster but passes fewer tests when upstream workarounds are not present.

Set `COMMON_CONFIGURED_REUSE=0` only for legacy single-invocation debugging (one ptf process, no grouping).

### Supported tests

The table below lists OCP `sai_test` classes that **pass** on the current VPP SAI backend (last validated **2026-07-14** against `sai_route_test sai_rif_test sai_neighbor_test sai_ecmp_test` on `sai_vpp_ut_phase3`, `ISOLATE_EACH_TEST=1`). It is the published substitute for a full compatibility matrix: only passing tests are listed. After a local matrix run, update this section when the pass set changes (see **Collecting results** below).

| Module | Passing test classes (CI required) |
|---|---|
| `sai_ecmp_test` | `EcmpLagDisableTestV4`, `EcmpLagDisableTestV6`, `EcmpReuseLagRouteV4`, `EcmpReuseLagRouteV6`, `RemoveAllNextHopMemeberTestV4`, `RemoveNexthopGroupTestV4` |
| `sai_neighbor_test` | `AddHostRouteTest`, `AddHostRouteTestV6`, `NhopDiffPrefixRemoveLonger`, `NhopDiffPrefixRemoveLongerV6`, `NhopDiffPrefixRemoveShorter`, `NhopDiffPrefixRemoveShorterV6`, `NoHostRouteTestV6` |
| `sai_rif_test` | `IngressDisableTestV4`, `IngressDisableTestV6` |
| `sai_route_test` | `DefaultRouteV4Test`, `DefaultRouteV6Test`, `DropRouteTest`, `DropRoutev6Test`, `LagMultipleRouteTest`, `LagMultipleRoutev6Test`, `RemoveRouteV4Test`, `RouteDiffPrefixAddThenDeleteLongerV4Test`, `RouteDiffPrefixAddThenDeleteLongerV6Test`, `RouteDiffPrefixAddThenDeleteShorterV4Test`, `RouteDiffPrefixAddThenDeleteShorterV6Test`, `RouteRifTest`, `RouteRifv6Test`, `RouteSameSipDipv4Test`, `RouteSameSipDipv6Test`, `RouteUpdateTest`, `RouteUpdatev6Test`, `StaicSviMacFloodingTest`, `StaicSviMacFloodingV6Test` |

**34** classes are required to pass the PR check (`ci-pass-tests.txt`). The harness plans **87** test targets across the four modules above; `gen_compatibility_matrix.py` may report a higher row count when a test produces both ERROR and FAIL JUnit entries.

#### Flaky L3-over-LAG ECMP tests (run, not gated)

Three additional `sai_ecmp_test` classes have passed in clean local matrix runs but are **dataplane-flaky** (hash/L3-over-LAG forwarding: pass/fail can flip run-to-run with no code change). They remain in the full matrix (`ci-matrix-tests.txt`) for visibility but are **not** in `ci-pass-tests.txt` and do not fail the PR check:

| Module | Flaky test classes |
|---|---|
| `sai_ecmp_test` | `ReAddLagEcmpTestV4`, `RemoveLagEcmpTestV4`, `RemoveLagEcmpTestV6` |

Promote one of these into `ci-pass-tests.txt` only after repeated clean runs show it is stable.

### CI regression baseline

`ci-matrix-tests.txt` is the expected set of 85 runnable selectors from the four-module plan (the 87 planned classes include two non-runnable base classes). `ci-pass-tests.txt` is the **34-selector** stable-pass subset used by the PR check. CI requires the observed JUnit selector set to match the matrix contract, then leaves failures outside the stable baseline visible while failing on a missing, failed, errored, or skipped baseline selector.

`evaluate_ci_baseline.py` compares the JUnit directory with the baseline, reports newly passing selectors as promotion candidates, and treats missing or malformed results and harness exit codes of 2 or greater as infrastructure failures. Baseline changes are reviewed explicitly; CI never updates the file automatically.

When a runnable test class is added, removed, or renamed in one of the four CI modules, update the sorted, fully qualified selectors in `ci-matrix-tests.txt` in the same reviewed change.

## b) Building the framework

The image bundles pre-built `.deb` packages from `debs/` (git-ignored locally). You only need to regenerate `.deb`s when the corresponding source changes. All examples below use the local image tag **`docker-sai-test-vpp:local`** (the `build_harness.sh` default).

### Use cases

The framework is designed to support three distinct deployment and testing scenarios:

#### **Use case 1: Local dev in a `sonic-buildimage` workspace**
- **What changes:** `vslib/` C++ backend, `SAI/test/sai_test` Python tests, VPP packages, or harness scripts.
- **How dependencies are supplied:** Copy runtime `.deb`s from `target/debs/<suite>/` into `docker-sai-test-vpp/debs/`, then build the image. The default suite today is **trixie** (see Dockerfile).
- **Status:** Supported today via `build_harness.sh` (below). Assumes the workspace layout `sonic-buildimage/src/sonic-sairedis`. VPP packages can come from `target/debs/trixie/` after a platform-vpp build, from a `sonic-platform-vpp` pipeline download, or from `VPP_DEB_DIR` when running the build script.

#### **Use case 2: `sonic-sairedis` PR CI**
- **What changes:** `vslib/` C++ backend, harness files, or OCP tests.
- **How dependencies are supplied:** The pipeline's **Build** stage compiles and produces fresh `libsairedis` / `libsaivs` / `saiserver` / `python-saithrift` artifacts. Other runtime `.deb`s (`libswsscommon`, `libyang`, VPP) must be downloaded from existing pipeline artifacts — following the same pattern as `.azure-pipelines/build-docker-sonic-vs-template.yml` (swss-common pipeline, sonic-platform-vpp `vpp-trixie`, buildimage common libs).
- **Status:** Pipeline wiring is implemented by `BuildSaiTestVpp` and `TestSaiVpp`; Azure artifact authorization and burn-in are required before making the check mandatory.
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

All of these are staged in [`debs/`](debs/) (local-only). The Dockerfile installs every runtime `.deb` it finds there (skipping `-dbg`/`-dev`/`-dbgsym`). When both legacy and v2 saithrift packages are present, **`saiserverv2_*` and `python-saithriftv2_*` are preferred**.

### Build script (use case 1)

`build_harness.sh` automates staging from `sonic-buildimage` and building the image as **`docker-sai-test-vpp:local`**. It validates that all required runtime `.deb`s are present in `debs/` before `docker build`. If sonic-sairedis packages are missing, it runs the trixie `make` targets automatically (disable with `--no-auto-build`). VPP, `libswsscommon`, and `libyang`/`libyang3` are not auto-built — the script fails with hints if they are absent.

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

# Stage the fresh packages into the harness build context (prefer v2 names on trixie)
cp target/debs/trixie/libsaivs_*.deb     target/debs/trixie/libsaivs-dev_*.deb \
   target/debs/trixie/libsairedis_*.deb  target/debs/trixie/libsairedis-dev_*.deb \
   target/debs/trixie/libsaimetadata_*.deb \
   target/debs/trixie/saiserverv2_*.deb  target/debs/trixie/python-saithriftv2_*.deb \
   src/sonic-sairedis/.azure-pipelines/docker-sai-test-vpp/debs/
```

Then run `./build_harness.sh` (or `./build_harness.sh --no-stage-debs` if `debs/` is already up to date).

#### Trixie `libsaithrift-dev` build failure (workaround)

On trixie, `make -f Makefile.work target/debs/trixie/libsaithrift-dev_0.9.4_amd64.deb` may fail (legacy `libthrift-0.11.0` / python2 debhelper dependency). `libsairedis` / `libsaivs` debs still build. If saithrift packaging fails:

1. Build `libsairedis` as above (produces fresh `libsaivs` / `libsaimetadata`).
2. Build `saiserver` manually inside `sonic-slave-trixie` from the sairedis tree, or copy a known-good `saiserverv2_*.deb` into `debs/`.
3. Rebuild the harness image with `./build_harness.sh --no-stage-debs`.

The image must contain a `saiserverv2` binary built against the same `libsaivs` as the staged debs — a stale saiserver deb will produce confusing Thrift/runtime failures.

> Behind a corporate proxy, prefix `make` with your Docker build credentials, e.g. `DOCKER_CONFIG=<dir>`. VPP `.deb`s come from the `sonic-platform-vpp` pipeline; drop new ones into `debs/` or pass `--vpp-deb-dir`. `build_harness.sh` forwards proxy env vars to `docker build`.

### Building the image manually

Equivalent to `./build_harness.sh --no-stage-debs` after `debs/` is populated. From the **`sonic-sairedis`** directory:

```bash
cd <sonic-buildimage>/src/sonic-sairedis

docker build --no-cache \
  -f .azure-pipelines/docker-sai-test-vpp/Dockerfile \
  -t docker-sai-test-vpp:local .
```

If you are behind a proxy, pass it through (both lower- and upper-case):

```bash
docker build --no-cache \
  --build-arg http_proxy=$http_proxy   --build-arg https_proxy=$https_proxy \
  --build-arg HTTP_PROXY=$http_proxy   --build-arg HTTPS_PROXY=$https_proxy \
  --build-arg no_proxy=$no_proxy       --build-arg NO_PROXY=$no_proxy \
  -f .azure-pipelines/docker-sai-test-vpp/Dockerfile \
  -t docker-sai-test-vpp:local .
```

> Rebuild scope: editing only `run_test.sh` / `sai_test/**` / harness templates needs an **image rebuild** only (fast). Editing `vslib/` C++ needs a **`.deb` rebuild** (above) first, then the image.

### VPP startup plugins (`vpp_startup.conf.template`)

The harness-generated VPP config enables plugins required by current VPP + libsaivs, including:

- **`tap_plugin.so`** — needed for VPP 26.06 linux-cp host-interface creation.
- **`sflow_plugin.so`** — saiserver startup asserts if the sflow message-id base is missing when this plugin is disabled.

Other enabled plugins (`af_packet`, `linux_cp`, `linux_nl`, `acl`, `vxlan`, …) match the VPP SAI dataplane bench. Do not trim these without validating saiserver startup and host-interface creation.

## c) Running tests

The container entrypoint is `run_test.sh`. Test selectors are PTF targets: `module` (e.g. `sai_route_test`) or `module.Class` (e.g. `sai_route_test.RouteRifTest`). `PORT_COUNT` sets the port/veth count (use 32 for the standard T0 topology).

### Where the tests come from

The OCP `sai_test` suite is baked into the image at `/sai_test` (copied from `SAI/test/sai_test/` in the repo). PTF and the SAI Thrift client come from `SAI/test/ptf` and the `python-saithrift` / `python-saithriftv2` package. `run_test.sh` discovers test classes under `/sai_test` automatically.

### Run a single test

```bash
docker run --rm --privileged -e PORT_COUNT=32 \
  docker-sai-test-vpp:local sai_route_test.RouteRifTest
```

### Run several tests (one container, grouped or isolated per env)

```bash
docker run --rm --privileged -e PORT_COUNT=32 \
  docker-sai-test-vpp:local \
  sai_route_test.RouteRifTest sai_ecmp_test.EcmpHashFieldSportTestV4
```

With default `ISOLATE_EACH_TEST=1`, each selector still gets its own fresh backend even when multiple classes are listed.

### Run whole modules, or everything

```bash
# whole modules (default isolation: ~45-50 min for all four)
docker run --rm --privileged -e PORT_COUNT=32 \
  docker-sai-test-vpp:local sai_route_test sai_rif_test sai_neighbor_test sai_ecmp_test

# faster grouped mode (~9x; fewer passes without upstream workarounds)
docker run --rm --privileged -e PORT_COUNT=32 -e ISOLATE_EACH_TEST=0 \
  docker-sai-test-vpp:local sai_route_test sai_rif_test sai_neighbor_test sai_ecmp_test

# every discovered test class (no args)
docker run --rm --privileged -e PORT_COUNT=32 docker-sai-test-vpp:local
```

### Collecting results (JUnit XML) and updating the supported-test list

PTF writes one JUnit-XML file per test into `/test-results`. By convention these land in the local-only `results/` tree (git-ignored; not published to the remote). Run from the `docker-sai-test-vpp/` directory and bind-mount `results/xml` out.

For a full 4-module matrix, prefer **`nohup`** (or an equivalent detached logger) so a dropped SSH/IDE session does not kill the container mid-run. Tail the log file for progress — long runs produce megabytes of output and some IDE terminals stop updating while the run continues.

```bash
cd <sonic-buildimage>/src/sonic-sairedis/.azure-pipelines/docker-sai-test-vpp
mkdir -p results/xml
nohup docker run --rm --privileged -e PORT_COUNT=32 \
  -v "$PWD/results/xml:/test-results" \
  docker-sai-test-vpp:local \
  sai_route_test sai_rif_test sai_neighbor_test sai_ecmp_test \
  > results/run.log 2>&1 &
tail -f results/run.log
```

### Monitor

While a matrix run is in progress (from the `docker-sai-test-vpp/` directory):

| | |
|---|---|
| **Log** | `results/run.log` (or whatever path you passed to `nohup` / `tee`) |
| **Progress** | `grep -oE '\[[0-9]+/87\]' results/run.log \| tail -1` |
| **Container** | `docker ps --filter ancestor=docker-sai-test-vpp:local` |

The `87` in the progress grep matches the four-module isolation plan (`sai_route_test sai_rif_test sai_neighbor_test sai_ecmp_test`). For other selectors, use `grep -oE '\[[0-9]+/[0-9]+\]'` instead.

Monitor progress without the full firehose:

```bash
grep -oE '\[[0-9]+/[0-9]+\]' results/run.log | tail -1
grep -E '^(OK|FAIL|ERROR) |\[run_test\] ===' results/run.log | tail -20
```

Then build a local compatibility matrix with the bundled generator (defaults to the `results/` tree). The generator parses JUnit XML with `defusedxml` (hardened against XXE), so install it once if needed:

```bash
pip install defusedxml
python3 gen_compatibility_matrix.py        # writes results/compatibility-matrix.md
```

`gen_compatibility_matrix.py` walks `results/xml/TEST-*.xml` and writes a PASS/FAIL/ERROR/SKIP table with a count summary. You can also pass an explicit `<xml_dir> [output.md]` to point it elsewhere.

Evaluate the same results against the PR baseline with the matrix process exit code (`0` for an all-pass matrix, `1` when test failures are present, or `2+` for setup failure):

```bash
python3 evaluate_ci_baseline.py \
  --xml-dir results/xml \
  --baseline ci-pass-tests.txt \
  --expected ci-matrix-tests.txt \
  --matrix-rc 1 \
  --report results/baseline-report.txt
```

**Publishing pass results:** when repeated clean runs establish a newly stable pass, add its fully qualified selector to `ci-pass-tests.txt` and update the **Supported tests** section in this `README.md` in the same reviewed change. List only PASS classes and do not commit the generated matrix; working notes and deep-dive logs may be kept under `devdocs/` (also local-only and git-ignored).

## d) Additional information

Debug hold mode, environment variables, in-container log paths, and common pitfalls.

### Debug mode (leave VPP / saiserver / veths alive for inspection)

```bash
docker rm -f officesai-debug 2>/dev/null
docker run -d --name officesai-debug --privileged -e PORT_COUNT=32 \
  docker-sai-test-vpp:local --debug sai_route_test.RouteRifTest

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
| `ISOLATE_EACH_TEST` | 1 | 1 = fresh backend + own config per test; 0 = config-signature grouping with reuse |
| `COMMON_CONFIGURED_REUSE` | 1 | 1 = plan multi-target runs with grouping/isolation; 0 = legacy single ptf invocation |
| `SAI_PORT_UP_SHARED_WAIT` | 1 | 1 = poll all ports together in `port_configer.py` (seconds, not ~64s serial) |
| `SAI_PORT_UP_RETRIES` | 2 | shared-wait retry count (with `SAI_PORT_UP_SHARED_WAIT=1`) |
| `SAI_PORT_UP_POLL_INTERVAL` | 1 | seconds between shared-wait polls |
| `KEEP_VETHS_UP_SECONDS` | 120 | how long the per-group watchdog keeps VPP host-interfaces/veths up |
| `KEEP_VETHS_UP_INTERVAL` | 3 | seconds between watchdog `vppctl set interface state` batches |
| `LAG_COUNT` | 4 | PortChannel netdevs torn down at container exit |
| `MTU` | 9100 | veth MTU |
| `STARTUP_TIMEOUT` | 60 | seconds to wait for VPP / saiserver readiness |
| `THRIFT_PORT` | 9092 | saiserver Thrift listen port |
| `LAG_RIF_IPS` | 1 | enable LAG RIF connected-IP assignment in sai_test setUp (`SIMULATE_SONIC`) |
| `SVI_RIF_IPS` | 1 | enable SVI RIF connected-IP assignment in sai_test setUp |
| `SIMULATE_SONIC` | 1 | set by `run_test.sh`; enables sai_test's SONiC control-plane simulation (PortChannel netdevs + LAG/SVI RIF IPs) |
| `TEST_FILTER` | — | alternative way to pass a single selector via env |

These are read by `run_test.sh` at container start (defaults shown).

### Logs inside the container

- `/var/log/saiserver.log` — saiserver stdout/stderr **plus** `libsaivs` `SWSS_LOG_*` lines. After the SAI change that removed `swss::Logger` setup from `saiserver.cpp`, the harness routes `SWSS_LOG_*` to **stderr** via an `LD_PRELOAD` shim (`swss_log_stdout_preload.cpp` → `/usr/local/lib/libswss_log_stdout.so`; the `.so` name is historical). `run_test.sh` redirects saiserver `2>&1` into this file so backend traces still land here. Routing to stderr (not stdout) keeps `SWSS_LOG_ENTER` lines out of `swss::exec()` captured stdout (e.g. the `find_new_bond_id` shell pipeline used during LAG create).
- `/var/log/vpp.log`, `/var/log/vpp-startup.log` — VPP CLI history / stdout (crash backtraces).
- `/var/log/vpp-api-trace.txt` — decoded VPP binary-API trace (dumped at teardown).
- `/test-results/TEST-*.xml` — per-test JUnit results (bind-mount to the local `results/xml/` directory on the host).

### Gotchas

- The container must be `--privileged` (raw AF_PACKET sockets on veths/TAPs).
- The VPP SAI backend can build the switch once per saiserver process; `run_test.sh` restarts the backend per test (default) or per config-signature group — do not expect to re-run a full config build inside a single long-lived saiserver.
- **`stop_backend()` waits for child exit:** `terminate_process()` calls `wait` on VPP/saiserver/redis PIDs after they die so the next `start_vpp()` cannot overlap with a dying instance (avoids transient `Killed vpp` during per-test recycle).
- **`Set port...` / `Turn up ports...` looks hung:** each test's T0 common-config build spends ~10–20s on port admin-up and veth bring-up with little console output; this is normal, not a deadlock.
- **Long matrix runs:** use `nohup` + `tail -f results/run.log`; do not rely on IDE agent terminals for live progress on 87-test isolation runs.
- A benign `buffer: numa[1] falling back to non-hugepage backed buffer pool` line at teardown is a host hugepage-availability warning, not a test failure.
