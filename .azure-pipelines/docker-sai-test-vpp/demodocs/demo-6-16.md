# VPP SAI Unit-Test Framework — Stakeholder Demo

## 1) What this is

- A **single-container framework** that validates the **SAI implementation of the VPP virtual-switch backend** (`libsaivs.so`) against a **real VPP dataplane**.
- Runs the OCP `sai_test` PTF suite over Thrift, injects/captures packets on virtual interfaces, and emits a per-test **compatibility matrix**.
- Goal: find the gaps between the SAI contract and the VPP backend — and track progress.

**Demo agenda:** ① build & run → ② what's inside the container → ③ run passing tests + peek at VPP state → ④ where we are (matrix: lots still to do).

---

## 2) Architecture

```
┌──────────────────────────────────────────────────────────────────────┐
│ docker-sai-test-vpp  (--privileged)                                  │
│                                                                      │
│  ┌───────────┐     ┌──────────────┐      ┌────────────────────────┐  │
│  │ VPP       │     │ saiserver    │      │ PTF test runner        │  │
│  │ af_packet │◄───►│ libsaivs.so  │◄────►│ sai_test/*.py          │  │
│  │ linux_cp  │ VPP │ (VPP SAI)    │Thrift│ sai_thrift adapter     │  │
│  └─────┬─────┘ API └──────────────┘ :9092└───────────┬────────────┘  │
│        │ AF_PACKET                          raw socket │ (AF_PACKET) │
│   OEthernet0 ◄════ veth ════► OEth0_peer ──────────────┘             │
│   …            (32 pairs)                                            │
└──────────────────────────────────────────────────────────────────────┘
```

**How it fits together (left → right):**

- **PTF** drives SAI calls over **Thrift (:9092)** and does packet I/O on the `OEth*_peer` ends.
- **saiserver** is a thin **Thrift → SAI** shim linked against the backend under test, `**libsaivs.so`**.
- **VPP** is the actual dataplane; `OEthernetX` are the VPP-facing veth ends.
- A test **programs** routes/neighbors/LAGs via SAI, then **verifies the dataplane** by sending a packet in one port and asserting it comes out the expected port(s).
- One design subtlety: the harness **auto-groups tests by their required common config** and restarts the backend per group — that's what lets us run the whole suite in one go.

### Bring-up order

**Typical OCP** `sai_test` **flow**: PTF calls SAI to configure ports → checks each port is `oper_status` UP → creates host interfaces → builds VLANs, routes, neighbors, etc.

**What our harness adds before that:** `run_test.sh` pre-creates veth pairs, starts Redis/VPP/saiserver, brings kernel veth ends up, and runs a watchdog that keeps VPP `host-OEthernetX` admin-up while SAI creates hostifs — required because VPP AF_PACKET needs real netdevs and hostifs start admin-down.

**Side effect:** OCP still checks `oper_status` before hostifs exist; on VPP that status stays DOWN until hostifs are created, so you see `port N is not up, status: 2. Retry. Reset Admin State.` for every port. The framework continues anyway, hostifs are created next, and tests pass — the messages are expected noise, not a failure.

---

## 3) Demo #1: Build & run

### **Build the image**

From the `sonic-sairedis` repo root (Cisco proxy required for `apt-get` / `pip` during the build):

```bash
cd <sonic-buildimage>/src/sonic-sairedis

docker build --no-cache \
  --build-arg http_proxy=http://sonic-build-rtp.cisco.com:3128/ \
  --build-arg https_proxy=http://sonic-build-rtp.cisco.com:3128/ \
  --build-arg no_proxy=.cisco.com,.webex.com,localhost,127.0.0.1,10.0.0.0/8,172.16.0.0/12,192.168.0.0/16 \
  -f .azure-pipelines/docker-sai-test-vpp/Dockerfile \
  -t docker-sai-test-vpp:phase1 .
```

