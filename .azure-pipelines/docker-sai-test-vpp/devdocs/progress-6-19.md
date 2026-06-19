# VPP SAI UT — ECMP `-6`/`-7` config-reuse artifacts + port-up wait speedup

Date: June 19, 2026

This document covers two coupled pieces of work:

1. **Item 2b** — the ECMP `-6`/`-7` (ITEM_ALREADY_EXISTS / ITEM_NOT_FOUND) failures on the LAG-ECMP reuse/re-add tests, shown to be **test-harness config-reuse artifacts, not VPP SAI backend gaps**, and fixed in the OCP test layer.
2. A follow-on **port-up wait speedup** in the OCP port configer that cuts the per-config-build time from ~75s to ~13s (the port-down polling was ~80% of it), motivated by the analysis below of whether to tear down/bring up the backend between *every* test instead of per config group.
3. **Per-test isolation mode** (`ISOLATE_EACH_TEST`, now the harness default): because the port-up speedup made per-test backend recycle affordable, every test now runs in its own group with a fresh backend and its own freshly-built common config. This removed the *root cause* of the Item 2b artifacts, so the in-test config-reuse workarounds from (1) were **reverted** and the upstream OCP tests are pristine again (one genuine latent bug fix is kept — see §1.4 / §4).

(Item 2b was originally written up as §5c of `progress-6-18.md`; it has been moved here so all of the post-6-18-commit work lives in one place. `progress-6-18.md` is back to its committed state.)

---

## 1. Item 2b — ECMP `-6`/`-7` reuse/re-add: NOT a backend gap (FIXED in harness)

Follow-up investigation of `EcmpReuseLagRoute* (-6)`, `ReAddLagEcmpTest* (-7)`, `RemoveLagEcmpTest*`, `RemoveNexthopGroupTestV4 (-7)`.

### 1.1 Root cause — shared backend + persisted `dut` across a config-signature group
The harness runs tests grouped by `setUp` config signature, **one saiserver per group**, persisting the T0 `dut` once and reusing it (`common_configured=true`) for the rest of the group (see README "config-signature grouping"). This reuse exists because of a hard backend constraint: **the VPP SAI backend can only build the switch + host-interfaces once per saiserver process** — a second full config build inside the same saiserver re-runs `sai_create_switch` + 32×`sai_create_hostif` and crashes saiserver on the duplicate-create. So instead of rebuilding per test, the group's first test builds + persists the config and the rest reload the persisted `dut`.

That reuse is exactly what breaks these particular tests:

- The `-6`/`-7` codes are emitted by the **meta layer** (`meta_generic_validation_*` / `meta_sai_validate_oid: object key ... doesn't exist`), *before* control reaches the backend's `removeNexthopGroupMember`/`createNexthopGroupMember`. The idempotency code added to `vslib/vpp/SwitchVppNexthop.cpp` (6-18) is therefore never even reached — this is the proof it is not a backend bug. Each affected test **passes in isolation** (apart from the separate dataplane-forwarding gap).
- A test removes / re-adds / deletes NHG members in its `setUp`, body, or `tearDown`, mutating the in-memory `dut`, but the **persisted** `dut` on disk is not updated. The next test in the same group reloads the stale persisted `dut` and asks the backend to remove an OID that was already removed (`-7`) or to create one that still exists (`-6`). The backend/meta layer is correctly rejecting an operation on a stale OID.

### 1.2 Why this only affects *some* tests
Most tests treat the shared common config as **read-only** (send packets, check forwarding) — they never mutate NHG membership, so reuse is safe and they pass. Only the small family of tests that **mutate or destroy** the shared NHG state are affected: `RemoveLagEcmp*`, `ReAddLagEcmp*`, `RemoveAllNextHopMemeberTestV4`, `RemoveNexthopGroupTestV4`, and the inline `EcmpReuseLagRoute*` (which re-creates an NHG/route over the same nexthops a co-grouped predecessor leaked). The damage only manifests when such a mutating/destructive test shares a group with a later test that expects the original config — i.e. it is an **ordering-within-a-group** effect, which is why the same test can pass standalone and fail in the batched matrix.

