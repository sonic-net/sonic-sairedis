/*
 *------------------------------------------------------------------
 * SaiAclStats.c
 *
 * Copyright (c) 2023 Cisco and/or its affiliates.
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

#include <vlib/vlib.h>

#include "SaiVppStats.h"
#include "SaiAclStats.h"

static void handle_stat_two (const char *stat_name, uint32_t ace_index,
			     uint64_t count1, uint64_t count2, void *data)
{
    vpp_ace_stats_t *st_p = (vpp_ace_stats_t *) data;

    if (st_p->ace_index == ace_index) {
        st_p->packets += count1;
        st_p->bytes += count2;
    }
}

/*
 * The prefix is passed as a %s argument rather than held as a table of format
 * strings: vslib builds with -Wformat-nonliteral and -Werror, so indexing an
 * array of format strings would not compile.  const char *const because
 * -Wwrite-strings is on in the same set.
 */
static const char *const rule_stats_prefix[VPP_RULE_STATS_MAX] = {
    [VPP_RULE_STATS_ACL] = "/acl",
    [VPP_RULE_STATS_PBH] = "/sonic-ext/pbh",
};

int vpp_rule_stats_query (vpp_rule_stats_type_t type, uint32_t table_index,
			  uint32_t rule_index, vpp_ace_stats_t *stats)
{
    char pathbuf[256];

    if (type >= VPP_RULE_STATS_MAX) {
        return -EINVAL;
    }

    snprintf(pathbuf, sizeof(pathbuf), "%s/%u/matches",
             rule_stats_prefix[type], table_index);

    memset(stats, 0, sizeof(*stats));
    stats->ace_index = rule_index;

    return vpp_stats_dump(pathbuf, NULL, handle_stat_two, stats);
}

#ifdef MAIN
void classify_get_trace_chain(void){};

void
os_exit (int code)
{
  exit (code);
}

int main (int argc, char *argv[])
{
  vpp_interface_stats_t stats;

  if (argc > 1)
    {
      memset(&stats, 0, sizeof(stats));
      vpp_stats_dump(argv[1], handle_stat_one, handle_stat_two, &stats);
    }
  exit(0);
}
#endif
