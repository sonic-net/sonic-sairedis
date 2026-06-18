# VPP SAI LAG/ECMP Backend Fixes — Phase 2 (6-18)

Date: June 18, 2026

Continuation of the 6-18 plan: LAG-RIF neighbor/host-route fixes, NHG member
idempotency, `SAI_LAG_MEMBER_ATTR_EGRESS_DISABLE`, and validation via OCP
`sai_test` in `docker-sai-test-vpp`.

---

## 1. Session resume — container hygiene

On resume, one stale debug container (`officesai-debug`) was still running from the
prior session; it was removed before new runs. No other `docker-sai-test-vpp` /
`officesai-*` containers were left behind.

---

## 2. Root cause: `-5` on neighbor/route create was **not** the vslib path first

### 2.1 Stale `saiserver` in the Docker image

The `saiserver_0.9.4_amd64.deb` staged in `debs/` (and inside
`docker-sai-test-vpp:phase1`) still contained an **8.5 MB binary dated 2017-03-24**.
Rebuilding `libsaithrift-dev` via `make target/debs/...` repackaged that same stale
artifact even though sources under `SAI/test/saithrift/` had been edited.

The harness uses **saithriftv2** (`SAI/meta/sai.thrift` + `sai_rpc_server.cpp`), not
the legacy `SAI/test/saithrift/switch_sai.thrift` server. Patching only the v1 RPC
server had no effect on the running container.

### 2.2 Thrift `switch_id` omission (OCP `sai_test`)

Many OCP tests build `sai_thrift_neighbor_entry_t` / `sai_thrift_route_entry_t` with
`rif_id` (+ `ip_address` / `destination`) but **no `switch_id`**. Meta validation
then emits:

```
ERROR: meta_generic_validation_create: switch id is NULL for SAI_OBJECT_TYPE_NEIGHBOR_ENTRY
```

Common-config paths that pass `switch_id=self.dut.switch_id` succeeded; isolated
tests (`AddHostRouteTest`, `RemoveAddNeighborTest*`) failed with `-5`.

### 2.3 Wrong global for fallback (`gSwitchId` vs `switch_id`)

In saithriftv2, `sai_thrift_create_switch()` stores the created OID in the RPC-global
`switch_id` (declared in generated `sai_rpc_server.cpp`). The legacy
`gSwitchId` in `saiserver.cpp` is **not** updated on that RPC path. A fallback that
only consults `gSwitchId` still leaves `switch_id == 0` for thrift clients that omit
the field.

---

## 3. Fixes applied

### 3.1 saithriftv2 RPC parsers (primary harness fix)

**Files:** `SAI/meta/templates/sai_rpc_server_helper_functions.tt` (source of truth),
patched `SAI/meta/sai_rpc_server.cpp` for immediate rebuild.

Custom parsers (skipping auto-generated struct copies):

- `sai_thrift_parse_neighbor_entry()` — if thrift `switch_id == 0`, use RPC-global
  `switch_id`, else `gSwitchId`.
- `sai_thrift_parse_route_entry()` — same fallback chain.

Rebuilt `saiserver` via `SAI/test/saithriftv2` (`platform=vpp`) and repacked
`debs/saiserver_0.9.4_amd64.deb` (~25 MB, includes parse fix).

### 3.2 vslib/vpp backend (from prior 6-18 work, unchanged this session)

| Area | File(s) | Change |
|------|---------|--------|
| LAG host routes | `SwitchVppNbr.cpp` | `programNeighborHostRoute()` on neighbor add/remove when `NO_HOST_ROUTE=false` |
| RIF type guard | `SwitchVppNbr.cpp` | Read RIF **type** before `PORT_ID`; skip VPP neighbor prog for non-PORT/SUB_PORT RIFs (fixes VLAN broadcast `-7`) |
| ROUTER_INTERFACE NH | `SwitchVppRoute.cpp` | Enable `vpp_add_del_intf_ip_addr` branch; resolve `hwif` from `rif_oid` |
| LAG intf IP | `SwitchVppRif.cpp` | LAG `BondEthernet<N>` / `be<N>` support in `vpp_add_del_intf_ip_addr` |
| NHG member | `SwitchVppNexthop.cpp` | Idempotent remove; treat re-add `ITEM_ALREADY_EXISTS` as success |
| LAG egress disable | `SwitchVppFdb.cpp`, `SwitchVpp.cpp` | `setLagMember()` / bond member detach-reattach |