### 1.3 Decision: fix the tests vs. tear down/bring up between every test
Two ways to remove the contamination:

- **(a) Per-test backend recycle** — give every test a fresh saiserver + freshly built common config, so no persisted state is ever shared. This is the "most correct" option and would let us leave the upstream OCP tests untouched. But it means running the full T0 config build **once per test** instead of once per group.
- **(b) Keep per-group reuse, make the mutating tests keep the persisted `dut` in sync / isolate them.** Smaller runtime cost, but requires touching the OCP test files.

We measured (a). The reconfigure cost between two groups (measured on the **second** group transition, to avoid first-group cold-boot bias) was **~95s**, of which the backend process recycle (Redis+VPP+saiserver) was only ~9s and the **T0 common-config build was ~75s**. Within that 75s, the dominant cost (~64–68s, i.e. ~80%) was the OCP framework's port-up polling (see §2). At ~85 tests, per-test recycle would add roughly 85 × ~95s ≈ **2.2+ hours** to a run that is currently ~12 minutes — not viable as the default.

So we chose **(b)** for the artifacts now, **and** separately attacked the 80% port-up cost (§2) so that per-test recycle becomes a feasible *option* in future.

### 1.4 Fixes — two phases
**Phase 1 (interim, per-group reuse retained):** three harness-side edits under `SAI/test/sai_test/` made the mutating tests keep the persisted `dut` consistent and split destructive tests into their own config-signature groups. These produced PASS 28→31 with no ECMP `-6`/`-7`. They were **reverted in Phase 2** (below) except for one genuine bug fix, since per-test isolation (§5) removes their reason to exist.

The retained fix (the one true latent bug, not a config-reuse workaround):
- `config/route_configer.py` — `create_nexthop_group_by_nexthops` gives the v4 and v6 NHGs **independent copies** of `member_port_indexs` (`list(member_port_indexs)`). They were constructed sharing one Python list object, so a v4 member remove/re-add mutated the v6 group's port list too → `ValueError: list.remove(x): x not in list` (the `ReAddLagEcmpTestV6` ERROR). This is a real aliasing defect independent of how the harness groups tests — a single test doing v4 then v6 member ops could hit it — so it is kept and is worth upstreaming.

Reverted Phase-1 workarounds (no longer needed under per-test isolation):
- `route_configer.py` `persist_helper.persist_dut(...)` re-persist calls in `remove_nhop_member_by_lag_idx` / `create_nhop_member_by_lag_port_idxs`.
- `sai_ecmp_test.py` distinct-config-signature kwargs and the `RemoveAllNextHopMemeberTestV4` persisted-member-clear. `sai_ecmp_test.py` is now **pristine upstream**.

> `force_config=True` was tried and rejected for Phase 1: it re-runs config creation against the already-initialized saiserver (the once-per-process constraint), and the second `create_hostif` fails (`TSocket read 0 bytes`). Per-test isolation (§5) gets a genuinely fresh saiserver per test via a full backend restart, which works.

### 1.5 Result (matrix delta vs. the 6-18 §5b baseline)
- Phase 1 (per-group): PASS **28 → 31 (+3)** (`EcmpReuseLagRoute{V4,V6}`, `RemoveNexthopGroupTestV4`); all ECMP `-6`/`-7`/ERROR gone; ERROR back to 11.
- Phase 2 (per-test isolation, workarounds reverted): PASS **31 → 32** — `RouteSameSipDipv4Test` additionally flips FAIL→PASS (it was an ordering-dependent victim of cross-test state in grouped mode; isolation fixes it). No regressions.
- `ReAddLagEcmpTest{V4,V6}` and `RemoveLagEcmpTest{V4,V6}` still fail **only** on the dataplane (`Did not receive expected packet`) — the remaining LAG-ECMP forwarding gap shared with the host-route / LPM families (next work item).

