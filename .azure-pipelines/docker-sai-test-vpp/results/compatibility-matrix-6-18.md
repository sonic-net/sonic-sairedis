# VPP SAI Compatibility Matrix

_Generated: 2026-06-18 12:34:09 PDT_

## Summary

| Result | Count | % |
|--------|------:|----:|
| ✅ PASS | 27 | 28.4% |
| ❌ FAIL | 55 | 57.9% |
| ⚠️ ERROR | 11 | 11.6% |
| ⏭️ SKIP | 2 | 2.1% |
| **Total** | **95** | **100.0%** |

## Legend

**Result status**

| Icon | Meaning |
|------|---------|
| ✅ PASS | test passed |
| ❌ FAIL | assertion failed (e.g. expected packet not received) |
| ⚠️ ERROR | test errored out (exception / setUp / tearDown) |
| ⏭️ SKIP | test was skipped |

**SAI status codes** (see `SAI/inc/saistatus.h`) — appear in the Detail column

| Code | Symbol |
|------|--------|
| `0` | `SAI_STATUS_SUCCESS` |
| `-1` | `SAI_STATUS_FAILURE` |
| `-2` | `SAI_STATUS_NOT_SUPPORTED` |
| `-3` | `SAI_STATUS_NO_MEMORY` |
| `-4` | `SAI_STATUS_INSUFFICIENT_RESOURCES` |
| `-5` | `SAI_STATUS_INVALID_PARAMETER` |
| `-6` | `SAI_STATUS_ITEM_ALREADY_EXISTS` |
| `-7` | `SAI_STATUS_ITEM_NOT_FOUND` |
| `-8` | `SAI_STATUS_BUFFER_OVERFLOW` |
| `-9` | `SAI_STATUS_INVALID_PORT_NUMBER` |
| `-10` | `SAI_STATUS_INVALID_PORT_MEMBER` |
| `-11` | `SAI_STATUS_INVALID_VLAN_ID` |
| `-12` | `SAI_STATUS_UNINITIALIZED` |
| `-13` | `SAI_STATUS_TABLE_FULL` |
| `-14` | `SAI_STATUS_MANDATORY_ATTRIBUTE_MISSING` |
| `-15` | `SAI_STATUS_NOT_IMPLEMENTED` |
| `-16` | `SAI_STATUS_ADDR_NOT_FOUND` |
| `-17` | `SAI_STATUS_OBJECT_IN_USE` |

## Results