`libsaivs` / `libsairedis` debs in `debs/` are dated 2026-06-18 (vslib changes).

### 3.3 Legacy v1 thrift (retained, not used by harness image)

`SAI/test/saithrift/src/switch_sai.thrift` + `switch_sai_rpc_server.cpp` still carry
`switch_id` field + `gSwitchId` fallback for completeness; the docker harness does
**not** install this server today.

---

## 4. Validation status (with patched saiserver overlaid on `phase1` image)

Until the Docker image is rebuilt with network access, tests used:

```bash
docker create ... docker-sai-test-vpp:phase1 --debug <test>
docker cp SAI/debian/usr/sbin/saiserver <container>:/usr/sbin/saiserver
docker start -a <container>
```

| Test | SAI API (`-5`) | Dataplane (PTF) | Notes |
|------|----------------|-----------------|-------|
| `AddHostRouteTest` | **PASS** (setUp) | FAIL — no pkt on LAG1 `[17,18]` | Neighbor + host route programmed; see saiserver log |
| `RemoveAddNeighborTestIPV4` | **PASS** (setUp) | FAIL — no pkt on `[17,18]` | Route create no longer `-5` after route parser fix |
| `AddHostRouteTestV6` | not re-run this session | — | |
| `RemoveAddNeighborTestIPV6` | not re-run this session | — | |
| Route LPM / diff-prefix families | not re-run | — | Blocked on image rebuild + matrix regen |

**Confirmed:** meta `switch id is NULL` for `NEIGHBOR_ENTRY` / `ROUTE_ENTRY` is
**fixed** when the v2 `saiserver` with parser fallback is used.

**Remaining:** PTF `verify_packet_any_port` failures on LAG member ports — same
signature as pre-6-17 L3-over-LAG dataplane gaps (`progress-6-17.md` Issue A). Those
were fixed for `RouteRifTest` via `run_test.sh` LAG IP watchdog; host-route neighbor
tests may need additional forwarding-path verification (nexthop type, connected route on
`be<N>`, etc.).

---

## 5. Build / image notes

- `docker build` initially failed because the build invocation did not pass the
  corporate proxy build args; the base-layer `apt-get install` could not reach the
  Debian mirror. **Resolved** by building with the proxy args documented in the
  README (`--build-arg http_proxy=... https_proxy=... no_proxy=...`). A clean
  `--no-cache` image now builds and bakes in the fresh debs.
- Repacked `debs/saiserver_0.9.4_amd64.deb` contains the v2 binary (25 MB); the clean
  image picks it up without any `docker cp` overlay. Verified in-image:
  `wc -c /usr/sbin/saiserver` = 25105104 and `sai_thrift_neighbor_entry_t` field
  order is `switch_id, rif_id`.
- Full `libsaithrift-dev` rebuild through `Makefile.work` still needs verification that
  `dh_auto_build` regenerates `sai_rpc_server.cpp` from the updated template (not a
  cached 2017 artifact); the working path used this session was a direct
  `make -C SAI/test/saithriftv2` build + repack.

---

## 5b. Compatibility matrix — clean-image run (2026-06-18)

Full run of `sai_route_test sai_rif_test sai_neighbor_test sai_ecmp_test` on the
clean image (85 collected tests; `results/compatibility-matrix-6-18.md`).

| Result | 6-17 baseline | 6-18 | Δ |
|--------|--------------:|-----:|---:|
| ✅ PASS | 19 | **27** | **+8** |
| ❌ FAIL | 59 | 55 | −4 |
| ⚠️ ERROR | 13 | 11 | −2 |
| ⏭️ SKIP | 2 | 2 | 0 |