Current matrix (`results/compatibility-matrix-6-19.md`): PASS 32 / FAIL 50 / ERROR 11 / SKIP 2 of 95. The only remaining `-6` are `SviMacFlooding{,v6}Test` — the confirmed-unsupported SVI MAC family, not ECMP.

### 1.6 Conclusion
Item 2b's `-6`/`-7` were **test-harness config-reuse artifacts, not VPP SAI backend gaps** — consistent with the senior engineer's note that route/neighbor + member ops "pass sonic-mgmt; some sequence of requests not properly handled." The remaining ECMP failures are the LAG-ECMP **dataplane forwarding** gap.

---

## 2. Port-up wait speedup (`port_configer.py`) — ~75s → ~13s per config build

### 2.1 Why the per-config build took ~75s
Profiling the group-1 (representative) common-config build showed ~75s from test start to `common config done`, and ~64–68s of that was the OCP framework's `turn_up_and_get_checked_ports()` (`SAI/test/sai_test/config/port_configer.py`):

- It set every port `admin_state=True`, then for **each** of the 32 ports **serially**, polled `oper_status` and, if not UP, looped `retries=2` times with a blocking `time.sleep(1)` per iteration, re-asserting admin-up each time.
- In this VPP/veth harness the SAI `oper_status` reads `DOWN` (status 2) for the entire window — every port logs "not up ... Retry" and the loop ends with "Ports [0..31] are down after retries". So the loop spends ~`32 × retries × 1s ≈ 64s` and then gives up.

### 2.2 The wait is dead time here (but the links are fine)
Inspecting a running container *after* the build (`vppctl show interface`, `/sys/class/net/OEthernet0/{operstate,carrier}`) shows the VPP host-interfaces are **up with carrier and tx packets** — the links genuinely come up, just *after* the poll loop has already given up. The reason: the `host-OEthernetX` interfaces are created **admin-DOWN by the SAI hostif-creation path** and `linux_cp` takes a few seconds to propagate carrier; the test polls faster than that and exhausts its budget. Crucially `down_port_list` is **informational only** (printed, never asserted), and the dataplane works by the time the test body runs — so the entire wait is wasted in this environment.

### 2.3 Fix — env-gated shared/bounded wait (upstream-safe by default)
`turn_up_and_get_checked_ports()` now reads three env knobs, **defaulting to the exact original behavior** so real-ASIC / other OCP consumers are unchanged unless they opt in:

- `SAI_PORT_UP_RETRIES` (default `2`) — unchanged retry count.
- `SAI_PORT_UP_POLL_INTERVAL` (default `1`) — unchanged poll interval (seconds).
- `SAI_PORT_UP_SHARED_WAIT` (default `0`) — when `1`, switch from the per-port serial budget to a **single bounded wait shared across all ports**: issue admin-up to every port once, then poll **all** still-down ports together for at most `retries × interval` seconds total, re-asserting admin-up on stragglers each round.

Semantics are preserved (every port set admin-up; oper-status polled; identical `down_port_list` and returned list); only the *total wall-clock wait* changes from `N × retries × interval` to `retries × interval`.

`run_test.sh` opts our harness in by exporting `SAI_PORT_UP_SHARED_WAIT=1` (knobs are overridable). This is test-only code — `port_configer.py` is part of the OCP PTF suite invoked through the Thrift `saiserver` shim; it is **not** in the SONiC production image path (production brings ports up via orchagent/portmgrd from CONFIG_DB), so there is **no impact on production image functionality, stability, or debuggability**. The only shared consumers are other OCP/PTF runners (incl. real ASICs), and they keep the original timing because the fast path is opt-in.

### 2.4 Measured result (group-1 transition, representative)
| Phase | Before | After |
|-------|-------:|------:|
| Port-up wait (`turn_up_and_get_checked_ports`) | ~64–68s | **~6s** |
| Full common-config build (start → `common config done`) | ~75s | **~13.5s** |
| Total inter-group reconfigure (`Force stopping` → `common config done`) | ~95s | **~31s** |

