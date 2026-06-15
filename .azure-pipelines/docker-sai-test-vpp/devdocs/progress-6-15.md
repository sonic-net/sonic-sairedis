# VPP SAI UT — Phase 2 (L3) Batch Run Findings & Reset Issue

Date: June 15, 2026

This document records the findings from the first Phase 2 (L3 validation) batch run
of `sai_route_test` against the VPP SAI backend (`docker-sai-test-vpp:phase1`,
`PORT_COUNT=32`), the root-cause analysis of the "one test per container" behavior,
and the proposed fixes. It follows the runbook style of `debug-6-12.md` but is
investigation-focused.

---

## 1. Summary of the batch run

Command:
```bash
docker run --rm --privileged -e PORT_COUNT=32 docker-sai-test-vpp:phase1 sai_route_test
```

Observed result (run was stopped early by the operator because every test after the
first failed identically and each pays a ~67–82s timeout):

| # | Test (class)                         | Outcome | Stage that failed |
|---|--------------------------------------|---------|-------------------|
| 1 | `RouteRifTest`                       | **FAIL** (failures=1) | runTest — data-plane verify |
| 2 | `RouteRifv6Test`                     | **ERROR** | setUp — `Create Host intfs...` |
| 3 | `LagMultipleRouteTest`               | **ERROR** | setUp — `Create Host intfs...` |
| 4 | `LagMultipleRoutev6Test`             | **ERROR** | setUp — `Create Host intfs...` |
| 5 | `DropRouteTest`                      | **ERROR** | setUp — `Create Host intfs...` |
| … | (all remaining classes)              | **ERROR** | setUp — `Create Host intfs...` |

Two **distinct** failure classes appear and must not be conflated:

- **(A) Internal data-plane FAIL** — only the *first* test (`RouteRifTest`). Full T0
  config completed (`common config done`), the test body ran, and the routed packet
  was not received on the expected egress ports.
- **(B) Cross-test setUp ERROR** — *every* subsequent test. The shared
  saiserver/VPP backend is already "used up" by test #1, so test #2's configuration
  crashes saiserver and all later tests inherit a dead RPC endpoint.

---

## 2. Issue B — "one test per container" (the reset issue) — ROOT CAUSE FOUND

### 2.1 Hypothesis test (operator-requested)

Ran the second test **in isolation** in a fresh container:
```bash
docker run --rm --privileged -e PORT_COUNT=32 docker-sai-test-vpp:phase1 sai_route_test.RouteRifv6Test
```
Result:
```
  sai_route_test.RouteRifv6Test
Create Host intfs...            <-- succeeded, NO "Cannot create hostif" errors
common config done              <-- full T0 setup completed
AssertionError: Did not receive expected packet on any of ports [19, 20] for device 0.
Ran 1 test in 81.727s
FAILED (failures=1)
```

**Conclusion: hypothesis CONFIRMED.** `RouteRifv6Test`, which *errored* in setUp as
the 2nd test of the batch, *configures cleanly and runs* when it is the first/only
test in a fresh container. The container/back-end can service exactly **one** test's
full configuration cycle; the failure is a between-tests reset problem, not a bug in
any individual test's config.

### 2.2 Failure signature of the 2nd+ test (from the batch log)

```
Create Host intfs...
Cannot create hostif, error : required argument is not an integer   <-- 1st hostif create returned a null OID
Cannot create hostif, error : TSocket read 0 bytes                  <-- saiserver closed the socket (process died)
Cannot create hostif, error : TSocket read 0 bytes
Cannot create hostif, error : unexpected exception                  <-- x29 (saiserver gone)
...
Get default 1Q bridge...
ERROR ... BrokenPipeError: [Errno 32] Broken pipe                   <-- next RPC on dead socket
   -> thrift.transport.TTransport.TTransportException
```

Interpretation:
- `required argument is not an integer` = a SAI create returned `None` (null OID) — the
  recurring "null OID serialized" signature (see `progress-6-12.md` gotchas).
- `TSocket read 0 bytes` = the saiserver process closed/died mid-request (EOF on the
  Thrift socket). After this, every subsequent Thrift call raises `BrokenPipe`.