| Module | Test Class | Result | Detail |
|--------|------------|--------|--------|
| `sai_ecmp_test.EcmpCoExistLagRouteV4` | `runTest` | ❌ FAIL | One of the expected packets was received on device 0 on an unexpected port: 0 == |
| `sai_ecmp_test.EcmpCoExistLagRouteV6` | `runTest` | ❌ FAIL | Did not receive expected packet on any of ports for device 0. ========== RECEIVE |
| `sai_ecmp_test.EcmpHashFieldDportTestV4` | `runTest` | ❌ FAIL | Did not receive expected packet on any of ports for device 0. ========== RECEIVE |
| `sai_ecmp_test.EcmpHashFieldDportTestV6` | `runTest` | ❌ FAIL | Did not receive expected packet on any of ports for device 0. ========== RECEIVE |
| `sai_ecmp_test.EcmpHashFieldProtoTestV4` | `runTest` | ❌ FAIL | Did not receive expected packet on any of ports for device 0. ========== RECEIVE |
| `sai_ecmp_test.EcmpHashFieldProtoTestV6` | `runTest` | ❌ FAIL | Did not receive expected packet on any of ports for device 0. ========== RECEIVE |
| `sai_ecmp_test.EcmpHashFieldSIPTestV4` | `runTest` | ❌ FAIL | Did not receive expected packet on any of ports for device 0. ========== RECEIVE |
| `sai_ecmp_test.EcmpHashFieldSIPTestV6` | `runTest` | ❌ FAIL | Did not receive expected packet on any of ports for device 0. ========== RECEIVE |
| `sai_ecmp_test.EcmpHashFieldSportTestV4` | `runTest` | ❌ FAIL | Did not receive expected packet on any of ports for device 0. ========== RECEIVE |
| `sai_ecmp_test.EcmpHashFieldSportTestV6` | `runTest` | ❌ FAIL | Did not receive expected packet on any of ports for device 0. ========== RECEIVE |
| `sai_ecmp_test.EcmpIngressDisableTestV4` | `runTest` | ❌ FAIL | -1 != 0 |
| `sai_ecmp_test.EcmpIngressDisableTestV6` | `runTest` | ❌ FAIL | -1 != 0 |
| `sai_ecmp_test.EcmpLagDisableTestV4` | `runTest` | ✅ PASS |  |
| `sai_ecmp_test.EcmpLagDisableTestV6` | `runTest` | ✅ PASS |  |
| `sai_ecmp_test.EcmpLagTwoLayersWithDiffHashOffsetTestV4` | `runTest` | ❌ FAIL | Did not receive expected packet on any of ports for device 0. ========== RECEIVE |
| `sai_ecmp_test.EcmpLagTwoLayersWithDiffHashOffsetTestV6` | `runTest` | ❌ FAIL | Did not receive expected packet on any of ports for device 0. ========== RECEIVE |
| `sai_ecmp_test.EcmpReuseLagRouteV4` | `runTest` | ❌ FAIL | -6 != 0 |
| `sai_ecmp_test.EcmpReuseLagRouteV6` | `runTest` | ❌ FAIL | -6 != 0 |
| `sai_ecmp_test.EcmpTwoLayersWithDiffHashOffsetTestV4` | `runTest` | ❌ FAIL | Did not receive expected packet on any of ports for device 0. ========== RECEIVE |
| `sai_ecmp_test.EcmpTwoLayersWithDiffHashOffsetTestV6` | `runTest` | ❌ FAIL | Did not receive expected packet on any of ports for device 0. ========== RECEIVE |
| `sai_ecmp_test.IngressNoDiffTestV4` | `runTest` | ❌ FAIL | Did not receive expected packet on any of ports [17, 18, 19, 20, 21, 22, 23, 24] |
| `sai_ecmp_test.LagTwoLayersWithDiffHashOffsetTestV4` | `runTest` | ❌ FAIL | Did not receive expected packet on any of ports for device 0. ========== RECEIVE |
| `sai_ecmp_test.LagTwoLayersWithDiffHashOffsetTestV6` | `runTest` | ❌ FAIL | Did not receive expected packet on any of ports for device 0. ========== RECEIVE |
| `sai_ecmp_test.ReAddLagEcmpTestV4` | `runTest` | ❌ FAIL | -7 != 0 |
| `sai_ecmp_test.ReAddLagEcmpTestV6` | `runTest` | ❌ FAIL | -7 != 0 |
| `sai_ecmp_test.RemoveAllNextHopMemeberTestV4` | `runTest` | ❌ FAIL | -7 != 0 |
| `sai_ecmp_test.RemoveLagEcmpTestV4` | `runTest` | ❌ FAIL | Did not receive expected packet on any of ports for device 0. ========== RECEIVE |
| `sai_ecmp_test.RemoveLagEcmpTestV6` | `runTest` | ❌ FAIL | Did not receive expected packet on any of ports for device 0. ========== RECEIVE |
| `sai_ecmp_test.RemoveNexthopGroupTestV4` | `runTest` | ❌ FAIL | -7 != 0 |
| `sai_neighbor_test.AddHostRouteTest` | `runTest` | ❌ FAIL | Did not receive expected packet on any of ports [17, 18] for device 0. ========= |
| `sai_neighbor_test.AddHostRouteTestV6` | `runTest` | ❌ FAIL | Did not receive expected packet on any of ports [17, 18] for device 0. ========= |
| `sai_neighbor_test.NhopDiffPrefixRemoveLonger` | `runTest` | ✅ PASS |  |
| `sai_neighbor_test.NhopDiffPrefixRemoveLongerV6` | `runTest` | ✅ PASS |  |
| `sai_neighbor_test.NhopDiffPrefixRemoveShorter` | `runTest` | ✅ PASS |  |
| `sai_neighbor_test.NhopDiffPrefixRemoveShorterV6` | `runTest` | ✅ PASS |  |
| `sai_neighbor_test.NoHostRouteTest` | `runTest` | ⚠️ ERROR | name 'status' is not defined |
| `sai_neighbor_test.NoHostRouteTestV6` | `runTest` | ✅ PASS |  |
| `sai_neighbor_test.RemoveAddNeighborTestIPV4` | `runTest` | ❌ FAIL | Did not receive expected packet on any of ports [17, 18] for device 0. ========= |
| `sai_neighbor_test.RemoveAddNeighborTestIPV6` | `runTest` | ❌ FAIL | Did not receive expected packet on any of ports [17, 18] for device 0. ========= |
| `sai_rif_test.IngressDisableTestV4` | `runTest` | ✅ PASS |  |
| `sai_rif_test.IngressDisableTestV6` | `runTest` | ✅ PASS |  |
| `sai_rif_test.IngressMacUpdateTest` | `runTest` | ⚠️ ERROR | 'NoneType' object is not subscriptable |
| `sai_rif_test.IngressMacUpdateTest` | `runTest` | ❌ FAIL | Did not receive expected packet on any of ports [17, 18] for device 0. ========= |
| `sai_rif_test.IngressMacUpdateTestV6` | `runTest` | ⚠️ ERROR | 'NoneType' object is not subscriptable |
| `sai_rif_test.IngressMacUpdateTestV6` | `runTest` | ❌ FAIL | Did not receive expected packet on any of ports [17, 18] for device 0. ========= |
| `sai_rif_test.IngressMtuTestV4` | `runTest` | ⚠️ ERROR | 'IngressMtuTestV4' object has no attribute 'mtu_Vlan10_rif' |
| `sai_rif_test.IngressMtuTestV4` | `runTest` | ❌ FAIL | Did not receive expected packet on any of ports [17, 18] for device 0. ========= |
| `sai_rif_test.IngressMtuTestV6` | `runTest` | ⚠️ ERROR | 'IngressMtuTestV6' object has no attribute 'mtu_port10_rif' |
| `sai_rif_test.IngressMtuTestV6` | `runTest` | ❌ FAIL | Did not receive expected packet on any of ports [17, 18] for device 0. ========= |
| `sai_route_test.DefaultRouteV4Test` | `runTest` | ✅ PASS |  |
| `sai_route_test.DefaultRouteV6Test` | `runTest` | ✅ PASS |  |
| `sai_route_test.DropRouteTest` | `runTest` | ✅ PASS |  |
| `sai_route_test.DropRoutev6Test` | `runTest` | ✅ PASS |  |
| `sai_route_test.LagMultipleRouteTest` | `runTest` | ✅ PASS |  |
| `sai_route_test.LagMultipleRoutev6Test` | `runTest` | ✅ PASS |  |
| `sai_route_test.RemoveRouteV4Test` | `runTest` | ✅ PASS |  |
| `sai_route_test.RouteDiffPrefixAddThenDeleteLongerV4Test` | `runTest` | ✅ PASS |  |
| `sai_route_test.RouteDiffPrefixAddThenDeleteLongerV6Test` | `runTest` | ✅ PASS |  |
| `sai_route_test.RouteDiffPrefixAddThenDeleteShorterV4Test` | `runTest` | ✅ PASS |  |
| `sai_route_test.RouteDiffPrefixAddThenDeleteShorterV6Test` | `runTest` | ✅ PASS |  |
| `sai_route_test.RouteLPMRouteNexthopTest` | `runTest` | ⚠️ ERROR | 'RouteLPMRouteNexthopTest' object has no attribute 'port1_route' |
| `sai_route_test.RouteLPMRouteNexthopTest` | `runTest` | ❌ FAIL | Expected packet was not received on device 0, port 2. ========== EXPECTED ====== |
| `sai_route_test.RouteLPMRouteNexthopv6Test` | `runTest` | ⚠️ ERROR | 'RouteLPMRouteNexthopv6Test' object has no attribute 'port1_route' |
| `sai_route_test.RouteLPMRouteNexthopv6Test` | `runTest` | ❌ FAIL | Expected packet was not received on device 0, port 2. ========== EXPECTED ====== |
| `sai_route_test.RouteLPMRouteRifTest` | `runTest` | ⚠️ ERROR | 'RouteLPMRouteRifTest' object has no attribute 'port1_route' |
| `sai_route_test.RouteLPMRouteRifTest` | `runTest` | ❌ FAIL | Expected packet was not received on device 0, port 2. ========== EXPECTED ====== |
| `sai_route_test.RouteLPMRouteRifv6Test` | `runTest` | ⚠️ ERROR | 'RouteLPMRouteRifv6Test' object has no attribute 'port1_route' |
| `sai_route_test.RouteLPMRouteRifv6Test` | `runTest` | ❌ FAIL | Expected packet was not received on device 0, port 2. ========== EXPECTED ====== |
| `sai_route_test.RouteRifTest` | `runTest` | ✅ PASS |  |
| `sai_route_test.RouteRifv6Test` | `runTest` | ✅ PASS |  |
| `sai_route_test.RouteSameSipDipv4Test` | `runTest` | ❌ FAIL | Did not receive expected packet on any of ports [19, 20] for device 0. ========= |
| `sai_route_test.RouteSameSipDipv6Test` | `runTest` | ✅ PASS |  |
| `sai_route_test.RouteUpdateTest` | `runTest` | ✅ PASS |  |
| `sai_route_test.RouteUpdatev6Test` | `runTest` | ✅ PASS |  |
| `sai_route_test.StaicSviMacFloodingTest` | `runTest` | ✅ PASS |  |
| `sai_route_test.StaicSviMacFloodingV6Test` | `runTest` | ✅ PASS |  |
| `sai_route_test.SviDirectBroadcastTest` | `runTest` | ❌ FAIL | False is not true : Did not receive pkt on one of ports [[9, 10, 11, 12, 13, 14, |
| `sai_route_test.SviMacAgeAfterMoveV4Test` | `runTest` | ❌ FAIL | Did not receive expected packet on any of ports [19, 20] for device 0. ========= |
| `sai_route_test.SviMacAgeAfterMoveV6Test` | `runTest` | ❌ FAIL | 0 != -1 |
| `sai_route_test.SviMacAgingTest` | `runTest` | ❌ FAIL | Did not receive expected packet on any of ports [19, 20] for device 0. ========= |
| `sai_route_test.SviMacAgingV6Test` | `runTest` | ❌ FAIL | 0 != -1 |
| `sai_route_test.SviMacFloodingTest` | `runTest` | ⚠️ ERROR | 'SviMacFloodingTest' object has no attribute 'port4_nbr_v4' |
| `sai_route_test.SviMacFloodingTest` | `runTest` | ❌ FAIL | -6 != 0 |
| `sai_route_test.SviMacFloodingv6Test` | `runTest` | ⚠️ ERROR | 'SviMacFloodingv6Test' object has no attribute 'port4_nbr_v4' |
| `sai_route_test.SviMacFloodingv6Test` | `runTest` | ❌ FAIL | -6 != 0 |
| `sai_route_test.SviMacLarningAfterAgeV6Test` | `runTest` | ❌ FAIL | 0 != -1 |
| `sai_route_test.SviMacLarningAfterageV4Test` | `runTest` | ❌ FAIL | Did not receive expected packet on any of ports [19, 20] for device 0. ========= |
| `sai_route_test.SviMacLearningTest` | `runTest` | ❌ FAIL | Did not receive expected packet on any of ports [19, 20] for device 0. ========= |
| `sai_route_test.SviMacLearningV6Test` | `runTest` | ❌ FAIL | 0 != -1 |
| `sai_route_test.SviMacMoveV4Test` | `runTest` | ❌ FAIL | Did not receive expected packet on any of ports [19, 20] for device 0. ========= |
| `sai_route_test.SviMacMoveV6Test` | `runTest` | ❌ FAIL | 0 != -1 |
| `sai_route_test.SviMacrMoveStressV4Test` | `runTest` | ⏭️ SKIP | SKIP! Skip test for performance issue. |
| `sai_route_test.SviMacrMoveStressV6Test` | `runTest` | ⏭️ SKIP | SKIP! Skip test for performance issue. |
| `sai_route_test.SviRouteL3Test` | `runTest` | ❌ FAIL | Expected packet was not received on device 0, port 1. ========== EXPECTED ====== |
| `sai_route_test.SviRouteL3v6Test` | `runTest` | ❌ FAIL | Expected packet was not received on device 0, port 1. ========== EXPECTED ====== |