Per-test runtime also dropped (e.g. each ECMP test ~80s → ~17–20s). Re-ran the mutating/destructive ECMP tests (`EcmpReuseLagRouteV4`, `RemoveNexthopGroupTestV4`, `RemoveAllNextHopMemeberTestV4`, `EcmpLagDisableTestV4`) with the shared wait: all still **PASS**, no behavior change.

### 2.5 Knock-on benefit
With the dominant ~64s removed, per-test backend recycle became affordable (~31s reconfigure vs. ~95s), which is exactly what §3 builds on.

---

## 3. Per-test isolation mode (`ISOLATE_EACH_TEST`) — the real fix for the artifacts

### 3.1 What it does
`run_test.sh` gained `ISOLATE_EACH_TEST` (default **1**). When on, the run planner assigns **every test target a unique group id**, so the existing per-group machinery tears down and brings up a fresh backend (VPP + saiserver) before each test and each test rebuilds its own common config from scratch (`common_configured=false`). No test ever reloads a `dut` persisted by another test. Set `ISOLATE_EACH_TEST=0` to fall back to config-signature grouping with persisted-config reuse (faster full runs, but tests then share a backend within a group).

Implementation is a one-line plan rewrite (awk renumbers the group column to a unique value per line); the signature-grouping logic in `plan_test_groups` is untouched and still used when the mode is off.

### 3.2 Why this lets us delete the workarounds
The Item 2b `-6`/`-7`/`ValueError` artifacts (§1) were **entirely** caused by reusing a persisted `dut` across tests in a shared-backend group. With per-test isolation there is no shared persisted state, so the Phase-1 workarounds in `sai_ecmp_test.py` and the re-persist calls in `route_configer.py` have nothing left to fix and were reverted (§1.4). Only the genuine `member_port_indexs` aliasing bug fix is kept (it is not a reuse artifact). `sai_ecmp_test.py` is now pristine upstream.

### 3.3 Validation
Full 4-module matrix in isolation mode (each of 85 tests in its own group, fresh backend + own config, workarounds reverted):
- **PASS 32 / FAIL 50 / ERROR 11 / SKIP 2** — i.e. **+1 vs. grouped Phase-1 (31)**: `RouteSameSipDipv4Test` flips FAIL→PASS (an ordering-dependent victim in grouped mode). No regressions; **zero ECMP `-6`/`-7`/ERROR**; the only `-6` left are the out-of-scope `SviMacFlooding{,v6}Test`.
- The previously-affected ECMP tests (`RemoveLagEcmp*`, `ReAddLagEcmp*`, `EcmpReuse*`, `RemoveNexthopGroup*`, `RemoveAllNextHopMemeber*`) confirmed clean in their own groups (the destructive/reuse ones PASS; `RemoveLag*`/`ReAdd*` fail only on the dataplane).

### 3.4 Cost
Per test pays ~22s of recycle+rebuild (backend ~9s + config build ~13.5s, the latter only affordable because of §2). See §4 for the full head-to-head with grouped mode.

---

## 4. Mode comparison — isolation vs. grouped (head-to-head)

Both runs were on the **same image** (pristine upstream `sai_ecmp_test.py`; only the `port_configer.py` shared-wait and the `route_configer.py` aliasing fix present), the same 4 modules (`sai_route_test sai_rif_test sai_neighbor_test sai_ecmp_test`), 95 collected tests, `PORT_COUNT=32`.

### 4.1 Invocation
Isolation mode (default — each test gets a fresh backend + own config):
```bash
docker run --rm --privileged -e PORT_COUNT=32 \
  -v "$PWD/results/xml:/test-results" \
  docker-sai-test-vpp:phase1 \
  sai_route_test sai_rif_test sai_neighbor_test sai_ecmp_test
```
Grouped mode (config-signature grouping + persisted-config reuse):
```bash
docker run --rm --privileged -e PORT_COUNT=32 -e ISOLATE_EACH_TEST=0 \
  -v "$PWD/results/xml:/test-results" \
  docker-sai-test-vpp:phase1 \
  sai_route_test sai_rif_test sai_neighbor_test sai_ecmp_test
```
(`gen_compatibility_matrix.py [xml_dir] [out.md]` then builds the matrix.)