**What goes into the image:** the Dockerfile build context is the `sonic-sairedis` repo root (`.`). It pulls together three layers:

1. **Pre-built `.deb`s** staged in `docker-sai-test-vpp/debs/` (VPP, libsaivs, libsairedis, saiserver, saithrift, …) — installed with `apt-get`.
2. **The harness module** (`docker-sai-test-vpp/`) — copied from source at build time: `run_test.sh` (entrypoint), `sai.profile`, port maps (`lanemap.ini`, `port-map.ini`, `ptf-port-map.ini`). These are *our* orchestration code, not pre-built packages.
3. **The OCP test suite** — `SAI/test/sai_test/` and `SAI/test/ptf/` copied from the same repo and baked into `/sai_test` and the PTF install.

**Rebuild scope:** editing harness files (`run_test.sh`, configs) or the test suite → image rebuild only (fast). Editing the C++ backend (`vslib/vpp/`) → rebuild the `.deb`s first, stage them into `debs/`, then rebuild the image.

### **Run tests**

**Where the tests come from:** the OCP `sai_test` PTF suite lives in the `sonic-sairedis` repo at `SAI/test/sai_test/` (the `SAI` submodule). The Dockerfile copies it into the image at `**/sai_test`** at build time; `run_test.sh` discovers test classes there automatically. PTF and the SAI Thrift client come from `SAI/test/ptf` and the `python-saithrift` package baked into the same image. The container entrypoint `run_test.sh` brings up Redis → veth topology → VPP → saiserver → PTF. One `docker run` = one orchestrated test pass (or many, if you pass multiple selectors).

**Selectors:** pass one or more targets after the image name. Each target is either a **module** (runs every class in that file) or `**module.Class`** (runs one class). `PORT_COUNT=32` is the standard T0 topology (required for most tests).


| Module              | Example Class                      | Coverage                                                         |
| ------------------- | ---------------------------------- | ---------------------------------------------------------------- |
| `sai_sanity_test`   | `SaiSanityTest`                    | T0 bring-up smoke test — verifies basic config and packet flood  |
| `sai_route_test`    | `DropRouteTest`                    | L3 routes: RIF, LAG, SVI, default/drop/remove, LPM, MAC learning |
| `sai_neighbor_test` | `AddHostRouteTest`                 | Neighbor adjacencies, host routes, add/remove, prefix changes    |
| `sai_rif_test`      | `IngressDisableTestV4`             | Router interface ingress admin-state, MTU, MAC update            |
| `sai_ecmp_test`     | `EcmpLagDisableTestV4`             | ECMP hash fields, nexthop groups, LAG+ECMP combinations          |
| `sai_lag_test`      | `LagConfigTest`                    | LAG config, load-balance hashing, member add/remove, disable     |
| `sai_port_test`     | `PortAutoNegTest`                  | Front-panel port attributes (auto-negotiation)                   |
| `sai_vlan_test`     | `Vlan_Domain_Forwarding_Test`      | VLAN forwarding, tagging, flooding, MAC learning                 |
| `sai_fdb_test`      | `L2PortForwardingTest`             | FDB learning, aging, flush, MAC move                             |
| `sai_tunnel_test`   | `BasicIPInIPTunnelEncapv4Inv4Test` | IP-in-IP tunnel encap/decap, TTL, ECN, peer mode                 |


### **Usage**

**One class within a module**:

```bash
docker run --rm --privileged -e PORT_COUNT=32 \
  docker-sai-test-vpp:phase1 <module.Class>
```

**The Entire Module**:

```bash
docker run --rm --privileged -e PORT_COUNT=32 \
  docker-sai-test-vpp:phase1 <module>
```

**Multiple classes in one container**, list as separate arguments; the harness auto-groups by config signature and restarts the backend per group. For example:

```bash
docker run --rm --privileged -e PORT_COUNT=32 \
  docker-sai-test-vpp:phase1 \
  sai_route_test.DefaultRouteV4Test \
  sai_route_test.DropRouteTest \
  sai_route_test.RemoveRouteV4Test
```

**Where results are stored:** PTF writes one JUnit-XML file per test to `/test-results` inside the container (default). Without a bind-mount those files are discarded when the container exits. To keep them on the host:

```bash
cd <sonic-buildimage>/src/sonic-sairedis
mkdir -p .azure-pipelines/docker-sai-test-vpp/results/xml
docker run --rm --privileged -e PORT_COUNT=32 \
  -v "$PWD/.azure-pipelines/docker-sai-test-vpp/results/xml:/test-results" \
  docker-sai-test-vpp:phase1 <module.Class>
```

JUnit XML lands in `.azure-pipelines/docker-sai-test-vpp/results/xml/TEST-*.xml`. Build the compatibility matrix with `python3 .azure-pipelines/docker-sai-test-vpp/gen_compatibility_matrix.py` → `.azure-pipelines/docker-sai-test-vpp/results/compatibility-matrix.md`.

---

## 4) Demo #2: What's inside the container

**Start it in debug mode.** `--debug` holds the container open after the test finishes — VPP, saiserver, Redis, and the veths stay up for inspection until you remove the container:

```bash
docker rm -f <instanceName> 2>/dev/null
docker run -d --name <instanceName> --privileged -e PORT_COUNT=32 \
  docker-sai-test-vpp:phase1 --debug sai_route_test.DefaultRouteV4Test
```

**Show the moving parts** (wait for the test to finish, then inspect):

```bash
# the runtime processes: VPP + saiserver stay up (PTF exits once the test completes)
docker exec <instanceName> ps -ef | grep -E 'vpp|saiserver|ptf'

# the OCP sai_test suite baked into the image, we're using upstream sai_test Python modules
docker exec <instanceName> ls /sai_test

# the 32 veth pairs, OEthernetX (VPP/SAI side) and OEthX_peer (PTF packet I/O side)
docker exec <instanceName> ip -br link | grep OEth

# clean up when done
docker rm -f <instanceName>
```

---

## 5) Demo #3: Run passing tests + peek at VPP state

**Pause-and-inspect with `vppctl`** (the debug container from Demo #2 is still up). This is exactly how we debug a failure:

```bash
# interfaces VPP created from the SAI config
docker exec <instanceName> vppctl show interface
docker exec <instanceName> vppctl show interface addr

# the routes the test programmed via SAI → did they land in the VPP FIB?
docker exec <instanceName> vppctl show ip fib
docker exec <instanceName> vppctl show ip neighbors

# LAG / bond state (for the ECMP-over-LAG tests)
docker exec <instanceName> vppctl show bond

# clean up when done
docker stop <instanceName>
docker rm -f <instanceName>
```

---

## 6) Demo #4: Where we are (compatibility matrix)

Open the latest matrix:

```bash
cd <sonic-buildimage>/src/sonic-sairedis
$EDITOR .azure-pipelines/docker-sai-test-vpp/results/compatibility-matrix.md     # or render the Markdown preview
```

- Generated automatically from the JUnit XML of a full suite run (`gen_compatibility_matrix.py`): **timestamp**, a **summary table** (PASS/FAIL/ERROR/SKIP with %), and a **legend** (result icons + SAI status codes).
- **Today: 11 PASS / large remainder FAIL or ERROR** — so the framework works end-to-end, but the backend is **early**.
- The dominant real gap is the **L3-over-LAG family**: neighbor/route/ECMP on LAG-backed router interfaces are rejected (`-5` INVALID_PARAMETER) or not forwarded to LAG members. That's the next investigation.
- We've already separated **genuine VPP/SAI gaps** from **test-framework artifacts**, so the matrix reflects real backend status.

The framework is solid and repeatable; the backend coverage is the work ahead — the matrix is how we measure it climbing.