- So **saiserver crashes during the 2nd test's host-interface creation**, then all
  remaining tests fail at the first RPC in their setUp (`get_default_1q_bridge`).

### 2.3 Why it happens (design root cause)

The SAI PTF T0 framework is designed to configure the switch **once** and reuse it:

- `sai_test_base.py::setUp()` runs the entire common config
  (`create_device` → `t0_switch_config_helper` → ports → **host intfs** → vlan → fdb →
  lag → routes → neighbors) **only when** `force_config or not self.common_configured`.
- `self.common_configured` is read from the PTF test-param `common_configured`
  (`sai_test_base.py` ~line 233): `true` → skip full config and instead
  `self.dut = self.persist_helper.read_dut()` (reuse the previously persisted object
  IDs from Redis); absent/`false` → re-run the **full** config from scratch.

In the standard sonic-mgmt workflow the runner invokes PTF once with
`common_configured=false` to build + persist the config, then invokes PTF again for
every subsequent test with `common_configured=true` so they **reuse** the already
configured switch.

Our harness (`run_test.sh::build_ptf_args`) runs **all classes in a single `ptf`
invocation** with:
```
--test-params "thrift_server='127.0.0.1';port_map_file='<map>'"   # no common_configured
--relax
```
Because `common_configured` is never set, **every** test re-runs the complete config —
including a fresh `sai_create_switch` and a fresh `sai_create_hostif` for all 32 ports —
inside the **same long-lived saiserver + VPP process**, while the previous test's
linux-cp tap interfaces (`Ethernet0..31`) and VPP host-interfaces (`host-OEthernet*`)
still exist. The duplicate host-interface creation returns a null OID and crashes
saiserver. (`create_device()` itself is harmless — it only builds Python `Device`
objects — the SAI calls are in the config helpers and the `ThriftInterfaceDataPlane`
switch bring-up that run per test instance.)

Net: **the VPP SAI backend does not support a second full create-switch /
create-hostif cycle in one process, and the harness never engages the
config-reuse (`common_configured=true`) path, so only the first test can run.**

### 2.4 Proposed fixes (Issue B)

Three options, best first:

1. **Config-reuse (production-faithful, fast).** Mirror sonic-mgmt: run the common
   config once, persist to Redis, then run each test with `common_configured=true`.
   - Concretely: a first "config" PTF pass (or the first test) with
     `common_configured=false`, then run the remaining classes with
     `--test-params "...;common_configured=true"`.
   - Requires the persisted object IDs to remain valid for the life of the saiserver
     process (they do — same process, Redis-backed `PersistHelper`) and that no test's
     `tearDown` tears down the switch. This avoids ever re-creating the switch/hostifs.
   - Lowest runtime cost (config paid once). This is the intended framework design.

2. **One container per test class (simple isolation).** Change the runner to enumerate
   test classes and launch a fresh container per class, so each gets a clean
   saiserver/VPP and a single config cycle.
   - Matches the observed "works when run alone" behavior.
   - Simple and robust, but slow: each class pays the full VPP+saiserver+config
     startup (~30–80s). For `sai_route_test` alone that is ~20 classes.
   - Note the HLD §4.5 CI loop runs one **file** per container; that is insufficient
     because a file contains many classes. The loop must be per **class**.

3. **Fix the backend to support switch/hostif re-creation (hardest).** Make the VPP
   SAI backend cleanly destroy and re-create the switch + host interfaces within one
   process (proper `remove_hostif`/`remove_switch` semantics + idempotent linux-cp tap
   handling). Largest effort; tracked as future work, not required for Phase 2.

**Recommendation for Phase 2:** use option 1 (config-reuse) as the harness change so
the compatibility-matrix runs are fast and faithful; fall back to option 2 if a test's
`tearDown` mutates shared config in a way that breaks reuse.

---

## 3. Issue A — `RouteRifTest` internal FAIL (real VPP SAI L3 gap)

This is the actual kind of finding Phase 2 exists to surface.

