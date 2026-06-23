# VPP SAI Unit-Test Framework — Progress Summary

High-level rolling summary of the project. Each entry is a short (≤5 sentence)
summary of one progress update; the full bug-by-bug detail for each date lives in
the matching `progress-<date>.md` / `debug-<date>.md` in this directory.

Newest first.

---

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
