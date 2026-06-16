# sonic-sairedis VPP Code Review Notes

Status: **Reviews 1–7 and 9 implemented; Review 8 tracked as future.**

This document collects code-review feedback on the VPP `vslib` code, the
verification of each item against the current code, and the proposed approach for
addressing it. The "Suggested reply to the reviewer" sections below reflect the
final decisions and the changes that were actually made. Add new reviews as new
`## Review N` sections below.

## Index

- [Review 1: Route stats performance](#review-1-route-stats-performance----vslibvppvppxlatesairoutestatsc) — `vslib/vpp/vppxlate/SaiRouteStats.c`
- [Review 2: Nexthop group member duplication](#review-2-nexthop-group-member-refactor----vslibvppswitchvppnexthopcpp) — `vslib/vpp/SwitchVppNexthop.cpp`
- [Review 3: Redundant counter stats erase](#review-3-redundant-counter-stats-erase----vslibvppswitchvppcpp) — `vslib/vpp/SwitchVpp.cpp`
- [Review 4: Reserved context index magic number](#review-4-reserved-context-index-magic-number----vslibvppvppxlatesaivppxlatec) — `vslib/vpp/vppxlate/SaiVppXlate.c`
- [Review 5: Use SaiObjectDB for route/counter relations](#review-5-use-saiobjectdb-for-routecounter-relations----vslibvppswitchvpph) — `vslib/vpp/SwitchVpp.h`
- [Review 6: Extract route stats-index update into a helper](#review-6-extract-route-stats-index-update-into-a-helper----vslibvppswitchvppnexthopcpp) — `vslib/vpp/SwitchVppNexthop.cpp`
- [Review 7: Can a route already be bound to a different counter?](#review-7-can-a-route-already-be-bound-to-a-different-counter----vslibvppswitchvpproutecpp) — `vslib/vpp/SwitchVppRoute.cpp`
- [Review 8: Systematic rollback via transaction log (future)](#review-8-systematic-rollback-via-transaction-log-future----vslibvppswitchvpproutecpp) — `vslib/vpp/SwitchVppRoute.cpp`
- [Review 9: Failed route COUNTER_ID update could leave SaiObjectDB inconsistent](#review-9-failed-route-counter_id-update-could-leave-saiobjectdb-inconsistent----vslibvppswitchvpproutecpp-vslibvppswitchvppcpp) — `vslib/vpp/SwitchVppRoute.cpp`, `vslib/vpp/SwitchVpp.cpp`

---

## Review 1: Route Stats Performance — `vslib/vpp/vppxlate/SaiRouteStats.c`

### 1. Reviewer feedback (verbatim intent)

- For a device with `N` monitored routes and `R` total routes, the query
  function is called `N` times per polling cycle, and each call loops over all
  `R` routes, so the total work is `R * N`. There is a performance concern.
- The current design passes **one route index per call**, which makes a true
  batch update hard.
- Proposed mitigation: maintain a short-lived cache (e.g. 5 second expiration,
  or shorter) populated by a single `vpp_stats_dump` of *all* route stats. While
  the cache is valid, the getter reads from it; otherwise the getter triggers a
  cache refresh. This is expected to be more efficient with less change.

### 2. Verification against the current code

The concern is **correct**, and the real cost is slightly worse than `R * N`.

Call chain (per route counter OID, driven by the FlexCounter polling thread):

```
SwitchVpp::getStatsExt()            // called once per COUNTER OID per cycle  -> N times
  -> SwitchVpp::getRouteStatsExt()
    -> SwitchVpp::getRouteCounterStats()      // SwitchVpp.cpp:753
      -> vpp_route_stats_query(stats_index)   // SaiRouteStats.c:50
        -> vpp_stats_dump("/net/route/to", ...) // SaiVppStats.c:36
```

What `vpp_stats_dump` does on **every** call (`SaiVppStats.c`):

1. `stat_segment_connect_r(...)`        (line 46) — connect to the VPP stats socket
2. `stat_segment_ls_r(...)`             (line 59)
3. `stat_segment_dump_r(...)`           (line 68)
4. iterate **all** returned counters    (lines 76-143) and invoke the callback
   for every `(worker_thread k, index j)` pair
5. `stat_segment_disconnect_r(...)`     (line 146) — disconnect

`handle_stat_two` (`SaiRouteStats.c:35`) is invoked for every entry but keeps
only the one whose `index == query->stats_index` (filtering `R` down to 1). It
accumulates `+=` across VPP worker threads `k` for that single index.

Net cost per polling cycle of `N` route counters:

| Resource                         | Current cost per cycle |
| -------------------------------- | ---------------------- |
| VPP stats-socket connect/disconnect | `N`                 |
| `ls` + `dump` round trips        | `N`                    |
| Per-counter callback iterations  | `N * R`                |

So beyond the `N * R` iteration, we also pay `N` connect/disconnect handshakes
to the VPP stats segment, which is the more expensive part in practice.

### 3. Assessment of the proposed cache

The cache idea is sound and is the lowest-risk change. With a single refresh per
cycle the cost becomes:

| Resource                         | With cache (1 refresh/cycle) |
| -------------------------------- | ---------------------------- |
| VPP stats-socket connect/disconnect | `1`                       |
| `ls` + `dump` round trips        | `1`                          |
| Dump-population iterations       | `R` (once)                   |
| Per-counter getter cost          | `N` O(1) map lookups         |

This removes the `N`× connect/dump/disconnect amplification while keeping the
existing `vpp_route_stats_query` semantics for callers.

Importantly, the read-and-clear semantics do **not** clear VPP counters today:
`getRouteStatsExt` (`SwitchVpp.cpp:847`) implements clear via the
`m_routeCounterStatsBaseMap` / `m_routeCounterStatsCarryMap` bookkeeping, not by
mutating VPP. So caching the raw VPP dump does not interfere with clear-on-read.

### 4. Proposed approach: time-bounded cache of the full route-stats dump (reviewer's suggestion)

- Add a cache of `stats_index -> {packets, bytes}` plus a `last_refresh`
  timestamp.
- `vpp_route_stats_query` (or a thin wrapper) checks the cache: if fresh, serve
  from it; if stale/empty, do one full `vpp_stats_dump` that populates the entire
  map, then serve.
- TTL should be shorter than the FlexCounter polling interval so a cycle gets at
  most one refresh while avoiding cross-cycle staleness. Make the TTL a named
  constant (or configurable) rather than hard-coded magic.

Pros: minimal change, big win, no API change for callers.
Cons: introduces a wall-clock TTL; staleness bounded by TTL; needs a clear
ownership/locking story (see §5).

#### Placement

Put the cache where the route maps already live (`SwitchVpp`, alongside
`m_routeStatsIndexMap` / `m_counterToRouteMap` in `SwitchVpp.h:711-715`), since
that object owns the route/counter lifecycle and can invalidate on bind/unbind.
Keep `SaiRouteStats.c` focused on producing a full
`stats_index -> {packets, bytes}` map in a single pass.

### 5. Open questions / risks to resolve before coding

1. **Cache location**: C layer (`SaiRouteStats.c`, with a new "dump all" API) vs
   C++ layer (`SwitchVpp`). Leaning C++ for lifecycle/invalidation; needs a
   single-pass dump helper in C.
2. **Thread safety**: `vpp_stats_dump` uses a `__thread` client
   (`SaiVppStats.c:25`). Confirm whether route-counter getters run on a single
   FlexCounter thread or several. If shared cache state is added in `SwitchVpp`,
   decide on locking vs. thread-local caches.
3. **TTL value and source**: fixed constant vs. derived from the polling
   interval vs. configurable. Must be shorter than the poll interval to bound
   staleness while still collapsing a cycle into one refresh.
4. **Invalidation on route churn**: a full refresh naturally drops indices for
   removed routes and picks up new ones; confirm TTL-bounded staleness is
   acceptable around `setRouteCounterBinding` / `removeRouteCounterBinding`.
5. **Not-found semantics**: today a missing index returns `-ENOENT`
   (`SaiRouteStats.c:66`). Define behavior when an index is absent from a fresh
   full dump (treat as not-found vs. trigger a one-shot refresh).
6. **Multi-worker accumulation**: the full-dump helper must preserve the existing
   per-index `+=` accumulation across VPP worker threads `k` that
   `handle_stat_two` performs, so cached totals match today's values.
7. **Failure handling**: if a refresh fails (VPP socket down), decide whether to
   serve the last good cache, return an error, or both (e.g. serve-stale with a
   warning).

### 6. Suggested reply to the reviewer

The R×N concern is confirmed (and we also pay N connect/disconnect handshakes per
cycle). Done: added a single-pass `vpp_route_stats_dump_all` C API and a
short-lived full-dump cache so each polling cycle does one dump plus O(1) lookups.
Resolved decisions: the cache lives in `SwitchVpp` (C++) keyed by stats index;
TTL is a named constant `ROUTE_STATS_CACHE_TTL = 1s` (shorter than the poll
interval); access is guarded by a `std::mutex`. Only the polling path
(`getRouteStatsExt`) reads from the cache (`allow_cache=true`); config-path reads
stay exact. On a failed refresh the last good cache is served (stale) with a
warning; a missing index returns not-found.

---

## Review 2: Nexthop Group Member Refactor — `vslib/vpp/SwitchVppNexthop.cpp`

### 1. Reviewer feedback

`createNexthopGroupMember` / `removeNexthopGroupMember` have common code; it would
be better to wrap the shared logic into a helper function.

### 2. Verification against the current code

The feedback is **correct**. The two functions duplicate two substantial blocks
almost verbatim.

#### Duplicated block 1 — extract member info (`fillNHGrpMember`)

- `createNexthopGroupMember`: lines 298-316
- `removeNexthopGroupMember`: lines 390-408

Both do the same thing: read `SAI_NEXT_HOP_GROUP_MEMBER_ATTR_NEXT_HOP_ID`
(mandatory), read `SAI_NEXT_HOP_GROUP_MEMBER_ATTR_WEIGHT` (default `1`), call
`fillNHGrpMember(&member, next_hop_oid, weight, 0)`, and on failure log
`"Failed to fill NHG member info for %s"` and return the status. The blocks are
identical.

#### Duplicated block 2 — per-route path update loop

- `createNexthopGroupMember`: lines 318-360
- `removeNexthopGroupMember`: lines 422-463

This ~43-line loop is identical except for two things:

1. The add/remove flag passed to `IpRoutePathAddRemove(route.second.get(),
   &member, true|false, &stats_index)` (`true` in create, `false` in remove).
2. Log message wording: `"before adding path to route"` vs
   `"before removing path from route"`, and `"NHG member added. Adding path to
   route"` vs `"NHG member removed. Removing path from route"`.

Everything else (the `m_routeToCounterMap` lookup, reading old counter stats,
calling `IpRoutePathAddRemove`, updating `m_routeStatsIndexMap`, resetting the
counter base in `m_routeCounterStatsBaseMap`, and `carryRouteCounterStatsDelta`
handling) is byte-for-byte the same in both functions.

### 3. Proposed refactor

Extract two private helpers on `SwitchVpp`.

#### Helper A — per-route path update loop (primary win)

```cpp
// pseudocode signature
void SwitchVpp::updateRoutesForNhgMember(
        const std::unordered_map<std::string, std::shared_ptr<SaiObject>>& routes,
        const nexthop_grp_member_t& member,
        bool isAdd);
```

- Body is the shared loop (current lines 318-360 / 422-463).
- `isAdd` selects the `IpRoutePathAddRemove` flag and the log wording.
- By the time both callers reach this loop they already hold a `routes`
  collection and a filled `member`, so there is no entanglement with the
  object-acquisition differences described in §4. This is the cleanest, highest
  value extraction (removes ~43 duplicated lines).

#### Helper B — member-info extraction (secondary)

```cpp
// pseudocode signature
sai_status_t SwitchVpp::buildNhgMember(
        const SaiObject& nhgMbrObj,
        nexthop_grp_member_t& member);
```

- Body is the shared block 1 (current lines 298-316 / 390-408).
- `SaiCachedObject` (used by create) and `SaiDBObject` (returned by
  `get_sai_object` in remove) both derive from `SaiObject`
  (`vslib/vpp/SaiObjectDB.h:33,74,112`), and `get_mandatory_attr` / `get_attr`
  are declared on that base. So a helper taking `const SaiObject&` works for both
  call sites; the create caller passes `nhg_mbr_obj`, the remove caller passes
  `*nhg_mbr_obj`.

### 4. Differences to preserve (do NOT fold into the helpers)

These differ between the two functions and must stay in each caller:

1. **Object acquisition**: create builds a stack `SaiCachedObject` from
   `attr_list` and accesses via `.`; remove fetches via `get_sai_object(...)` and
   accesses via `->`. Only the shared parts (after obtaining a `SaiObject` /
   filled `member`) move into helpers.
2. **Internal create vs remove ordering**:
   - create: `create_internal` (line 288) is called *before* fetching routes and
     filling the member.
   - remove: routes are fetched (388) and the member is filled (390-408) *before*
     `remove_internal` (411).
   The helpers should be invoked at the points that keep this ordering intact;
   do not move `create_internal` / `remove_internal` into a helper.
3. **`routes == nullptr` early return** placement differs slightly (create checks
   right after `create_internal`; remove checks after `remove_internal`), but
   both return `SAI_STATUS_SUCCESS`. Keep these checks in the callers, guarding
   the call to Helper A.

### 5. Open questions before coding

1. **Helper scope**: extract both helpers, or only Helper A (the big win)?
   Helper B saves ~18 lines but introduces a base-class-reference parameter.
2. **Naming**: confirm preferred names (`updateRoutesForNhgMember`,
   `buildNhgMember`) and that they should be private members of `SwitchVpp`
   declared in `SwitchVpp.h`.
3. **Tests**: confirm whether existing VS unit/integration tests cover
   create/remove NHG member so we can verify no behavior change after the
   refactor.

### 6. Suggested reply to the reviewer

Confirmed: the per-route update loop (~43 lines) and the member-info extraction
block are duplicated. Done: factored the loop into
`updateRoutesForNhgMember(routes, member, isAdd)` and the extraction into
`buildNhgMember`, plus a third helper `recordRouteStatsIndexAndResetBase` for the
inner stats-index/base-reset block (see Review 6). The create/remove-specific
object acquisition and internal-DB call ordering stay in the callers, and the
duplicated `m_routeStatsIndexMap[...]` assignment was hoisted (behavior-preserving).

---

## Review 3: Redundant Counter Stats Erase — `vslib/vpp/SwitchVpp.cpp`

### 1. Reviewer feedback

Lines 1405/1406 are a redundant `erase` when the counter is bound.

The lines in question (inside the `SAI_OBJECT_TYPE_COUNTER` branch of
`removeObject`):

```cpp
auto routeIt = m_counterToRouteMap.find(objectId);
if (routeIt != m_counterToRouteMap.end())
{
    std::string route = routeIt->second;
    removeRouteCounterBinding(route);          // line 1403
}
m_routeCounterStatsBaseMap.erase(objectId);    // line 1405
m_routeCounterStatsCarryMap.erase(objectId);   // line 1406
```

### 2. Verification against the current code

The feedback is **correct** for the bound case.

`objectId` here is the counter OID. The two stats maps are keyed by counter OID
(`SwitchVpp.h:714-715`), and the route<->counter maps are kept bidirectionally
consistent: `setRouteCounterBinding` sets both directions together
(`SwitchVppRoute.cpp:278-279`), and `removeRouteCounterBinding` erases everything
for that counter together (`SwitchVppRoute.cpp:319-322`):

```cpp
m_counterToRouteMap.erase(counterIt->second);          // counterIt->second == counter OID
m_routeCounterStatsBaseMap.erase(counterIt->second);   // SwitchVppRoute.cpp:320
m_routeCounterStatsCarryMap.erase(counterIt->second);  // SwitchVppRoute.cpp:321
m_routeToCounterMap.erase(counterIt);
```

When the counter is **bound**, `routeIt->second` is the route, and because the
maps are consistent `m_routeToCounterMap[route] == objectId`. So inside
`removeRouteCounterBinding`, `counterIt->second == objectId`, and lines 320-321
already erase `m_routeCounterStatsBaseMap[objectId]` and
`m_routeCounterStatsCarryMap[objectId]`. Lines 1405-1406 then erase the same keys
again — a no-op (erasing an absent key does nothing), hence redundant.

When the counter is **not bound** (`routeIt == end`), `removeRouteCounterBinding`
is not called, so lines 1405-1406 are the only cleanup. In the normal flow an
unbound counter OID should have no base/carry entries (they are created only when
binding), so this is defensive cleanup rather than strictly required, but it is
the only path that touches the maps in the unbound case.

### 3. Proposed approach

Make the erase conditional on the unbound case so the bound path does not
re-erase what `removeRouteCounterBinding` already handled:

```cpp
auto routeIt = m_counterToRouteMap.find(objectId);
if (routeIt != m_counterToRouteMap.end())
{
    removeRouteCounterBinding(routeIt->second);   // already erases base/carry
}
else
{
    m_routeCounterStatsBaseMap.erase(objectId);   // defensive cleanup when unbound
    m_routeCounterStatsCarryMap.erase(objectId);
}
```

This is behavior-preserving: erasing an absent key is a no-op, so moving the two
erases into the `else` only drops the redundant lookups in the bound path while
keeping the defensive cleanup for the unbound path.

### 4. Open questions before coding

1. **Keep defensive cleanup?** If we can guarantee the invariant that an unbound
   counter OID never has base/carry entries, the `else` block could be dropped
   entirely. Leaning toward keeping it (cheap, defensive). Confirm preference.

### 5. Suggested reply to the reviewer

Confirmed: when the counter is bound, `removeRouteCounterBinding` already erases
`m_routeCounterStatsBaseMap` / `m_routeCounterStatsCarryMap` for the same counter
OID, so the two erases were redundant in that path. Done: moved them into an
`else` branch (unbound case, defensive cleanup), which was behavior-preserving.
Follow-up note: Review 5 later removed the route↔counter maps, so this
COUNTER-removal path is now a single flat base/carry erase by counter OID (the
bound/unbound branch collapsed, since the relationship lives in `SaiObjectDB`).

---

## Review 4: Reserved Context Index Magic Number — `vslib/vpp/vppxlate/SaiVppXlate.c`

### 1. Reviewer feedback

In `static uint32_t alloc_index()` the bare `return 0;` (line 476) uses a magic
number. Define a constant like `INVALID_INDEX` (value `0`) with a meaningful name
and add a one-line comment explaining that index 0 is reserved.

### 2. Verification against the current code

The feedback is **correct**, and the same magic `0` sentinel is used in several
related places, not only line 476. The context allocator reserves index 0 to mean
"invalid / no context":

- `alloc_index()` (`SaiVppXlate.c:466-477`): the search loop starts at `idx = 1`
  (line 468), implicitly reserving 0, and returns `0` on exhaustion (line 476).
- `store_ptr()` (`SaiVppXlate.c:479-490`): `if (idx == 0) return 0;`
  (lines 483-484) — checks the reserved index and returns `0` as the invalid
  *context*.
- `release_index()` (`SaiVppXlate.c:496`): `if (idx == 0 || idx >= VPP_MAX_CTX)`.
- `get_index_ptr()` (`SaiVppXlate.c:510`): `if (idx == 0 || idx >= VPP_MAX_CTX)`.
- Caller `ip_route_add_del_get_stats()` (`SaiVppXlate.c:2068,2071`):
  `uint32_t context = 0;` and `if (context == 0) { ... return -ENOMEM; }`.

Note index 0 and context 0 coincide: a context is
`(generation << VPP_CTX_GENERATION_SHIFT) | idx`, so index 0 with generation 0
encodes to context `0`. The single value `0` therefore serves as both the
"invalid index" and the "invalid context" sentinel.

The surrounding bounds already use named macros (`VPP_MAX_CTX`,
`VPP_CTX_INDEX_MASK`, `VPP_CTX_GENERATION_SHIFT` at `SaiVppXlate.c:386-388`), so
the reserved `0` is the remaining magic number in this allocator.

### 3. Proposed approach

Add one named constant near the existing context macros (`SaiVppXlate.c:386-388`)
with a one-line comment, and replace every reserved-`0` site listed in §2 so we
do not leave magic numbers behind:

```c
/* Context index 0 is reserved to mean "no/invalid context"; valid indices are 1..VPP_MAX_CTX-1. */
#define VPP_INVALID_CTX_INDEX 0
```

Replacements (each currently a literal `0`):

- `alloc_index()`: `return VPP_INVALID_CTX_INDEX;` (line 476). Keep the loop start
  at `idx = 1`; optionally add a short comment there noting index 0 is reserved.
- `store_ptr()`: `if (idx == VPP_INVALID_CTX_INDEX) return VPP_INVALID_CTX_INDEX;`
  (lines 483-484).
- `release_index()` / `get_index_ptr()`:
  `if (idx == VPP_INVALID_CTX_INDEX || idx >= VPP_MAX_CTX)` (lines 496, 510).
- Caller `ip_route_add_del_get_stats()`:
  `uint32_t context = VPP_INVALID_CTX_INDEX;` and
  `if (context == VPP_INVALID_CTX_INDEX)` (lines 2068, 2071).

### 4. Decisions (resolved)

1. **Constant name**: use `VPP_INVALID_CTX_INDEX` (matches the existing
   `VPP_*_CTX*` macro naming at `SaiVppXlate.c:386-388`).
2. **Scope of replacement**: replace **all** reserved-`0` sites listed in §2 and
   §3, not just `alloc_index()` line 476, so no magic `0` remains.
3. **One constant vs two**: use the single `VPP_INVALID_CTX_INDEX` for both the
   invalid-index and invalid-context sentinels (they are both `0`); no separate
   `VPP_INVALID_CONTEXT` alias.

### 5. Suggested reply to the reviewer

Confirmed. Done: added `#define VPP_INVALID_CTX_INDEX 0` with a one-line comment
that index 0 is reserved, and replaced the reserved-`0` literals across the
allocator (`alloc_index`, `store_ptr`, `release_index`, `get_index_ptr`) and the
route caller's invalid-context checks. Scope note: the other `store_ptr(...) == 0`
checks in unrelated handlers (ACL, tunterm, link-state, bridge, sub-interface)
were left untouched as out-of-scope pre-existing code; they can adopt the constant
in a follow-up for full uniformity.

---

## Review 5: Use SaiObjectDB for route/counter relations — `vslib/vpp/SwitchVpp.h`

### 1. Reviewer feedback

Don't maintain the object-to-object maps individually; use `SaiObjectDB`. Add a
second child relation to `SAI_OBJECT_TYPE_ROUTE_ENTRY` in
`sai_child_relation_defs` (`SaiObjectDB.cpp`):

```cpp
{SAI_OBJECT_TYPE_ROUTE_ENTRY,
    {{SAI_OBJECT_TYPE_NEXT_HOP_GROUP, SAI_ROUTE_ENTRY_ATTR_NEXT_HOP_ID, SAI_ATTR_VALUE_TYPE_OBJECT_ID},
     {SAI_OBJECT_TYPE_COUNTER,        SAI_ROUTE_ENTRY_ATTR_COUNTER_ID,  SAI_ATTR_VALUE_TYPE_OBJECT_ID}}},  // second entry is new
```

Forward lookup (route -> counter), replacing `m_routeToCounterMap`:

```cpp
auto route_obj = m_object_db.get(SAI_OBJECT_TYPE_ROUTE_ENTRY, serializedRouteId);
auto linked_counter = route_obj->get_linked_object(SAI_OBJECT_TYPE_COUNTER, SAI_ROUTE_ENTRY_ATTR_COUNTER_ID);
```

Reverse lookup (counter -> routes), replacing `m_counterToRouteMap`:

```cpp
auto counter_obj = m_object_db.get(SAI_OBJECT_TYPE_COUNTER, sai_serialize_object_id(counter_oid));
auto routes = counter_obj->get_child_objs(SAI_OBJECT_TYPE_ROUTE_ENTRY);
```

### 2. Verification against the current code

The suggestion is **sound and consistent with the existing design**. `SaiObjectDB`
exists precisely to model SAI object relationships (`SaiObjectDB.h:232-240`), and
the infrastructure to populate/maintain a COUNTER relation is already wired in:

- `create_or_update` is already called on create (`SwitchVpp.cpp:1300`), on set
  (`SwitchVpp.cpp:1759`, `is_create=false`), and `remove` on delete
  (`SwitchVpp.cpp:1569`). So adding the COUNTER child relation would be populated
  and torn down automatically as routes are created/set/removed with
  `SAI_ROUTE_ENTRY_ATTR_COUNTER_ID`.
- The route `SAI_ROUTE_ENTRY_ATTR_COUNTER_ID` set handler
  (`SwitchVppRoute.cpp:579-613`) goes through `set_internal`
  (`SwitchVppRoute.cpp:603` -> `SwitchVpp.cpp:1751`), which calls
  `create_or_update`. So binding via set maintains the relation.
- The forward and reverse helpers the reviewer cites already exist:
  `SaiObject::get_linked_object` (`SaiObjectDB.cpp:295`) and
  `SaiDBObject::get_child_objs` (`SaiObjectDB.h:147`), reached via
  `m_object_db.get` (`SaiObjectDB.cpp:265`). The member is `m_object_db`
  (`SwitchVpp.h:472`).
- The existing NHG path already consumes this same mechanism
  (`get_child_objs(SAI_OBJECT_TYPE_ROUTE_ENTRY)` on the NHG parent, used in
  `SwitchVppNexthop.cpp`), so the COUNTER parent would behave identically.

So the two **object-to-object** maps can be replaced:

| Current map                          | SaiObjectDB replacement                                      |
| ------------------------------------ | ----------------------------------------------------------- |
| `m_routeToCounterMap` (route->counter) | `route_obj->get_linked_object(COUNTER, ..._COUNTER_ID)`     |
| `m_counterToRouteMap` (counter->route) | `counter_obj->get_child_objs(SAI_OBJECT_TYPE_ROUTE_ENTRY)`  |

### 3. Scope: what can and cannot move

Only 2 of the 5 maps added in `SwitchVpp.h:711-715` are object relationships. The
other 3 are VPP/runtime accounting state with no SAI-object-graph equivalent and
must stay:

- `m_routeStatsIndexMap` (route -> VPP stats index): backend VPP state, not a SAI
  relation.
- `m_routeCounterStatsBaseMap` (counter -> base snapshot): runtime counter
  accounting for delta computation.
- `m_routeCounterStatsCarryMap` (counter -> carried delta): runtime counter
  accounting across rebinding/clear.

So this review migrates the **relationship** maps only; the stats/index maps
remain (though they can be re-keyed independently of this change).

### 4. Caveats to resolve before coding

1. **1:1 vs 1:many semantics.** `m_counterToRouteMap` is 1:1 today, and
   `setRouteCounterBinding` enforces it by returning `SAI_STATUS_OBJECT_IN_USE`
   when a counter is already bound to a different route
   (`SwitchVppRoute.cpp:267-275`, also checked at `399-401`). `get_child_objs` is
   inherently 1:many. The migration must reconstruct the 1:1 guard from
   `get_child_objs` (e.g. reject if a non-empty child set points at a different
   route), or consciously relax the invariant. Consumers that assume a single
   route (`getRouteCounterStats`, the COUNTER-removal path) depend on this.
2. **Update ordering.** Today `setRouteCounterBinding` updates the maps at
   `SwitchVppRoute.cpp:597`, *before* `set_internal` (line 603) writes the
   attribute and calls `create_or_update` (line 1759). If the relationship moves
   into the DB, the reverse-lookup child set only reflects the binding after
   `create_or_update` runs. Any logic that currently reads the maps between those
   points must be re-sequenced so it reads the relation after it is updated.
3. **`get_linked_object` reads the live attribute.** It calls `get_attr` ->
   `m_switch_db->get` (`SaiObjectDB.cpp:295-321`, `SaiDBObject::get_attr` at
   `393`). The route's `SAI_ROUTE_ENTRY_ATTR_COUNTER_ID` must already be persisted
   in switch state at lookup time; confirm this holds at each call site that
   currently uses `m_routeToCounterMap`.
4. **Rollback paths.** `setRouteCounterBinding` has explicit rollback that saves
   and restores the old counter's base/carry on failure
   (`SwitchVppRoute.cpp:244-300`). The relationship rollback would shift onto the
   DB's `create_or_update`/`remove`, but the base/carry rollback still needs the
   stats maps. Keep these two concerns separate during the migration.
5. **`get_child_objs` may return `nullptr`.** When a counter has no bound routes
   the helper returns `nullptr` (`SaiObjectDB.h:147-154`); callers must
   null-check, unlike `map::find` on an empty map.

### 5. Decisions (resolved)

1. **Scope of this change**: migrated only `m_routeToCounterMap` /
   `m_counterToRouteMap` to `SaiObjectDB`; the 3 stats/index maps
   (`m_routeStatsIndexMap`, `m_routeCounterStatsBaseMap`,
   `m_routeCounterStatsCarryMap`) remain as VPP/runtime accounting state.
2. **1:1 invariant**: preserved. `SAI_STATUS_OBJECT_IN_USE` is reconstructed by
   inspecting `get_child_objs` (reject when a non-empty child set points at a
   different route).
3. **Sequencing**: resolved without restructuring the rollback flow. The binding
   reads (old-counter capture, `OBJECT_IN_USE`) stay *before* the
   `create_internal`/`set_internal` commit, where they correctly observe the old
   committed state. The bind-time base snapshot now reads the route's already-known
   VPP stats index directly (`readRouteStatsByIndex`) instead of the
   counter→route reverse lookup, so it does not depend on the new relation being
   committed yet. Cleanup that previously ran after the route was torn down
   (`removeIpRoute`) or after a failed `create_internal` now captures/erases by
   counter OID before the relation disappears.

### 6. Suggested reply to the reviewer

Done. Added the `{COUNTER, SAI_ROUTE_ENTRY_ATTR_COUNTER_ID, OBJECT_ID}` child
relation in `sai_child_relation_defs` and removed `m_routeToCounterMap` /
`m_counterToRouteMap`, replacing them with three helpers:
`getRouteBoundCounter` (`get_linked_object`, route→counter), `getCounterBoundRoute`
(`get_child_objs`, counter→route, 1:1), and `readRouteStatsByIndex`. The three
stats/index maps stay as accounting state. The 1:1 counter↔route invariant
(`SAI_STATUS_OBJECT_IN_USE`) is preserved via `get_child_objs` inspection. The
relation is committed only inside `create_internal`/`set_internal`, so all binding
reads run before commit (observing old/other state) and the base snapshot uses the
route's stats index directly — no rollback restructuring was needed. This also
collapses Review 3's COUNTER-removal branch into a single flat base/carry erase.
One minor follow-up: `get_linked_object` logs at `NOTICE` when a route has no
counter, which adds some control-plane log volume; can be guarded later if noisy.
Not built/tested locally (needs swsscommon/VPP toolchain); validate in CI/VS.

---

## Review 6: Extract route stats-index update into a helper — `vslib/vpp/SwitchVppNexthop.cpp`

### 1. Reviewer feedback

The `else if (stats_index != UINT32_MAX) { ... }` block is a good candidate to
make into a function: it improves reusability and is more readable with a clear
intent.

### 2. Verification against the current code

The feedback is **correct**. The block
(`SwitchVppNexthop.cpp:339-359` in `createNexthopGroupMember`, identical at
`442-462` in `removeNexthopGroupMember`) does one cohesive thing: after a path
add/remove returns a valid `stats_index`, record the route's stats index and, if
the route is bound to a counter, re-read the counter to reset its base (carrying
the pre-change stats forward as a delta, or erasing the base on failure).

It is duplicated verbatim between the two NHG functions, and a closely related
"carry old delta then store new base" idiom also appears in the route set flow
(`SwitchVppRoute.cpp:709-716` and `740-747`), so a named helper has reuse value
beyond the two call sites the reviewer pointed at.

This review **overlaps with Review 2**. Review 2 proposes wrapping the whole
per-route loop into `updateRoutesForNhgMember(routes, member, isAdd)`; this block
is the inner body of that loop. The two are complementary: extracting this block
is useful on its own and would also live inside the Review 2 helper. They should
be planned together to avoid conflicting refactors.

### 3. Proposed approach

Extract the block into a private `SwitchVpp` helper. The block does **not** depend
on the add/remove direction (only the surrounding log wording does), so no
`isAdd` parameter is needed:

```cpp
// pseudocode signature
void SwitchVpp::recordRouteStatsIndexAndResetBase(
        const std::string& route,
        uint32_t stats_index,
        bool has_old_counter_stats,
        const std::map<sai_stat_id_t, uint64_t>& old_counter_stats);
```

- Body is the current block (`SwitchVppNexthop.cpp:339-359`), minus the outer
  `else if (stats_index != UINT32_MAX)` guard (the caller keeps the guard, or the
  helper early-returns when `stats_index == UINT32_MAX`).
- The helper re-looks-up the counter via `m_routeToCounterMap.find(route)`; the
  binding does not change within the loop, so this is equivalent to reusing the
  caller's `counterIt`.
- Both `createNexthopGroupMember` and `removeNexthopGroupMember` then call this
  helper, removing the duplication.

Optionally, factor the innermost "carry old delta, then store/erase the new
counter base" tail (`SwitchVppNexthop.cpp:348-357`) into a smaller helper, e.g.:

```cpp
void SwitchVpp::resetRouteCounterBase(
        sai_object_id_t counter_oid,
        bool has_old_counter_stats,
        const std::map<sai_stat_id_t, uint64_t>& old_counter_stats,
        const std::map<sai_stat_id_t, uint64_t>* new_counter_base /* nullptr => erase */);
```

This smaller kernel is the part also reusable in `SwitchVppRoute.cpp:709-716` /
`740-747`, giving the reviewer's "reusability" point its widest reach. The route
flow there has different surrounding error handling (rollback), so only this tail
is shared, not the whole block.

### 4. Behavior-preserving simplification to note

Inside the block, `m_routeStatsIndexMap[route.first] = stats_index;` is assigned in
**both** branches (`SwitchVppNexthop.cpp:341` and `343`). It can be hoisted above
the `if (counterIt == end)` check so it runs once. This is behavior-preserving and
would make the helper read more clearly, but it is a small change beyond a pure
extraction, so call it out rather than fold it in silently.

### 5. Open questions

1. **Coordinate with Review 2**: do this as part of `updateRoutesForNhgMember`
   (Review 2), or as a standalone helper first? Recommend planning both together;
   the loop extraction (Review 2) plus this inner helper compose cleanly.
2. **One helper or two**: extract just the block (Helper above), or also the
   smaller `resetRouteCounterBase` tail that extends reuse to `SwitchVppRoute.cpp`?
3. **Naming**: confirm preferred helper name(s).

### 6. Suggested reply to the reviewer

Confirmed: the `else if (stats_index != UINT32_MAX)` block is identical in the NHG
create/remove functions, so a helper improves both readability and reuse. Done:
extracted it as `recordRouteStatsIndexAndResetBase`, which is the inner body of
Review 2's `updateRoutesForNhgMember` loop (the two were planned together, not as
competing refactors). The duplicated `m_routeStatsIndexMap[...]` assignment was
hoisted (behavior-preserving). Decision: we did **not** extract the smaller
`resetRouteCounterBase` tail into `SwitchVppRoute.cpp`'s route set flow — its
surrounding rollback differs, so folding it in was out of scope for this change.

---

## Review 7: Can a route already be bound to a different counter? — `vslib/vpp/SwitchVppRoute.cpp`

### 1. Reviewer feedback

On `auto oldCounterIt = m_routeToCounterMap.find(serializedObjectId);`
(`SwitchVppRoute.cpp:249`) in `setRouteCounterBinding()`: "can this really happen
that a route is bound with another counter id?" i.e. is the old-counter capture
and the rollback-restore that depends on it (lines 244-265, 286-298) actually
reachable?

### 2. Analysis

`setRouteCounterBinding(route, counter_oid)` has two real callers (a third is its
own rollback):

- **Create path** — `createIpRoute` (`SwitchVppRoute.cpp:505`). The route is being
  created fresh, so `m_routeToCounterMap` has no entry for it yet: `oldCounterIt
  == end()` and `old_counter_oid` stays `SAI_NULL_OBJECT_ID`. For this caller the
  "route already bound to another counter" case is effectively **unreachable**.
- **Set-attribute path** — `SAI_ROUTE_ENTRY_ATTR_COUNTER_ID` handler
  (`SwitchVppRoute.cpp:597`). Here the route already exists and may already be
  bound. This is the only path where `oldCounterIt` can be non-`end()`.

Within the set path there are three sub-cases for `old_counter_oid` vs the new
`counter_oid`:

1. **old == NULL** (route was unbound): normal `NULL -> B` bind. Old-counter
   branch not taken.
2. **old == counter_oid** (same counter re-set, `A -> A`): `oldCounterIt` is
   non-`end()`, so the save/restore code **does** run. The OBJECT_IN_USE check at
   `267-269` is skipped because `oldRouteIt->second == serializedObjectId`. Line
   `277` unbinds then `278-279` re-bind the same counter; the save/restore exists
   so that if `getRouteCounterStats` fails at `282`, the route's prior base/carry
   are restored (`286-298`). So this branch is **reachable and useful** here.
3. **old != counter_oid, both non-NULL** (`A -> B` direct rebind): this is the
   exact case the reviewer questions. It is **legal at the SAI layer**
   (`set_attribute(COUNTER_ID=B)` on a route bound to A, with no intervening
   NULL), so vslib must not corrupt state if it arrives. Whether SONiC actually
   produces it is the open question (see §3).

So the honest answer: a route being bound to a *different* counter can only arrive
via the set-attribute path, sub-case (3). The `oldCounterIt` capture is **not dead
code** regardless, because sub-case (2) (same-counter re-set) also exercises the
save/restore for rollback safety. In the create path it is effectively dead.

### 3. The real question: does SONiC ever issue a direct A -> B rebind?

This depends on the SAI client (sonic-swss `FlowCounterRouteOrch`), which lives in
a different submodule and was not inspected here. The typical flow-counter
lifecycle is `NULL <-> counter` (each route gets its own counter object that is
removed when unbound), which would make sub-case (3) never occur in practice. We
should **confirm** that orchagent does not change a route's counter from one
non-NULL object to another without an intervening NULL before relying on it.

Note this also intersects **Review 5**: if the route<->counter relation moves into
`SaiObjectDB`, the same 1:1-vs-rebind question governs how `OBJECT_IN_USE`
(`267-275`) is reconstructed from `get_child_objs`.

### 4. Recommendation

Do **not** remove the old-counter capture/restore: sub-case (2) needs it for
rollback, and sub-case (3) is SAI-legal. Options, in preference order:

1. **Keep, add a clarifying comment** at line 249 explaining that the old counter
   is captured for rollback on re-bind, that `A -> B` is SAI-legal but not
   expected from current SONiC, and that the create path never hits it.
2. **Make the unexpected case observable**: if `old_counter_oid != SAI_NULL_OBJECT_ID
   && old_counter_oid != counter_oid`, log a warning ("unexpected direct rebind
   A -> B") so we learn if a client ever does it, without changing behavior.
3. Only if orchagent is confirmed to never rebind A -> B *and* never re-set the
   same counter, consider simplifying. This seems unlikely to be worth it and
   would remove rollback safety, so not recommended.

### 5. Suggested reply to the reviewer

Good question. It cannot happen from the create path (fresh route, no prior
binding). It can only happen via the `SAI_ROUTE_ENTRY_ATTR_COUNTER_ID` set path,
and a *different* counter (A -> B) only if a client issues a direct rebind without
setting NULL first, which is SAI-legal but not expected from current SONiC
`FlowCounterRouteOrch` (typically NULL<->counter). The old-counter capture is
still needed because the same-counter re-set (A -> A) path uses it for rollback if
the new stats read fails. Done: kept the capture, added a clarifying comment, and
log a warning if an unexpected A -> B rebind is ever seen. (After Review 5 the old
counter is read via `getRouteBoundCounter` instead of the map, but the behavior is
the same.) Still worth confirming orchagent's behavior.

---

## Review 8: Systematic rollback via transaction log (future) — `vslib/vpp/SwitchVppRoute.cpp`

### 1. Reviewer feedback

On `rollbackRoute(route_db_obj.get(), NULL, old_stats_index)`
(`SwitchVppRoute.cpp:657`): the current implementation has lots of ad-hoc rollback
code and would benefit from a more systematic approach. Idea: a transaction log
where every VPP API call has a matching reverse operation; on each successful
call, enqueue the reverse op with the parameters needed to undo it (and likewise
for the internal mappings). For the `SaiObjectDB` map, defer the write to the end
so nothing is committed until everything succeeds. Flagged explicitly as a
**future** consideration.

### 2. Verification: current rollback is ad-hoc and scattered

The observation is **correct**. Within route handling alone, undo logic is spread
across several hand-written patterns, each responsible for remembering and
reversing its own steps:

- `removeProgrammedRoute` lambda (`SwitchVppRoute.cpp:445-458`): undoes the VPP
  program step in `createIpRoute`.
- `rollbackRoute` lambda (`SwitchVppRoute.cpp:537-577`): re-programs the old route
  and restores `m_routeStatsIndexMap` in the set path.
- Manual map erase/restore interleaved with those calls in `createIpRoute`
  (`508-525`): `m_routeStatsIndexMap.erase`, `removeRouteCounterBinding`, etc.,
  repeated per failure point.
- `setRouteCounterBinding` save/restore (`244-300`): manually snapshots the old
  counter base/carry and restores them if the new stats read fails.
- The `SAI_ROUTE_ENTRY_ATTR_COUNTER_ID` set path (`646-718`): multiple
  `rollbackRoute(...)` call sites, each followed by `if (rollback_status != ...)
  return rollback_status;`.

Issues with the current style:
- The set of steps to undo is re-derived by hand at every early-return, which is
  easy to get wrong (miss a map, wrong order).
- Ordering is implicit; correct undo must be LIFO, but that is enforced only by
  reading the code.
- VPP program changes and in-memory map mutations are committed eagerly, so a
  later failure forces explicit compensation rather than simply not committing.
- Rollback itself can fail (e.g. `555-562`), and the code then logs and returns
  the rollback status, leaving state partially inconsistent.

### 3. Assessment of the proposal

The transaction-log / compensating-action (undo-stack) pattern is a sound,
well-understood fit:

- Each successful mutating step pushes a reverse op (with captured args) onto a
  stack; on failure, pop and execute in LIFO order; on full success, discard the
  stack (commit).
- In C++ this is naturally a scoped guard (RAII): a `ScopeGuard`/transaction
  object that runs queued compensations in its destructor unless `commit()` was
  called. This removes the repetitive `if (rollback_status != ...) return;`
  plumbing.
- Deferring the `SaiObjectDB` write to the end (as the reviewer notes) means the
  authoritative object graph is only updated once all VPP and bookkeeping steps
  succeed, shrinking the window where compensation is needed.

Honest caveats:
- It does not solve "the compensating action itself fails" (VPP rejects the
  reverse call). That remains a best-effort log-and-continue situation; a
  transaction log makes it consistent and centralized but not infallible.
- Capturing the exact reverse parameters for every VPP API is real work and must
  be kept in sync as APIs change.
- This is **cross-cutting**, not route-specific: nexthop-group, tunnel, ACL, etc.
  have the same pattern, so a transaction abstraction ideally lives at a common
  layer (e.g. around `create_internal` / `set_internal` / VPP xlate calls), which
  is a sizable design effort.

This intersects **Review 5** (the "defer SaiObjectDB commit to the end" point is
exactly how a DB-backed relation should participate in the transaction) and
**Reviews 2/6** (a centralized undo would absorb the per-route counter-base
restore those reviews extract).

### 4. Recommendation

Agree with the direction; treat it as a **separate future design item**, not part
of the current PR:

1. Do not block the current change on it. Keep the existing explicit rollback for
   now (it is correct, if verbose).
2. Track it as a design note / RFC describing: the undo-stack/RAII transaction
   guard, the rule "every mutating VPP call and map write enqueues its reverse",
   and the "commit SaiObjectDB last" policy.
3. Optional incremental step that fits today's code: introduce a small RAII
   `ScopeGuard` and convert one function (e.g. the COUNTER_ID set path) to it as a
   proof of concept, before committing to a switch-wide abstraction.

### 5. Suggested reply to the reviewer

Agree, and the current rollback is indeed scattered (the `removeProgrammedRoute` /
`rollbackRoute` lambdas plus per-return manual map restores and the
`setRouteCounterBinding` save/restore). An undo-stack / RAII transaction guard
that enqueues a reverse op per successful mutation and commits the `SaiObjectDB`
write last is a good model, and it lines up with Review 5's "defer DB commit"
point. Since this is cross-cutting (nexthop, tunnel, ACL have the same pattern)
and sizable, we propose tracking it as a separate future design item rather than
in this PR, optionally prototyping a `ScopeGuard` on one set path first. We will
also keep in mind that a failing compensating action still needs a defined
policy.

---

## Review 9: Failed route COUNTER_ID update could leave SaiObjectDB inconsistent — `vslib/vpp/SwitchVppRoute.cpp`, `vslib/vpp/SwitchVpp.cpp`

### 1. Reviewer feedback (verbatim intent)

Medium. In the `SAI_ROUTE_ENTRY_ATTR_COUNTER_ID` path, `setRouteCounterBinding()`
updates base/carry accounting before `set_internal()` commits the new attribute.
If `set_internal()` fails, rollback calls
`setRouteCounterBinding(serializedObjectId, old_counter_oid)`. But `set_internal()`
mutates `m_object_db` via `create_or_update(...)` before checking whether the route
exists and before updating `m_objectHash`. Since `setRouteCounterBinding()` no
longer writes the route/counter relation (post-Review 5), the rollback restores
only accounting state and can leave `SaiObjectDB` still pointing at the new
counter. Later `getCounterBoundRoute()` / route counter stats can resolve the
wrong route-counter relationship. Suggest restoring the `SaiObjectDB` relation
during rollback, or moving/deferring the `create_or_update()` relation mutation
until after the internal set is known to succeed.

### 2. Verification against the current code

Confirmed. In `set_internal()` (`SwitchVpp.cpp`) `m_object_db.create_or_update(..., is_create=false)`
ran first. For a `COUNTER_ID` change `create_or_update` calls
`remove_child_from_parent` and re-adds the route under the new counter
(`SaiObjectDB.cpp`), so the relation is fully switched *before* the
`m_objectHash` existence check. If that check then returned
`SAI_STATUS_ITEM_NOT_FOUND`, the COUNTER_ID rollback
(`setRouteCounterBinding(serializedObjectId, old_counter_oid)` in
`SwitchVppRoute.cpp`) restored only `m_routeCounterStatsBaseMap`/`CarryMap`; the
route stayed a child of the new counter, and `getCounterBoundRoute()` would then
resolve the wrong pair.

Reachability is essentially nil in normal operation: `set_internal()`'s only
failure mode is `ITEM_NOT_FOUND`, which means the route is absent from
`m_objectHash`, yet the COUNTER_ID handler is only reached when updating an
*existing* route (always present in `m_objectHash`). So this was a latent /
defensive inconsistency rather than an active bug. It is still worth closing at
the root because the fix is cheap and removes a silent-corruption footgun for any
future caller that routes a non-existent object through `set_internal()`.

### 3. Decision (resolved)

Took the reviewer's second option (defer the relation mutation) at the root rather
than papering over it in the route-specific rollback. Reordered `set_internal()`
so the `m_objectHash` existence check runs *before* `create_or_update()`:

```cpp
auto it = m_objectHash.at(objectType).find(serializedObjectId);
if (it == m_objectHash.at(objectType).end()) { /* log */ return SAI_STATUS_ITEM_NOT_FOUND; }
m_object_db.create_or_update(objectType, serializedObjectId, 1, attr, false /*is_create*/);
```

This makes `set_internal()` atomic with respect to the relation: a failed set no
longer mutates `SaiObjectDB`, so no rollback of the relation is needed. It applies
to every object type that flows through `set_internal()`, not just routes. The
success path is unchanged — only the `ITEM_NOT_FOUND` path stops touching the
relation, and updating a parent/child relation for an object that does not exist
was already meaningless. `create_or_update` only updates parent/child tracking
(no other side effects), so the reorder is safe.

Chose this over restoring the relation inside the route rollback (option 1) because
that is messier, route-specific, and the compensating `create_or_update` could
itself fail.

### 4. Suggested reply to the reviewer

Good catch — confirmed. `set_internal()` committed the relation via
`create_or_update()` before the existence check, and post-Review 5
`setRouteCounterBinding()` no longer restores that relation, so a failed
`COUNTER_ID` set could leave `SaiObjectDB` pointing at the new counter. Fixed at
the root by reordering `set_internal()` to validate `m_objectHash` membership
*before* mutating the relation, so a failed set never touches `SaiObjectDB` (for
any object type) and no relation rollback is required. In practice the failure was
unreachable for an existing route (`set_internal()` only fails with
`ITEM_NOT_FOUND`), so this is hardening rather than a live-bug fix, but it is cheap
and removes the footgun.