### 3.1 What the test does
`RouteRifTest` (and its v6 twin) configures a route for `192.168.12.0/24`
(`fc02::12:0/112` for v6) whose next hop is a **RIF over LAG2**, sends a routable TCP
packet ingress, and expects the **routed** packet to egress on one of LAG2's member
ports `[19, 20]`.

### 3.2 Expected vs. received
```
EXPECTED (routed):  eth.dst=00:01:01:01:02:64 (next-hop neighbor MAC)
                    eth.src=00:77:66:55:44:00 (RIF/switch MAC)
                    ip.ttl=63                 (decremented)
                    on one of ports [19, 20]  (LAG2 members)

RECEIVED:           eth.dst=00:77:66:55:44:00 ip.ttl=64 ...  (un-routed / not rewritten)
                    AssertionError: Did not receive expected packet on any of ports [19, 20]
```
The routed packet never appears on the LAG egress ports: no MAC rewrite to the
neighbor, no TTL decrement, no delivery to a LAG member. The frames seen on the
dataplane are not the routed result.

### 3.3 Likely area
L3 forwarding **to a next hop that resolves over a LAG** in the VPP SAI backend —
i.e., route → nexthop → neighbor → RIF-over-LAG → LAG member egress. Candidates to
investigate (vslib/vpp):
- LAG (BondEthernet) member → SAI port mapping and whether routed traffic is hashed
  onto a live member.
- Whether the neighbor/nexthop adjacency is programmed into the VPP FIB for the
  LAG-backed RIF (adjacency rewrite present? TTL-decrement node hit?).
- Whether the LAG member host-interfaces are admin/oper-up at test time (the Phase 1
  veth-up watchdog brings up `host-OEthernet*`; confirm BondEthernet members forward).

This needs a dedicated debug pass (vppctl `show ip fib`, `show adj`, `show bond`,
`show interface`) during the runTest window and is tracked as a Phase 2 work item
separate from Issue B.

> Note: a separate benign VPP log line appears at teardown on this host:
> `buffer: numa[1] falling back to non-hugepage backed buffer pool (... Cannot
> allocate memory)`. This is a hugepage-availability warning on the build host, not a
> test failure.

---

## 4. How to reproduce / inspect

Single class in a fresh container (clean, one config cycle — this is the reliable way
to exercise an individual L3 test today):
```bash
docker rm -f officesai-debug 2>/dev/null
docker run --rm --privileged -e PORT_COUNT=32 docker-sai-test-vpp:phase1 sai_route_test.RouteRifTest
```

Debug mode (leave VPP/saiserver/veths alive for `vppctl` during the run window):
```bash
docker rm -f officesai-debug 2>/dev/null
docker run -d --name officesai-debug --privileged -e PORT_COUNT=32 \
    docker-sai-test-vpp:phase1 --debug sai_route_test.RouteRifTest
# during runTest:
docker exec officesai-debug vppctl show ip fib
docker exec officesai-debug vppctl show adj
docker exec officesai-debug vppctl show bond
docker exec officesai-debug vppctl show interface
docker cp officesai-debug:/var/log/saiserver.log ./saiserver_route.log
```

Confirm the saiserver crash on a 2nd config cycle (batch of two classes):
```bash
docker run --rm --privileged -e PORT_COUNT=32 docker-sai-test-vpp:phase1 sai_route_test \
  2>&1 | grep -nE "Create Host intfs|Cannot create hostif|TSocket read 0 bytes|BrokenPipe|^(OK|FAILED|ERROR)"
```

---

## 5. Status / next steps

- [x] Confirmed: container services exactly one full config cycle; 2nd test crashes
      saiserver at host-interface creation. Root cause = harness never uses the
      `common_configured=true` config-reuse path, so every test re-creates the
      switch/hostifs in one process.
- [x] **Issue B fix (harness):** implemented config-reuse in `run_test.sh` (each test
      target runs as its own ptf invocation; first builds+persists config, rest reuse
      via `common_configured=true`). Validated with two sequential tests.
- [ ] **Issue A (VPP SAI):** debug L3 route → LAG-backed next hop forwarding for
      `RouteRifTest`/`RouteRifv6Test`; capture FIB/adjacency/bond state.
