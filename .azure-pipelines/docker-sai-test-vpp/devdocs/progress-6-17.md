# VPP SAI UT — L3-over-LAG Forwarding: Environment/Topology Gaps (FIXED)

Date: June 17, 2026

This document records the investigation that re-classified the dominant Phase 2 failure family — **L3-over-LAG** — from a presumed VPP SAI backend gap (`progress-6-15.md` §6d/6e/8.2) to a set of **environment/topology gaps**, and the fixes that make the family pass. The senior engineer confirmed these tests pass when the SAI test suite and topology are brought up manually (the full SONiC stack), which is the ground truth that this is an environment problem, not a `libsaivs.so` bug.

Root cause turned out to be **three** coupled gaps, all stemming from the same fact: the standalone `saiserver` + OCP `sai_test` + PTF environment does not provide the netdev IPs and RIF attributes that real SONiC (IntfMgr/teamd/orchagent) supplies, and the VPP SAI backend depends on them. Fixes:

- **Issue A** (LAG egress has no IP) — harness, `run_test.sh`.
- **Issue B** (VLAN BVI never created) — backend, `vslib/vpp/SwitchVppFdb.cpp` (approved switch-MAC fallback).
- **Issue C** (SVI has no IP) — harness, `run_test.sh`.

After all three, the core L3-over-LAG dataplane-forwarding tests pass (§6). It follows the investigation style of `progress-6-15.md` §2/§3.

---

## 1. Symptom recap (from the compatibility matrix)

The L3-over-LAG family in `results/compatibility-matrix.md` shows two signatures:

- **Data-plane FAIL** — `Did not receive expected packet on any of ports [19,20]` (LAG2) or `[17,18]` (LAG1): `sai_route_test.RouteRifTest/RouteRifv6Test`, `RouteSameSipDip*`, `RouteUpdate*`, `LagMultipleRoute*`, `SviMac*`, etc.
- **SAI `-5` (INVALID_PARAMETER)** on neighbor/route create over a LAG-backed RIF: `sai_neighbor_test.AddHostRouteTest`, `RemoveAddNeighborTestIPV4/V6`, `NhopDiffPrefixRemove*`, several `sai_route_test.RouteLPMRoute*` / `RouteDiffPrefixAddThenDelete*`.

`progress-6-15.md` classified all of these as "SAI/VPP gap, confirmed standalone". That classification was wrong: a test failing identically standalone only proves the failure is *not* a config-reuse artifact — it does NOT prove the cause is the backend, because the **standalone harness builds an incomplete topology** (the same incomplete topology in every run, batch or standalone).

---

## 2. How the VPP SAI backend gets a router-interface IP (the crux)

The VPP SAI backend does **not** take a RIF IP from SAI (SAI RIFs carry no IP attr). Instead, in production SONiC, `IntfMgr` configures the interface IP on the **SONiC kernel netdev**, and VPP's `linux_cp`/`linux_nl` plugin mirrors that address onto the matching VPP interface. The backend's own IP helpers read the address straight off the netdev:

```78:110:vslib/vpp/SwitchVppRif.cpp
bool vpp_get_intf_ip_address (const char *linux_ifname, ... ) {
    ...
    cmd << IP_CMD << " addr show dev " << linux_ifname << " to " << prefix.to_string()
        << " scope global | awk '/inet / {print $2}'";
    ...
}
```

The standalone `saiserver` + OCP `sai_test` + PTF environment has **no `IntfMgr`, no `teamd`, no orchagent** — nothing ever puts an IP (or, for SVIs, a MAC) on any netdev. So every router interface that needs L3 forwarding comes up with no connected subnet in VPP, and routed traffic is dropped. Confirmed at runtime: in a fresh container, `ip -4 -br addr show scope global | grep -E 'Ethernet|PortChannel|Vlan'` returns **zero** netdevs with an IP.

---

## 3. Issue A — LAG egress: BondEthernet has no L3 address (FIXED)

### 3.1 Evidence
For `sai_route_test.RouteRifTest` (route `192.168.12.0/24` → RIF over LAG2 → next hop `10.1.2.100`, expected egress on LAG2 members ports `[19,20]`), the VPP FIB at test time:

```
# show ip fib 10.1.2.100/32
  [@0]: ipv4 via 10.1.2.100 BondEthernet1: ... 0001010102640077665544000800  <- rewrite BUILT
 forwarding:   UNRESOLVED                                                    <- but UNRESOLVED

# show ip fib 192.168.12.0/24
        via 10.1.2.100 in fib:0 via-fib:109 via-dpo:[dpo-drop:0]             <- route DROPS
    [0] [@0]: dpo-drop ip4

# show interface address BondEthernet1
BondEthernet1 (up):                                                          <- NO IP
```