### 4.2 Results

| Metric | Grouped (`ISOLATE_EACH_TEST=0`) | Isolation (`ISOLATE_EACH_TEST=1`, default) |
|--------|:--:|:--:|
| Wall-clock (4 modules, 95 tests) | **~5m15s** | **~49m** |
| ✅ PASS | **27** | **32** |
| ❌ FAIL | 55 | 50 |
| ⚠️ ERROR | 11 | 11 |
| ⏭️ SKIP | 2 | 2 |

Grouped mode is ~9× faster but passes 5 fewer tests, because with the upstream workarounds reverted the cross-test config-reuse artifacts return in grouped mode. Isolation trades ~9× runtime for full correctness with pristine upstream tests. Use grouped mode for fast iteration; use isolation mode for an authoritative matrix.

### 4.3 The 5 tests that PASS only under isolation (and why)
These fail in grouped mode purely because they share one saiserver + a persisted `dut` within their config-signature group, and a co-grouped predecessor mutated/destroyed the shared NHG state (the meta layer then rejects the stale/duplicate OID). In isolation each gets a fresh backend and rebuilds its own config, so the artifact cannot occur:

| Test | Grouped failure | Root cause removed by isolation |
|------|-----------------|---------------------------------|
| `sai_ecmp_test.EcmpReuseLagRouteV4` | `-6` (ITEM_ALREADY_EXISTS) | a predecessor (e.g. `EcmpCoExistLagRoute`) leaked an inline NHG member / route on the same nexthop+prefix; the fresh-config build has no such leftover |
| `sai_ecmp_test.EcmpReuseLagRouteV6` | `-6` | same as V4 (v6 nexthops/prefix) |
| `sai_ecmp_test.RemoveAllNextHopMemeberTestV4` | `-7` (ITEM_NOT_FOUND) | reads a persisted `dut` whose NHG members a prior test already removed; its own fresh build has the full member set |
| `sai_ecmp_test.RemoveNexthopGroupTestV4` | `-7` | a prior test in the group destroyed/mutated the shared NHG it expects to delete; isolation gives it an intact NHG |
| `sai_route_test.RouteSameSipDipv4Test` | dataplane (`Did not receive expected packet`) | a prior test left the shared backend in a forwarding state that breaks this case; with its own backend it forwards correctly (long-standing ordering-dependent flake, see progress-6-17) |

Additionally, `ReAddLagEcmpTest{V4,V6}` fail in **both** modes but more *accurately* under isolation: grouped reports `-7` (stale-OID artifact masking the real behavior), while isolation reaches the test body and reports the genuine LAG-ECMP **dataplane** failure (`Did not receive expected packet`) — the actual remaining gap. (In grouped mode `ReAddLagEcmpTestV6` reports `-7` rather than an ERROR thanks to the retained `member_port_indexs` aliasing fix.)

Artifacts: `results/compatibility-matrix-6-19.md` (isolation) and `results/compatibility-matrix-grouped.md` (grouped).

---

## 5. Neighbor host-route forwarded via `local0` (drop) — root cause of the LAG/host-route/LPM dataplane gap (FIXED, backend)

### 5.1 Symptom
The dominant remaining FAIL family across `sai_neighbor_test` / `sai_route_test` (and the LAG-ECMP `ReAdd*`/`RemoveLag*` tests) was `Did not receive expected packet` — routed traffic to a neighbor's `/32` (or `/128`) was dropped. This spanned three superficially different cases: LAG host-routes (`AddHostRouteTest`, `RemoveAddNeighborTest*`), port/SVI host-routes, and LPM `/32` overrides (`RouteLPMRouteRif/Nexthop*`).