- [x] Re-ran the full Phase 2 set and produced the compatibility matrix
      (now generated at `docker-sai-test-vpp/results/compatibility-matrix.md` by
      `gen_compatibility_matrix.py`; this run was originally under `phase2-results/`).
      Triage in section 6 below.

---

## 6. Phase 2 batch run — UT-framework artifacts vs. real SAI/VPP gaps

The first full Phase 2 batch (matrix now at `docker-sai-test-vpp/results/compatibility-matrix.md`, 87 classes →
95 rows: 10 PASS, 50 FAIL, 33 ERROR, 2 SKIP) is **partially contaminated by harness /
test-framework artifacts**. Some failures are NOT VPP SAI gaps. This section records the
UT-related issues so the matrix can be interpreted correctly. Classifications below are
backed by **standalone re-runs** (each test in its own fresh container with
`common_configured=false`, so it builds its own correct config) — that is the
ground-truth way to separate harness noise from real backend gaps.

### 6a. [UT — config-reuse limitation] ECMP "list index out of range" (~17 ERRORs)
- Symptom (batch): every `sai_ecmp_test.*` class errors with `IndexError: list index out
  of range` at e.g. `self.dut.nhp_grpv4_list[0].member_port_indexs`.
- ROOT CAUSE: the ECMP tests override the common config via their `setUp`:
  `T0TestBase.setUp(self, is_create_route_for_nhopgrp=True, is_create_route_for_lag=False)`.
  They need next-hop **groups** built. But the config-reuse harness only runs the full
  config builder for the FIRST test in the batch (here `RouteRifTest`, which uses the
  DEFAULT params and does NOT create nexthop groups). Every later class loads the
  persisted DUT (`read_dut()`) whose `nhp_grpv4_list`/`nhp_grpv6_list` is empty →
  index `[0]` throws.
- EVIDENCE (standalone): `docker run ... sai_ecmp_test.EcmpHashFieldSportTestV4` alone →
  `common_configured is: False`, NO IndexError; the test actually runs and yields a real
  result (`FAILED: expected packet received on an unexpected port: 7`, a genuine ECMP
  hashing observation).
- CLASSIFICATION: **UT-framework / config-reuse**. NOT a SAI/VPP gap. The `IndexError`
  rows must be discarded; the real ECMP results require standalone (or a per-config-group
  batching) run.
- IMPLICATION: config-reuse is only valid for tests that share the SAME common-config
  parameters as the batch's first test. Tests that override `setUp` config flags
  (`is_create_route_for_nhopgrp`, `is_create_route_for_lag`, tunnel flags, etc.) cannot
  reuse a default-config switch.
- RESOLUTION: implemented config-signature grouping + per-group backend restart in
  `run_test.sh` (section 7). After the fix the ECMP IndexError artifacts are GONE — the
  ECMP tests run in their own `is_create_route_for_nhopgrp=True` group and produce real
  results. NOTE: the signature detector must be **inheritance-aware** — the 6 ECMP
  `*TwoLayersWithDiffHashOffset*` classes subclass `EcmpBaseTestV4/V6` (which set the
  nhopgrp flags) and their own `setUp` just calls `super().setUp()` with no kwargs. A
  shallow (own-setUp-only) signature scan mis-grouped them into the default group and
  they STILL threw `list index out of range`. Fixed by resolving the signature through
  the class inheritance chain (section 7.5).

### 6b. [UT — upstream test-code bug] NameError `status` (NoHostRouteTest)
- Symptom: `sai_neighbor_test.NoHostRouteTest` errors `name 'status' is not defined`.
- ROOT CAUSE: genuine Python bug in the upstream test:
  `SAI/test/sai_test/sai_neighbor_test.py` setUp does
  `T0TestBase.setUp(self)` then `self.assertEqual(status, SAI_STATUS_SUCCESS)` — `status`
  is never assigned (line ~15). Reproduces standalone.
- CLASSIFICATION: **UT / test-code bug** (pre-existing in the OCP SAI repo; not our change,
  not a VPP gap). Should be reported/fixed upstream; for the matrix, treat as "test bug".

