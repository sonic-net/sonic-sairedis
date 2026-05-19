/*
 *------------------------------------------------------------------
 * SaiRouteStats.c
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

#include <errno.h>
#include <stdbool.h>
#include <string.h>

#include "SaiRouteStats.h"
#include "SaiVppStats.h"

#define ROUTE_TO "to"

typedef struct vpp_route_stats_query_ {
  uint32_t stats_index;
  vpp_route_stats_t *stats;
  bool found;
} vpp_route_stats_query_t;

static void handle_stat_two (const char *stat_name, uint32_t index,
			     uint64_t count1, uint64_t count2, void *data)
{
  vpp_route_stats_query_t *query = (vpp_route_stats_query_t *) data;

  if (index != query->stats_index || strcmp(stat_name, ROUTE_TO) != 0)
    {
      return;
    }

  query->stats->packets += count1;
  query->stats->bytes += count2;
  query->found = true;
}

int vpp_route_stats_query (uint32_t stats_index, vpp_route_stats_t *stats)
{
  char pathbuf[] = "/net/route/to";
  vpp_route_stats_query_t query;

  memset(stats, 0, sizeof(*stats));

  query.stats_index = stats_index;
  query.stats = stats;
  query.found = false;

  if (vpp_stats_dump(pathbuf, NULL, handle_stat_two, &query) != 0)
    {
      return -EIO;
    }

  return query.found ? 0 : -ENOENT;
}