### 5.2 Root cause (one bug, confirmed via live VPP FIB)
All three go through `SwitchVpp::programNeighborHostRoute()` (`vslib/vpp/SwitchVppNbr.cpp`), which installs the neighbor's host route. Inspecting the FIB for a failing dst (e.g. `10.1.1.100/32` over a LAG, `192.168.1.200/32` over a port) showed two competing entries:
```
API refs:1 src-flags:added,contributing,active,        <- ACTIVE path
  10.1.1.100 local0
  [@0]: dpo-load-balance ... [0] [@0]: dpo-drop ip4     <- DROPS
adjacency refs:1 ... cover:-1                            <- correct path, NOT active
  10.1.1.100 BondEthernet0
  [@0]: ipv4 via 10.1.1.100 BondEthernet0 ... 0001010101640077665544000800   <- correct rewrite
```
The active path resolved to **`local0`** and dropped, while the correct attached-nexthop adjacency over the real interface existed only as the inactive cover.

Cause: `programNeighborHostRoute()` builds its `nexthop_grp_member_t` with `memset(&member, 0, ...)` and never set `member.sw_if_index`, leaving it **0**. In `ip_route_add_del()` (`vppxlate/SaiVppXlate.c`):
```c
if (nexthop->sw_if_index != (u32)-1) {        // 0 != -1  -> taken
    fib_path->sw_if_index = htonl(0);         // sw_if_index 0 == local0 !
} else if (nexthop->hwif_name) {              // the correct branch, never reached
    idx = get_swif_idx(vam, nexthop->hwif_name);
    ...
}
```
So the host route's path was pinned to `local0` instead of resolving `hwif_name` to the real `BondEthernet<N>` / `host-OEthernet<N>`. The connected/NHG route paths were unaffected because `fillNHGrpMember()` already sets `sw_if_index = ~0`; only the hand-built neighbor host-route member missed it.

### 5.3 Fix
One line in `programNeighborHostRoute()`:
```c
member.sw_if_index = (uint32_t) ~0;   // select the hwif_name lookup branch, not local0
```
(Requires a `libsaivs`/`libsairedis` `.deb` rebuild + image rebuild.)

### 5.4 Validation
- `sai_neighbor_test.AddHostRouteTest` → **PASS** (was dataplane FAIL): LAG host-route now forwards on LAG members.
- `sai_route_test.RouteLPMRouteRifTest` → **both forwarding assertions now pass** ("received from port2", then LPM override "received from port1"); the test now fails only in **tearDown** with `-17` (OBJECT_IN_USE) because the test removes `port2_rif` without first removing its neighbor (`port2_nbr_entry`) — a teardown-ordering gap / RIF-removal cleanup question, not the forwarding gap.
- `sai_neighbor_test.RemoveAddNeighborTestIPV4` → the first forwarding check now passes; it now fails later at the **CPU-punt counter** assertion (`post-pre == 1`, got 0) — i.e. traffic to a route whose neighbor was removed is not trapped to CPU queue 0. That is a separate feature (the test itself annotates "bug 15205360"), not the forwarding gap.

So the fix resolves the host-route **forwarding** family; the residuals it exposes (`-17` RIF-in-use on teardown, CPU-trap-on-unresolved-route counter) are distinct follow-ups.

### 5.5 Full-matrix result (isolation mode, with the fix)
PASS **32 → 34** (same 85 tests): `AddHostRouteTest` and `AddHostRouteTestV6` flip
FAIL→PASS. The rest of the host-route family now fails only on the §5.4 residuals
(forwarding itself works): `RemoveAddNeighborTest{IPV4,IPV6}` → `0 != 1` (CPU-trap
counter); `RouteLPMRouteRif{,v6}Test` / `RouteLPMRouteNexthop{,v6}Test` → `-17`
(RIF removed while neighbor still bound, in tearDown). No regressions; the
`sai_rif_test.Ingress*` FAIL→ERROR deltas are the confirmed-unsupported RIF-ingress
family (collection/teardown variance), not in scope. Snapshot:
`results/compatibility-matrix-6-19.md`.

---

## 6. Residual analysis — both are correct backend behavior, not new gaps

After the §5 fix, the host-route family's two remaining failure signatures were
investigated with runtime evidence. Both turned out to be cases where the
backend/meta layer is behaving **correctly**; neither is fixed by changing backend
code (doing so would either be wrong or is a large separate feature).