### 6c. [UT — cascade artifact] tearDown AttributeError / NoneType (the duplicate rows)
- Symptom: several classes appear TWICE in the matrix — once `FAIL` (the real runTest
  result) and once `ERROR` from `tearDown`. Examples:
  - `'…object has no attribute 'port1_route''` (RouteLPMRoute* tearDown)
  - `'…object has no attribute 'mtu_Vlan10_rif'' / 'mtu_port10_rif'` (IngressMtuTest tearDown)
  - `'…object has no attribute 'port4_nbr_v4''` (SviMacFlooding tearDown)
  - `'NoneType' object is not subscriptable` (IngressMacUpdateTest tearDown)
- ROOT CAUSE: the test body raised before assigning an attribute (e.g. a create returned
  `-5` and `assertEqual` aborted setUp/runTest), then `tearDown` unconditionally
  references that attribute / removes that object → secondary AttributeError. EVIDENCE:
  standalone `RouteLPMRouteNexthopTest` shows BOTH `AssertionError: -5 != 0` (primary) and
  `AttributeError: 'RouteLPMRouteNexthopTest' object has no attribute 'port1_route'`
  (tearDown) together — so the ERROR is a dependent artifact of the FAIL, not an
  independent root cause.
- CLASSIFICATION: **UT / cascade artifact** (test-code teardown not guarded). The ERROR
  row should be collapsed into its paired FAIL; the real signal is the primary FAIL.

### 6d. [SAI/VPP gap — CONFIRMED standalone] L3-over-LAG: neighbor/route create `-5`
- Symptom: `AssertionError: -5 != 0` (SAI_STATUS_INVALID_PARAMETER) on create.
- Affected: `sai_neighbor_test.AddHostRouteTest/V6`, `RemoveAddNeighborTestIPV4/V6`,
  `NhopDiffPrefixRemove*`; `sai_route_test.RouteLPMRoute*`,
  `RouteDiffPrefixAddThenDelete*`.
- ROOT CAUSE (confirmed): the failing call is `sai_thrift_create_neighbor_entry(...
  rif_id=self.dut.lag_list[0].rif_list[0] ...)` (neighbor on a **LAG-backed** router
  interface) — `sai_neighbor_test.py:125` — and analogous route-entry creates whose
  next hop resolves over a LAG. Returns `-5` in the VPP backend. Reproduces standalone
  (`AddHostRouteTest` alone → `-5 != 0`), so it is NOT a config-reuse artifact.
- CLASSIFICATION: **SAI/VPP implementation gap** — programming a neighbor/adjacency on a
  LAG (BondEthernet) RIF is rejected by the VPP SAI backend. Same root family as Issue A
  (L3 traffic to a LAG next hop not forwarded). This is a real Phase 2 finding.

### 6e. [SAI/VPP gap — CONFIRMED] L3 routed traffic to LAG not received ([17,18]/[19,20])
- Symptom: `Did not receive expected packet on any of ports [17,18]` (LAG1) or
  `[19,20]` (LAG2). Affected: many `sai_route_test.*` and `sai_rif_test.*`.
- ROOT CAUSE: SAI control-plane config succeeds (route/nbr/RIF return 0) but VPP does not
  forward the routed packet out the LAG member ports. Same L3-over-LAG family as 6d/Issue
  A. Confirmed real (RouteRifTest fails identically standalone).
- CLASSIFICATION: **SAI/VPP implementation gap** (data-plane). Real Phase 2 finding.

### 6f. [SAI/VPP gap] other status-code FAILs (now uncontaminated)
SAI status codes (from `SAI/inc/saistatus.h`): `-1`=FAILURE, `-2`=NOT_SUPPORTED,
`-5`=INVALID_PARAMETER, `-6`=ITEM_ALREADY_EXISTS, `-7`=ITEM_NOT_FOUND.
- `-7 != 0` (ITEM_NOT_FOUND) — `sai_ecmp_test.ReAddLagEcmpTest*`,
  `RemoveAllNextHopMemeberTestV4`, `RemoveNexthopGroupTestV4`: remove/re-add of
  next-hop-group members / LAG members not found by the VPP backend. **SAI/VPP gap.**