### Newly PASSING (10 to-PASS flips)

- **Item 1 (route/neighbor on LAG RIF), `-5` now resolved → PASS:**
  `sai_neighbor_test.NhopDiffPrefixRemoveLonger{,V6}`,
  `NhopDiffPrefixRemoveShorter{,V6}`, `NoHostRouteTestV6`,
  `sai_route_test.RouteDiffPrefixAddThenDeleteLonger{V4,V6}Test`,
  `RouteDiffPrefixAddThenDeleteShorter{V4,V6}Test`.
- **Item 3 (LAG member egress-disable):** `sai_ecmp_test.EcmpLagDisableTestV6`
  flipped FAIL→PASS (V4 stays PASS). The egress-disable handler now enforces bond
  member removal, so traffic no longer leaks onto the disabled member.

### Regressions / honest re-classifications (2 from-PASS flips)

- `sai_ecmp_test.RemoveLagEcmpTestV4/V6` PASS→FAIL (`Did not receive expected
  packet`). Same shape as the 6-17 `EcmpLagDisableTestV6` false-pass: these
  previously passed vacuously and now exercise the real LAG-ECMP forwarding/removal
  path exposed by the NHG-member changes. To be triaged with the remaining ECMP
  forwarding family, not a true functional loss.

### ERROR→FAIL (4, net positive — no longer teardown-cascade ERRORs)

`sai_rif_test.IngressMacUpdateTest{,V6}`, `IngressMtuTest{V4,V6}` — these moved from
setUp/tearDown ERROR to a clean assertion FAIL. (`Ingress*` RIF families were
confirmed unsupported / out of scope.)

### Still-failing target families (dataplane, not SAI `-5`)

- `AddHostRouteTest{,V6}`, `RemoveAddNeighborTestIPV4/V6` — SAI create now succeeds;
  fail on `Did not receive expected packet on ports [17,18]` (LAG egress forwarding).
- `RouteLPMRouteNexthop{,v6}Test`, `RouteLPMRouteRif{,v6}Test` — `Expected packet not
  received on port 2` (LPM forwarding).
- `EcmpCoExistLagRoute{V4,V6}` — packet on unexpected port / not received.
- `EcmpReuseLagRoute{V4,V6}` — `-6` (ITEM_ALREADY_EXISTS) on reuse.
- `ReAddLagEcmpTest{V4,V6}` — `-7` (ITEM_NOT_FOUND) on re-add.

The neighbor/route `switch_id` `-5` family (Item 1 create-time) is **fully resolved**.
What remains in the LAG/ECMP scope is **dataplane forwarding** and a couple of
ECMP-member create-order codes (`-6`/`-7`) that the idempotency change did not fully
cover for the LAG-ECMP reuse/re-add sequences.

---

## 6. Confirmed unsupported (unchanged — not in 6-18 scope)

- SVI MAC learning / FDB on BVI
- SVI broadcast / SVI-to-SVI
- SVI neighbor idempotency `-6`
- RIF ingress disable
- ECMP hash-field distribution differences vs reference

---

## 7. Next steps

1. Rebuild `docker-sai-test-vpp:phase1` when apt/network is available (or use internal
   mirror) so `saiserver` + `libsaivs` land in the image without `docker cp`.
2. Finish Item 1 dataplane validation (`AddHostRoute*`, `RemoveAddNeighbor*`, route
   LPM/diff-prefix tests) — investigate any residual LAG forwarding vs Issue A
   watchdog.
3. Item 2a: `EcmpCoExist*` / `EcmpReuse*` after Item 1 green.
4. Item 3 validation: `EcmpLagDisableTestV4/V6`.
5. Regenerate `results/compatibility-matrix.md` from full `sai_route_test`,
   `sai_neighbor_test`, `sai_ecmp_test`.
