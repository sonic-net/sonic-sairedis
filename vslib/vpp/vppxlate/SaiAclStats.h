/*
 *------------------------------------------------------------------
 * SaiAclStats.h
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

#ifndef _SAIACLSTATS_H_
#define _SAIACLSTATS_H_

#ifdef __cplusplus
extern "C" {
#endif

    typedef struct vpp_ace_stats_ {
	uint64_t packets;
	uint64_t bytes;
	uint32_t ace_index;
    } vpp_ace_stats_t;

    /*
     * Selects the stats-segment subtree the rule counters live under.  ACL
     * and PBH counters are both per-table combined counters indexed by rule,
     * so only the path prefix differs.
     */
    typedef enum vpp_rule_stats_type_ {
	VPP_RULE_STATS_ACL = 0,		/* /acl/<acl_index>/matches            */
	VPP_RULE_STATS_PBH,		/* /sonic-ext/pbh/<table_index>/matches */
	VPP_RULE_STATS_MAX
    } vpp_rule_stats_type_t;

    int vpp_rule_stats_query(vpp_rule_stats_type_t type, uint32_t table_index,
			     uint32_t rule_index, vpp_ace_stats_t *stats);

#ifdef __cplusplus
}
#endif

#endif