- `-5 != 0` — also `sai_ecmp_test.EcmpCoExistLagRouteV4/V6`, `EcmpReuseLagRouteV4/V6`
  (these build their OWN nexthop group in setUp, default group, so not contaminated):
  ECMP-over-LAG nexthop programming rejected. **SAI/VPP gap** (same L3-over-LAG family).
- `-1 != 0` — `EcmpIngressDisableTestV4/V6`: generic FAILURE disabling ingress on a RIF.
  **SAI/VPP gap** (likely).
- `-6 != 0` — `SviMacFloodingTest/v6`: `create_neighbor_by_rif` on a VLAN-10 SVI RIF
  returns ITEM_ALREADY_EXISTS. Could be a genuine SVI-neighbor duplicate-handling gap OR a
  test that assumes a clean neighbor namespace; the SVI neighbor is built by THIS test's
  setUp (not the common config) and runs in the default group, so it is NOT a config-reuse
  artifact. **SAI/VPP gap** (SVI neighbor idempotency).
- `SviDirectBroadcastTest` (VLAN20 broadcast flood not received), `SviRouteL3Test`
  (routed pkt not on port 1): data-plane forwarding gaps. **SAI/VPP gap.**
- ECMP hash FAILs ("received on an unexpected port: N"): genuine VPP ECMP/LAG
  distribution behavior, now visible because the IndexError contamination is gone.
  **SAI/VPP gap / behavioral** (the VPP hash spreads differently than the test expects).

### 6g. Matrix-hygiene takeaways (UT)
- The compatibility matrix MUST be regenerated from **standalone** (or
  per-config-group) runs to be trustworthy: the single-batch default-config reuse
  mislabels every test that overrides its common-config params (all ECMP, and any
  tunnel/nhopgrp test) and adds tearDown cascade ERROR rows.
- Action items (harness, not backend):
  - [x] Group Phase 2 tests by required common-config params; run one reuse-batch per
        group. **DONE** — implemented in `run_test.sh` (see section 7).
  - [ ] Collapse tearDown ERROR rows that pair with a runTest FAIL in the matrix
        generator (report the primary FAIL only).
  - [ ] Report the upstream NameError bug in `sai_neighbor_test.NoHostRouteTest`.

---

## 7. Issue C — config-reuse only valid within one common-config group (RESOLVED)

### 7.1 The problem (root cause)
Issue B's first fix ran every test as its own ptf invocation against ONE long-lived
saiserver, with the FIRST test building the config and the rest reusing it
(`common_configured=true`). That is correct ONLY if every test wants the SAME common
config. It does not: a test's `setUp` may pass different kwargs to `T0TestBase.setUp`
that change what the common config builds. Concretely, AST-scanning the Phase 2 suite
shows just two real config "signatures":

- **64 tests** — signature `()` (default config; what `RouteRifTest` builds).
- **21 tests** — signature `is_create_route_for_nhopgrp=True, is_create_route_for_lag=False`
  (all `sai_ecmp_test.*`; they need next-hop GROUPS built).
- (2 `SviMacrMoveStress*` only pass `skip_reason`, which is config-irrelevant.)

Because the batch's first test (`RouteRifTest`) used the default signature, the ECMP
tests reused a config WITHOUT nexthop groups → `self.dut.nhp_grpv4_list[0]` →
`IndexError: list index out of range` (the section 6a artifact). So config-reuse across
differing signatures produced wrong results.

### 7.2 The fix (config-signature grouping + per-group backend restart)
Implemented entirely in `run_test.sh` (harness only; no backend or test changes):

- **`plan_test_groups`** — a Python planner that AST-parses each test class's `setUp`,
  extracts the kwargs it passes to `T0TestBase.setUp` (ignoring config-irrelevant
  `skip_reason`/`wait_sec`), and emits a run plan of
  `"<group_id>\t<module.Class>\t<signature>"` lines, grouping identical-signature tests
  together (first-seen order preserved). With no targets it plans EVERY discovered
  `sai_*_test.py` class.
