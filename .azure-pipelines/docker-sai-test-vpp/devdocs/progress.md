# VPP SAI Unit-Test Framework — Progress Summary

High-level rolling summary of the project. Each entry is a short (≤5 sentence)
summary of one progress update; the full bug-by-bug detail for each date lives in
the matching `progress-<date>.md` / `debug-<date>.md` in this directory.

Newest first.

---

## 2026-06-19 — ECMP `-6`/`-7` were config-reuse artifacts (fixed); port-up wait sped up ~75s→~13s
*(detail: [progress-6-19.md](progress-6-19.md))*

Showed the ECMP `-6`/`-7` (`EcmpReuseLagRoute`, `ReAddLagEcmp`, `RemoveNexthopGroup`,
etc.) were **test-harness config-reuse artifacts, not VPP SAI backend gaps**: the
harness shares one saiserver per config-signature group and reuses a persisted `dut`
(because the VPP backend can only build the switch once per process), so a test that
mutates/destroys NHG membership without re-persisting leaves later tests in the group
asking the meta layer to remove/recreate stale OIDs (`-7`/`-6`) — the `vslib`
idempotency code is never even reached, and each test passes standalone. Fixed in the
OCP test layer by re-persisting the `dut` after NHG member mutations, giving the v4/v6
NHGs independent `member_port_indexs` lists (a latent aliasing bug → the
`ReAddLagEcmpTestV6` ERROR), and giving destructive/inline NHG tests distinct config
signatures so each gets a fresh saiserver: PASS **28→31**, all ECMP `-6`/`-7`/ERROR
gone. We considered tearing down/bringing up the backend per test instead (would keep
upstream tests pristine) but measured ~95s reconfigure between groups — ~75s of it the
T0 config build, of which ~80% (~64s) was the OCP framework's serial per-port admin-up
polling (32 ports × 2 retries × 1s) that is pure dead time here (SAI oper-status reads
DOWN the whole window but the links settle right after and tests pass anyway). So we
added an env-gated **shared/bounded** port-up wait to `port_configer.py` (defaults to
the original behavior; our `run_test.sh` opts in), cutting the port wait ~64s→~6s and
the config build ~75s→~13.5s with no production impact (test-only code path). With that
speedup making per-test recycle affordable (~22s/test), added `ISOLATE_EACH_TEST` (now
the harness default): every test runs in its own group with a fresh backend and its own
freshly-built config, so no persisted `dut` is ever shared. This removed the artifacts'
root cause, letting us **revert the in-test workarounds** — `sai_ecmp_test.py` is
pristine upstream again and only the genuine `member_port_indexs` aliasing bug fix is
kept in `route_configer.py`. Head-to-head on the same (pristine-upstream) image: **isolation = 32 PASS in ~49 min**
vs **grouped = 27 PASS in ~5m15s**. The 5 isolation-only passes are the config-reuse
victims — `EcmpReuseLagRoute{V4,V6}` (`-6`), `RemoveAllNextHopMemeberTestV4` /
`RemoveNexthopGroupTestV4` (`-7`), and `RouteSameSipDipv4Test` (ordering-dependent
dataplane flake). Grouped is ~9× faster for iteration; isolation is the authoritative
matrix. Flip with `-e ISOLATE_EACH_TEST=0`.

## 2026-06-18 — LAG/ECMP backend fixes: saithriftv2 `switch_id` fallback + vslib host-route/NHG/LAG-member
*(detail: [progress-6-18.md](progress-6-18.md))*

Resumed the 6-18 plan after finding the Docker image still ran a **2017 `saiserver`**
while the harness uses **saithriftv2** (`meta/sai.thrift`). OCP tests that omit
`switch_id` on `neighbor_entry` / `route_entry` hit meta `-5` because the RPC-global
`switch_id` (set by `sai_thrift_create_switch`) was not used as fallback. Added custom
`parse_neighbor_entry` / `parse_route_entry` in `sai_rpc_server_helper_functions.tt`,
rebuilt v2 `saiserver`, and repacked `debs/saiserver_0.9.4_amd64.deb`. With the new
server plus prior vslib fixes (`programNeighborHostRoute`, ROUTER_INTERFACE NH on LAG,
NHG idempotent remove, `SAI_LAG_MEMBER_ATTR_EGRESS_DISABLE`), **SAI API `-5` on
`AddHostRouteTest` / `RemoveAddNeighborTestIPV4` setUp is fixed**; remaining failures
are PTF packet-not-received on LAG members `[17,18]` (dataplane, same family as 6-17
Issue A). The clean image was then rebuilt successfully (the earlier failure was just
missing proxy build args) and a full 4-module matrix run validated progress:
**PASS 19→27 (+8)**, FAIL 59→55, ERROR 13→11 (`compatibility-matrix-6-18.md`). The
`-5` neighbor/route-on-LAG-RIF family (Item 1 create) is fully resolved and
`EcmpLagDisableTestV6` (Item 3) flipped to PASS; `RemoveLagEcmpTestV4/V6` flipped
PASS→FAIL (another false-pass now exercising the real path). Remaining work is LAG/LPM
**dataplane forwarding** plus ECMP reuse/re-add `-6`/`-7` sequences.

