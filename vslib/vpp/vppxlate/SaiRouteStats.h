/*
 *------------------------------------------------------------------
 * SaiRouteStats.h
 *
 * Copyright (c) 2025 Cisco and/or its affiliates.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *------------------------------------------------------------------
 */

#ifndef _SAI_ROUTE_STATS_H_
#define _SAI_ROUTE_STATS_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct vpp_route_stats_ {
  uint64_t packets;
  uint64_t bytes;
} vpp_route_stats_t;

int vpp_route_stats_query(uint32_t stats_index, vpp_route_stats_t *stats);

/*
 * Callback invoked once per (worker thread, stats index) pair while dumping all
 * route stats. The same stats_index is reported once per VPP worker thread, so
 * the callback must accumulate (+=) packets/bytes per index to match the
 * per-index totals produced by vpp_route_stats_query.
 */
typedef void (*vpp_route_stats_cb)(uint32_t stats_index, uint64_t packets,
                                   uint64_t bytes, void *data);

/*
 * Perform a single full dump of "/net/route/to" and invoke cb for every entry.
 * Returns 0 on success, non-zero if the VPP stats dump failed. This collapses
 * the per-index connect/dump/disconnect of vpp_route_stats_query into one pass.
 */
int vpp_route_stats_dump_all(vpp_route_stats_cb cb, void *data);

#ifdef __cplusplus
}
#endif

#endif