- **`start_backend` / `stop_backend`** — the Redis+VPP+saiserver+veth-up-watchdog
  lifecycle factored into restartable functions. `stop_backend` also wipes
  `/tmp/sai_model` (persisted OIDs) and the link-up marker so the next group rebuilds
  cleanly.
- **`run_ptf` loop** — for each config group: `stop_backend` (if not first) →
  `start_backend` (fresh saiserver) → the group's FIRST test builds+persists that
  group's config (`common_configured=false`), the REST reuse it
  (`common_configured=true`).

The per-group **backend restart** is what makes the second (and Nth) config build safe:
building the switch/hostifs twice in ONE saiserver process is exactly the Issue B crash;
a fresh saiserver per group avoids it. The pre-created veth/PortChannel netdevs persist
across restarts — only the dataplane daemons recycle (~30s per group).

### 7.3 Validation (one `docker run`, two config groups)
```
docker run ... docker-sai-test-vpp:phase1 \
    sai_route_test.RouteRifTest sai_ecmp_test.EcmpHashFieldSportTestV4 sai_route_test.DropRouteTest
```
produced:
```
Planned 3 test target(s) across 2 config group(s)
### Config group 0 (signature: ()) ###
  [1/3] RouteRifTest    (common_configured=false)  built config; FAILED (Issue A, real)
  [2/3] DropRouteTest   (common_configured=true)   reused config; OK in 0.013s
### Config group 1 (signature: is_create_route_for_nhopgrp=True|is_create_route_for_lag=False) ###
  [3/3] EcmpHashFieldSportTestV4 (common_configured=false) REBUILT config (fresh saiserver);
        Create Host intfs... (no crash); FAILED "unexpected port: 7"  <-- NO IndexError
```
Confirms: (a) auto-grouping into 2 groups; (b) backend restart with no
`Cannot create hostif`/`TSocket read 0`/`BrokenPipe` (Issue B avoided); (c) the ECMP
`list index out of range` artifact is GONE — the ECMP test built its nexthop groups and
ran to a real ECMP result, matching its standalone run; (d) intra-group reuse still
works (`DropRouteTest` reused in 13 ms).

### 7.4 Result
A single `docker run` (no args, or any list of `module`/`module.Class` targets) now
plans → groups by config signature → runs every group against a fresh backend with
intra-group reuse → collects JUnit XML. The whole suite runs in one go and each test
gets the common config it actually requested, so the matrix is no longer contaminated by
config-reuse (section 6a) artifacts.

- Env knobs: `COMMON_CONFIGURED_REUSE=0` restores the legacy single-invocation behavior;
  `KEEP_VETHS_UP_SECONDS` bounds the per-group veth-up watchdog.
- Remaining (non-blocking) matrix-hygiene item: collapse tearDown cascade ERROR rows into
  their paired FAIL in the matrix GENERATOR (section 6c) — cosmetic reporting only.

### 7.5 Follow-up fix — inheritance-aware signature detection
The first cut of `plan_test_groups` only inspected a class's OWN `setUp` for config
kwargs. That mis-grouped the 6 `sai_ecmp_test.*TwoLayersWithDiffHashOffset*` classes:
they subclass `EcmpBaseTestV4`/`EcmpBaseTestV6` (whose `setUp` sets
`is_create_route_for_nhopgrp=True`) and their own `setUp` is just `super().setUp()` with
no kwargs → shallow scan saw `()` → placed them in the DEFAULT group → they STILL threw
`list index out of range`.
- FIX: `plan_test_groups` now builds a cross-file class registry (name → bases + setUp)
  and resolves each class's signature by walking the inheritance chain: if a class's
  `setUp` only chains to `super().setUp()` / `<Base>.setUp(self)` without config kwargs,
  the signature is taken from that base (recursively, up to `T0TestBase` → default).