## 2026-06-17 — L3-over-LAG forwarding: environment/topology gaps, now FIXED
*(detail: [progress-6-17.md](progress-6-17.md))*

Re-classified the dominant Phase 2 failure family — L3-over-LAG — from a presumed VPP
SAI backend gap to environment/topology gaps, after the senior engineer confirmed the
tests pass when the suite and topology are brought up manually (full SONiC stack). The
VPP SAI backend takes RIF IPs/MACs from the SONiC netdev/RIF attributes that
IntfMgr/teamd/orchagent normally provide; the standalone saiserver+OCP+PTF environment
provides none. Root cause was three coupled gaps: (A) the LAG egress BondEthernet has
no connected IP; (B) the VLAN SVI's BVI is never created because
`vpp_create_bvi_interface` treated `SAI_ROUTER_INTERFACE_ATTR_SRC_MAC_ADDRESS` as
mandatory and the OCP test never passes it; (C) the SVI BVI has no connected IP.
Fixes: a `run_test.sh` watchdog assigns the DUT connected IPs to each LAG `be<N>` LCP
tap (mirrored to BondEthernet by linux_nl) and to each `bvi<vlan_id>` via vppctl
(A, C); the backend now falls back to the switch src MAC when the RIF src MAC attr is
omitted (B, spec-compliant default, no orchagent-path regression — needs a `.deb`
rebuild). The watchdog was also changed to run for the whole backend lifetime so the
IPs survive long runs. Result: the core L3-over-LAG dataplane-forwarding tests
(`RouteRifTest`, `RouteRifv6Test`, `RouteUpdateTest`/v6, `LagMultipleRouteTest`/v6,
`RouteSameSipDipv6Test`) now PASS standalone and in the full `sai_route_test` module.
Remaining route/rif failures are separate families (SVI MAC learning/aging/flood,
`-5` host-route-on-LAG-RIF create, tearDown-cascade ERRORs) outside the L3-over-LAG
forwarding scope.

## 2026-06-15 — Run many tests in one go + compatibility matrix
*(detail: [progress-6-15.md](progress-6-15.md))*

Extended the harness so a single `docker run` executes any one, several, or all
discovered SAI tests and emits a JUnit-XML compatibility matrix (written to
`docker-sai-test-vpp/results/` by `gen_compatibility_matrix.py`). Discovered that
the VPP SAI backend can only build the switch/host-interfaces once per saiserver
process, so introduced **config-signature grouping with a per-group backend
restart**: tests are grouped by the common config their `setUp` requests
(resolved through the class inheritance chain), each group runs against a fresh
saiserver, the first test of a group builds the config and the rest reuse it.
This removed every harness/config-reuse artifact (e.g. ECMP `list index out of
range`), leaving a trustworthy matrix in which the remaining failures are genuine
SAI/VPP gaps — dominated by the **L3-over-LAG** family (neighbor/route/ECMP
programming on LAG-backed RIFs, and routed traffic not forwarded to LAG members).

## 2026-06-12 — Get the harness to actually run (sanity test passes)
*(detail: [progress-6-12.md](progress-6-12.md), [debug-6-12.md](debug-6-12.md))*

Drove `sai_sanity_test.SaiSanityTest` from "hangs/crashes" to a clean PASS at
`PORT_COUNT=32` by fixing a cascade of ~11 issues. The headline fix was a
**dedicated VPP event socket** in `SaiVppXlate.c` that resolved a mutex deadlock
between the synchronous command path and the async event path at scale; a VPP
SIGABRT from an IPv6 packet storm, several null-OID `struct.error`s (missing
`switch_id`, rejected `bridge_id`), a LAG bond-id lookup, and a PTF dataplane
carrier/RX-thread problem were also resolved. Net outcome: the full T0 setup and
the sanity flood test run end-to-end against the VPP dataplane.

## 2026-06-10 — Initial UT harness design and bring-up
*(detail: [progress-6-10.md](progress-6-10.md))*

Built the Phase 1 MVP harness: a single privileged Docker image
(`docker-sai-test-vpp`) bundling VPP + saiserver + PTF + the OCP `sai_test`
suite, with `run_test.sh` orchestrating Redis → veth/AF_PACKET topology → VPP →
saiserver → PTF. Documented the file architecture block-by-block and the early
errors fixed during bring-up. The harness reached SAI test setup but was still
blocked by the host-interface ("TAP dependency") ordering issue.

## 2026-06-03 — Phase 1 scaffolding and harness-vs-backend separation
*(detail: [progress-6-3.md](progress-6-3.md),
[vpp-port-admin-state-bug.md](vpp-port-admin-state-bug.md))*

Stood up the Phase 1 foundation aimed at running one SAI PTF test end-to-end
against a real VPP dataplane in one container, and established the guiding
principle of separating Docker-harness problems from pre-existing SAI/VPP backend
behavior. The container builds and starts Redis/VPP/saiserver/PTF, creates the
veth/AF_PACKET topology, and reaches SAI setup, but did not yet pass
`SaiSanityTest`. The main blocker identified was the SAI front-panel port
operational status staying down during the multi-port run (deep-dived in
`vpp-port-admin-state-bug.md`).