### 6.1 `-17` (OBJECT_IN_USE) on `port2_rif` removal — upstream test teardown bug
Tests: `RouteLPMRouteRifTest`, `RouteLPMRouteRifv6Test`, `RouteLPMRouteNexthopTest`,
`RouteLPMRouteNexthopv6Test`.

The `-17` is emitted by the **meta layer**, not vslib. saiserver log on the second
`remove_router_interface`:
```
ERROR: meta_generic_validation_remove: object 0x600000006 reference count is 1, can't remove
```
i.e. `port2_rif` still has refcount 1 because the test's `port2_nbr_entry` neighbor
references it. The tests remove `port1`'s neighbor before `port1_rif` (correct) but
**forget to remove `port2_nbr_entry` before `port2_rif`** — an asymmetric teardown
bug in the upstream OCP test. Requiring neighbors to be removed before their RIF is
**standard SAI semantics** (real orchagent does this ordering), so the meta refcount
rejection is correct; auto-cascading neighbor cleanup on RIF removal would *deviate*
from SAI and is not done. The test's actual purpose — LPM route forwarding (incl. the
`/32` override to a more-specific next hop) — now **passes**; the `-17` is teardown
noise only. **Decision: document as an upstream test teardown bug + correct SAI
behavior; no backend or test change.**

### 6.2 CPU-trap counter `0 != 1` — unimplemented CPU-punt feature (confirmed limitation)
Tests: `RemoveAddNeighborTestIPV4`, `RemoveAddNeighborTestIPV6`.

After the neighbor is removed, the test sends a packet to the (still-present) route
whose next hop is now unresolved and asserts the switch **CPU queue 0** packet count
increased by 1 — i.e. the packet should be **trapped/punted to the CPU** for ARP
resolution. Runtime check: the packet is correctly **dropped** (`verify_no_other_packets`
passes — no leak), but VPP drops it at the glean/unresolved adjacency and does **not**
punt it to the CPU host-interface with SAI per-queue stat accounting, so the counter
stays 0. This CPU-punt-on-unresolved-route path plus `SAI_QUEUE_STAT_PACKETS` plumbing
on the CPU port is a **substantial, separate backend feature**; the OCP test itself
annotates this very assertion with "bug 15205360" (a known-questionable check
upstream). **Decision: document as a confirmed backend limitation; leave the test
FAIL.** (Forwarding/drop behavior itself is correct.)

This joins the existing confirmed-unsupported set (SVI MAC learning/FDB, SVI
broadcast / SVI-to-SVI, SVI neighbor idempotency, RIF ingress disable, ECMP hash-field
distribution).

---

## 7. Files touched
- `vslib/vpp/SwitchVppNbr.cpp` — **backend fix:** set `member.sw_if_index = ~0` in
  `programNeighborHostRoute()` so the neighbor host route resolves via `hwif_name`
  instead of being pinned to `local0`/drop (§5). Requires `.deb` + image rebuild.
- `SAI/test/sai_test/config/port_configer.py` — env-gated shared/bounded port-up wait (§2).
- `SAI/test/sai_test/config/route_configer.py` — **kept:** independent `member_port_indexs` copy per v4/v6 NHG (latent aliasing bug). **Reverted:** the Phase-1 `persist_dut` re-persist calls.
- `SAI/test/sai_test/sai_ecmp_test.py` — **reverted to pristine upstream** (Phase-1 distinct-signature / persist-clear workarounds removed).
- `.azure-pipelines/docker-sai-test-vpp/run_test.sh` — `ISOLATE_EACH_TEST` mode (default on) + exported `SAI_PORT_UP_*` knobs (opt into shared wait).

## 8. Next steps
- The §5 residuals are dispositioned (§6): the `-17` is an upstream test teardown bug +
  correct SAI behavior; the CPU-trap counter is a confirmed limitation. No further
  action on those.
- Remaining LAG-ECMP **dataplane** failures (`ReAddLagEcmp*`, `RemoveLagEcmp*`) — verify
  whether the host-route fix changes them and what (if any) ECMP-member forwarding gap
  remains.