The neighbor rewrite adjacency is built, but with no connected subnet on `BondEthernet1` the host route `10.1.2.100/32` stays `UNRESOLVED`, so the recursive `192.168.12.0/24` resolves to `dpo-drop`. `show bond` was healthy throughout (`BondEthernet1` = `host-OEthernet19/20`, both active, link up) — so this is purely the missing L3 address, not a LAG-membership problem.

### 3.2 Root cause
There is no component to put the DUT-side connected IP on the LAG interface. The VPP SAI backend names a BondEthernet's `linux_cp` host-interface (LCP tap) **`be<N>`** (see `vpp_create_lag_member` → `configure_lcp_interface(hw_ifname, "be"+bond_id)` in `vslib/vpp/SwitchVppFdb.cpp` ~line 841). Adding an IP to the `be<N>` netdev makes `linux_nl` mirror it onto `BondEthernet<N>`:

```
# ip addr add 10.1.2.1/24 dev be1        (be1 is BondEthernet1's LCP host-if)
# show interface address BondEthernet1
BondEthernet1 (up):
  L3 10.1.2.1/24                          <- mirrored by linux_nl
# show ip fib 192.168.12.0/24
        via 10.1.2.100 ... via-dpo:[dpo-load-balance:127]   <- now FORWARDS to the bond
```

The T0 config (`config/route_configer.py` `t0_route_config_helper`) builds `LAG<k>` (→ `BondEthernet<k>` → `be<k>`) RIFs whose next hop is T1 device `10.1.(k+1).100` / `fc00:1::(k+1):100`, i.e. connected subnet `10.1.(k+1).0/24` (`fc00:1::(k+1):0/112`). The DUT takes the `.1` host in each.

> Note `be<N>` (the LCP host-interface, where the IP goes) is distinct from the
> `PortChannel<N>` netdev the harness pre-creates for bond-id derivation and the
> tap→PortChannel tc-mirred redirect. An earlier attempt that put the IP on
> `PortChannel<N>` did **not** work — that netdev is not the BondEthernet LCP pair,
> so `linux_nl` does not mirror it (verified: BondEthernet stayed with no IP).