- VALIDATION: full 87-class Phase 2 run now reports **"Planned 87 test target(s) across 2
  config group(s)"** and **zero** `list index out of range` occurrences in the log. The
  6 TwoLayers classes correctly land in the `is_create_route_for_nhopgrp=True` group.
  (The 4 `EcmpCoExist*`/`EcmpReuse*` classes legitimately stay in the default group —
  they call `T0TestBase.setUp(self)` and build their OWN nexthop group inside `setUp`.)

---

## 8. Phase 2 matrix after both harness fixes (config-grouping + inheritance-aware)

Re-ran the full 87-class Phase 2 set in ONE `docker run` (2 config groups). Matrix:
`Total 95 rows (95 because 6 cascade tearDown ERRORs duplicate their FAIL): PASS=11,
FAIL=71, ERROR=11, SKIP=2`. **No `list index out of range` and no Issue-B saiserver
crash remain** — every ERROR row is now either a real upstream test bug or a tearDown
cascade, NOT a harness/config-reuse artifact.

### 8.1 Remaining ERRORs (11) — all UT (test-code), none harness, none SAI/VPP
| Error | Tests | Class |
|-------|-------|-------|
| `name 'status' is not defined` | `NoHostRouteTest` (1) | **UT — upstream test bug** (6b) |
| `'NoneType' object is not subscriptable` | `IngressMacUpdateTest/V6` (2) | **UT — tearDown cascade** (6c) |
| `no attribute 'mtu_Vlan10_rif'/'mtu_port10_rif'` | `IngressMtuTestV4/V6` (2) | **UT — tearDown cascade** (6c) |
| `no attribute 'port1_route'` | `RouteLPMRouteNexthop/Rif{,v6}` (4) | **UT — tearDown cascade** (6c) |
| `no attribute 'port4_nbr_v4'` | `SviMacFlooding{,v6}` (2) | **UT — tearDown cascade** (6c) |

Each cascade ERROR pairs with a real FAIL on the same class (the primary signal). They
should be collapsed in the matrix generator (non-blocking, cosmetic).

### 8.2 FAILs (71) — classification
| Symptom | Count | Classification | Notes |
|---------|------:|----------------|-------|
| `-5` neighbor/route create on LAG RIF | 21 | **SAI/VPP gap** (6d) | L3-over-LAG nbr/route programming rejected (INVALID_PARAMETER) |
| `Did not receive on [17,18]/[19,20]` | ~28 | **SAI/VPP gap** (6e) | routed traffic not forwarded out LAG members (data-plane) |
| ECMP `unexpected port: N` | ~9 | **SAI/VPP gap / behavioral** | real VPP ECMP/LAG hash distribution differs from expectation |
| `-7` nexthop-group member remove/re-add | 4 | **SAI/VPP gap** | ITEM_NOT_FOUND on NHG/LAG member ops |
| `-5` ECMP-over-LAG (CoExist/Reuse) | 4 | **SAI/VPP gap** | ECMP nexthop over LAG rejected |
| `-1` ingress disable | 2 | **SAI/VPP gap** (likely) | generic FAILURE disabling RIF ingress |
| `-6` SVI neighbor already exists | 2 | **SAI/VPP gap** | SVI neighbor idempotency (built by the test, default group) |
| `SviDirectBroadcast` / `SviRouteL3` | 3 | **SAI/VPP gap** | VLAN broadcast flood / SVI L3 forwarding |

### 8.3 Bottom line
- **Harness is clean:** after config-signature grouping (7.2) + inheritance-aware
  detection (7.5), NO failure/error in the matrix is caused by the UT harness or
  config-reuse. The whole suite runs in one `docker run`.
- **UT issues to report upstream (not VPP):** the `NoHostRouteTest` NameError (6b) and the
  unguarded `tearDown`s that produce cascade ERRORs (6c). These are pre-existing OCP SAI
  test-code bugs, independent of the VPP backend.
- **Real SAI/VPP gaps (the Phase 2 deliverable):** dominated by the **L3-over-LAG**
  family — neighbor/route/ECMP programming on LAG-backed RIFs returns `-5`/`-7`, and
  routed traffic is not forwarded out LAG members. Plus SVI broadcast/L3 and ingress
  disable. These are the genuine implementation gaps to feed back to the VPP SAI backend.


