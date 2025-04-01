/*
 *------------------------------------------------------------------
 * SaiVppStats.h
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

#ifndef _SAI_VPP_STATS_H_
#define _SAI_VPP_STATS_H_

typedef  void (*vpp_stat_one)(const char *, uint32_t index,  uint64_t, void *);
typedef  void (*vpp_stat_two)(const char *, uint32_t index, uint64_t, uint64_t, void *);

int vpp_stats_dump(char *query_path, vpp_stat_one one, vpp_stat_two two, void *data);

#define SAIVPP_STAT_DBG(format,args...) {}
// #define SAIVPP_STAT_DBG clib_warning
#define SAIVPP_STAT_ERR clib_error

#endif