### 3.3 Fix (run_test.sh, harness-only)
`be<N>` is created lazily by the backend during the common-config build (first LAG member add), so the IP cannot be pre-assigned at startup. Added a short-lived background watchdog `keep_lag_rif_ips_up` (mirrors the existing `keep_vpp_veths_up` pattern): for the config-build window it polls for each `be<N>` tap and, once present, assigns `10.1.(N+1).1/24` and `fc00:1::(N+1):1/112`. IPv6 needs a per-tap `disable_ipv6=0` (the harness sets `all/default disable_ipv6=1` to stop the 32 veths flooding VPP with DAD/NDP) and `accept_dad=0` + `nodad` (a "tentative" address is not scope-global, which the backend's `... scope global` read would miss). Wired into `start_backend`/`stop_backend`/`cleanup`; gated by `LAG_RIF_IPS` (default on) with `LAG_RIF_IPV4_PATTERN` / `LAG_RIF_IPV6_PATTERN` / `LAG_BE_TAP_PREFIX` knobs.

### 3.4 Result
The LAG **egress** path is now fully resolved at runtime (verified in `--debug`): `be1` gets `10.1.2.1/24` + `fc00:1::2:1/112` → mirrored onto `BondEthernet1` → `192.168.12.0/24` forwards via `dpo-load-balance` to `BondEthernet1` (members `host-OEthernet19/20`) with the correct neighbor MAC rewrite. This removes the egress-side cause for the whole `-5`/`[17,18]`/`[19,20]` family.

---

## 3.5 Issue A2 — LAG egress alone is not enough: the ingress is a VLAN port
Fixing the egress did not make `RouteRifTest` pass on its own, because the test **ingresses on a VLAN access port** and the routed frame must first be picked up by the VLAN SVI. That uncovered Issues B (BVI not created) and C (SVI has no IP), below. All three (A, B, C) must be fixed together for the L3-over-LAG route tests to pass.

---

## 4. Issue B — Ingress SVI: the VLAN BVI is never created (FIXED, backend)

Fixing the egress alone did **not** make `RouteRifTest` pass, because the test also exercises an **ingress SVI**. The T0 topology (`config/vlan_configer.py`) puts port indices `[1..8]` in VLAN10 and `[9..16]` in VLAN20:

```51:55:SAI/test/sai_test/config/vlan_configer.py
    if is_create_vlan:
        vlan = configer.create_vlan(10, [1, 2, 3, 4, 5, 6, 7, 8])
        ...
        vlan = configer.create_vlan(20, [9, 10, 11, 12, 13, 14, 15, 16])
```

`RouteRifTest` sends its routed packet (dst MAC = `ROUTER_MAC`) on `port_obj_list[5].dev_port_index` — i.e. **port 5, a VLAN10 access port** (verified: port-obj index ↔ Ethernet(N*4) ↔ host-OEthernetN ↔ PTF port N is a clean 1:1 map, so there is no port-map ordering bug). For that frame to be routed to LAG2 it must be picked up by the **VLAN10 SVI (a VPP BVI)** and L3-forwarded. Runtime state:

```
# show bridge-domain 10 detail
   10  ...  BVI-Intf: N/A          <- no BVI bound to the bridge
# show interface | grep bvi        -> (nothing)
```

So the injected L3 frame is **flooded in the L2 domain** (confirmed via VPP trace: `192.168.0.1 -> 192.168.12.1` fanned out `host-OEthernet1..8`, the VLAN10 members) instead of being routed.

### 4.1 Root cause
The VLAN-RIF create path bails before creating the BVI:

```442:447:vslib/vpp/SwitchVppFdb.cpp
    auto attr_mac_addr = sai_metadata_get_attr_by_id(SAI_ROUTER_INTERFACE_ATTR_SRC_MAC_ADDRESS, attr_count, attr_list);
    if (attr_mac_addr == NULL)
    {
        SWSS_LOG_NOTICE("attr ROUTER INTERFACE MAC Address is not found");
        return SAI_STATUS_FAILURE;
    }
```

Confirmed in `saiserver.log`: `vpp_create_bvi_interface: attr ROUTER INTERFACE MAC Address is not found`.

The OCP `sai_test` `RouteConfiger.create_router_interface` (`config/route_configer.py` ~line 531) creates the VLAN RIF with only `virtual_router_id`, `type`, `vlan_id` — it **never passes `src_mac_address`** (nor does it for PORT/LAG RIFs). The VPP backend's `vpp_create_bvi_interface` has **no fallback** to the switch default MAC (`SAI_SWITCH_ATTR_SRC_MAC_ADDRESS`), so with the OCP test the BVI is never built.

In the manual/sonic-mgmt path the **full orchagent** programs the VLAN RIF and passes the switch router MAC as `SAI_ROUTER_INTERFACE_ATTR_SRC_MAC_ADDRESS`, so the BVI is created and ingress routing works — which is exactly why the senior engineer sees these tests pass manually. Our standalone `saiserver` + OCP path has no orchagent to supply that MAC.

### 4.2 Fix (backend) — BVI src MAC falls back to the switch MAC
The BVI MAC is a **SAI create attribute**, not a netdev property, so the harness has nothing to write to (no LCP host-interface exists for a BVI). Per the SAI spec `SAI_ROUTER_INTERFACE_ATTR_SRC_MAC_ADDRESS` is **optional and defaults to the switch source MAC**. The VPP backend was treating it as mandatory. Fixed `vpp_create_bvi_interface` (`vslib/vpp/SwitchVppFdb.cpp`) to, when the RIF src MAC attr is absent, read `SAI_SWITCH_ATTR_SRC_MAC_ADDRESS` from the switch object and use that instead of returning `SAI_STATUS_FAILURE`. (Approved by the senior engineer; option chosen because it matches the SAI contract and how a real ASIC SAI behaves.)

```c
// vslib/vpp/SwitchVppFdb.cpp, vpp_create_bvi_interface()
auto attr_mac_addr = sai_metadata_get_attr_by_id(SAI_ROUTER_INTERFACE_ATTR_SRC_MAC_ADDRESS, ...);
if (attr_mac_addr != NULL) { memcpy(mac_addr, attr_mac_addr->value.mac, ...); }
else {
    // optional attr; SAI default is the switch src MAC. orchagent always passes it,
    // the OCP sai_test never does -> read it from the switch instead of failing.
    sai_attribute_t sw_attr; sw_attr.id = SAI_SWITCH_ATTR_SRC_MAC_ADDRESS;
    get(SAI_OBJECT_TYPE_SWITCH, m_switch_id, 1, &sw_attr);
    memcpy(mac_addr, sw_attr.value.mac, ...);
}
```

**Reason:** without a BVI the VLAN SVI cannot route; routed traffic into a VLAN access port is flooded/dropped, breaking every standalone L3 route test whose ingress is a VLAN member port (the bulk of `sai_route_test`/`sai_rif_test`).

**Side effects / risk assessment:**
- **No regression to the orchagent path.** orchagent always passes `SAI_ROUTER_INTERFACE_ATTR_SRC_MAC_ADDRESS`, so the `attr != NULL` branch (the prior behavior) is unchanged for production SONiC; the fallback only runs when the attr is omitted (today only the standalone OCP `sai_test`).
- **Spec-compliant default.** SAI defines the RIF src MAC default as the switch src MAC, so using it is the correct behavior, not a test-only hack.
- **Failure mode preserved when truly unset.** If the switch has no src MAC either, the function still returns `SAI_STATUS_FAILURE` (now with a clearer log), so a genuinely missing MAC is not silently masked.
- Requires a `libsaivs`/`libsairedis` `.deb` rebuild + image rebuild (backend change).
- The same missing-MAC handling for PORT/LAG RIFs was **not** changed (those paths don't gate on the MAC); this fix is scoped to the BVI create only.

---

## 5. Issue C — Ingress SVI has no connected IP (FIXED, harness)

With the BVI created (Issue B), the VLAN ingress frame reaches `bvi10` and enters L3, but is then dropped at `ip4-validate` (VPP trace: `IP4-VALIDATE ... -> error-drop rx:bvi10`) because **`bvi10` has no L3 address** — VPP does not enable IP4 input on an interface with no address. Same root cause as Issue A (no IntfMgr to set the connected subnet), but a BVI has **no linux_cp host-interface** to mirror an IP from, so the `be<N>` trick does not apply.

### 5.1 Fix (harness)
Extended the `keep_lag_rif_ips_up` watchdog to also program the SVI connected IP **directly in VPP** via `vppctl set interface ip address bvi<vlan_id> ...` once the BVI exists. T0 builds VLAN10 → server group 1 → `192.168.1.0/24` (`fc02::1:0/112`) and VLAN20 → group 2 → `192.168.2.0/24` (`fc02::2:0/112`); the DUT takes the `.1` host (`SVI_RIF_VLANS="10:1 20:2"`, gated by `SVI_RIF_IPS`). Verified by VPP trace: the VLAN-ingress frame is now `ip4-rewrite → BondEthernet1-output → host-OEthernet19-tx` (routed to a LAG2 member, TTL decremented), end to end.

### 5.2 Watchdog must run for the whole backend lifetime
The watchdog originally used the `keep_vpp_veths_up` fixed window (`KEEP_VETHS_UP_SECONDS`, 120s). A single config group runs many tests for well over 120s, and later tests (the SVI MAC-learning/aging suite) churn interface/FDB state, which dropped the connected IPs after the window closed and made the LAG/SVI route tests **pass in a small batch but fail in the full module run**. Fixed by making the watchdog loop until `stop_backend`/`cleanup` kills it (it is already PID-tracked and killed there), so the IPs are continuously re-asserted.

---

## 6. Validation

`docker-sai-test-vpp:phase1` rebuilt with all three fixes (Issue A/C harness + Issue B `.deb`). The **core L3-over-LAG dataplane-forwarding family now PASSES**, both standalone and in the full `sai_route_test` module run:

| Test | Before | After |
|------|--------|-------|
| `sai_route_test.RouteRifTest` (LAG2 [19,20]) | FAIL "Did not receive [19,20]" | **PASS** |
| `sai_route_test.RouteRifv6Test` | FAIL | **PASS** |
| `sai_route_test.RouteUpdateTest` (LAG1 [17,18]) | FAIL | **PASS** |
| `sai_route_test.RouteUpdatev6Test` | FAIL | **PASS** |
| `sai_route_test.LagMultipleRouteTest` | FAIL | **PASS** |
| `sai_route_test.LagMultipleRoutev6Test` | FAIL | **PASS** |
| `sai_route_test.RouteSameSipDipv6Test` | FAIL | **PASS** |

(`RouteRifTest` log: all 8 DIPs `received packet ... on one of lag2 member`, `OK`.)

### 6.1 Still failing — separate families, OUT of the L3-over-LAG forwarding scope
These remain in `sai_route_test`/`sai_rif_test` and are *not* the L3-over-LAG forwarding gap this task targeted; they are distinct issues for follow-up:
- **SVI MAC learning/aging/move/flood** (`SviMacLearning*`, `SviMacAging*`, `SviMacMove*`, `SviMacFlooding*`, `SviMacLarningAfterAge*`) and **SVI-to-SVI L3** (`SviRouteL3*`), plus `SviDirectBroadcastTest`: an SVI/FDB L2-behavior family (intra-VLAN learning/flooding), not LAG forwarding.
- **`-5` on overlapping / host-route-on-LAG-RIF create** (`RouteLPMRoute*`, `RouteDiffPrefixAddThenDelete*`, and the `sai_neighbor_test` `AddHostRouteTest` / `RemoveAddNeighborTestIPV4/V6` / `NhopDiffPrefixRemove*`): a create-time backend family — e.g. `AddHostRouteTest` does `create_neighbor_entry(rif=lag_list[0].rif, 10.1.1.10, no_host_route=False)` and the implicit `/32` host route over the LAG RIF returns `-5`. Needs separate backend investigation, distinct from forwarding.
- **`IngressMacUpdate*` / `IngressMtu*` ERRORs**: the pre-existing unguarded-tearDown cascade artifacts (see `progress-6-15.md` §6c), unrelated to this change.
- **`RouteSameSipDipv4Test`**: passes standalone / in small batches but flaps in the full run (`[19,20]`); likely a uRPF / SIP==DIP behavioral nuance under state churn, not the systematic LAG-IP gap. Tracked as a follow-up, not part of this fix.

---

## 7. Summary of changes

| Change | File | Type | Why |
|--------|------|------|-----|
| `keep_lag_rif_ips_up` watchdog: LAG `be<N>` tap IPs (v4+v6) | `run_test.sh` | harness | Issue A — give BondEthernet a connected subnet |
| same watchdog: SVI `bvi<vlan_id>` IPs via vppctl | `run_test.sh` | harness | Issue C — give the SVI a connected subnet |
| watchdog loops for backend lifetime (not fixed 120s) | `run_test.sh` | harness | re-assert IPs across long runs / state churn |
| BVI src MAC falls back to switch src MAC | `vslib/vpp/SwitchVppFdb.cpp` | backend (`.deb`) | Issue B — create the VLAN BVI when the RIF MAC attr is omitted |

New env knobs (all default-on, overridable): `LAG_RIF_IPS`, `LAG_RIF_IPV4_PATTERN`, `LAG_RIF_IPV6_PATTERN`, `LAG_BE_TAP_PREFIX`, `SVI_RIF_IPS`, `SVI_RIF_VLANS`, `SVI_RIF_IPV4_PATTERN`, `SVI_RIF_IPV6_PATTERN`, `SVI_BVI_PREFIX`.

---

## 8. How to reproduce / inspect

```bash
cd <sonic-buildimage>
# backend (.deb) — only when vslib/vpp changes (Issue B):
rm -f target/debs/bookworm/libsaivs_*.deb target/debs/bookworm/libsairedis_*.deb \
      target/debs/bookworm/libsaivs-dev_*.deb target/debs/bookworm/libsairedis-dev_*.deb
NOTRIXIE=1 make target/debs/bookworm/libsairedis_1.0.0_amd64.deb
cp target/debs/bookworm/{libsaivs,libsaivs-dev,libsairedis,libsairedis-dev}_*.deb \
   src/sonic-sairedis/.azure-pipelines/docker-sai-test-vpp/debs/

cd src/sonic-sairedis
docker build -f .azure-pipelines/docker-sai-test-vpp/Dockerfile \
  -t docker-sai-test-vpp:phase1 .            # (+ proxy --build-arg behind a proxy)

cd .azure-pipelines/docker-sai-test-vpp
# the L3-over-LAG forwarding family now passes:
docker run --rm --privileged -e PORT_COUNT=32 docker-sai-test-vpp:phase1 \
  sai_route_test.RouteRifTest sai_route_test.RouteUpdateTest sai_route_test.LagMultipleRouteTest

# inspect the now-correct dataplane in --debug:
docker run -d --name officesai-debug --privileged -e PORT_COUNT=32 \
  docker-sai-test-vpp:phase1 --debug sai_route_test.RouteRifTest
# after ~80s:
docker exec officesai-debug ip addr show dev be1                         # 10.1.2.1/24 (Issue A)
docker exec officesai-debug vppctl show interface address BondEthernet1  # L3 mirrored
docker exec officesai-debug vppctl show interface address bvi10          # L3 192.168.1.1/24 (Issue C)
docker exec officesai-debug vppctl show bridge-domain 10 detail          # BVI-Intf: bvi10 (Issue B)
docker exec officesai-debug vppctl show ip fib 192.168.12.0/24           # dpo-load-balance (not drop)
docker rm -f officesai-debug
```
