/*
 *------------------------------------------------------------------
 * SaiIntfStats.h
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

#ifndef _SAI_INTF_STATS_H_
#define _SAI_INTF_STATS_H_

#ifdef __cplusplus
extern "C" {
#endif

typedef struct vpp_interface_stats_ {
  uint64_t drops;
  uint64_t punt;
  uint64_t ip4;
  uint64_t ip6;
  uint64_t rx_no_buf;
  uint64_t rx_miss;
  uint64_t rx_error;
  uint64_t tx_error;
  uint64_t mpls;

  uint64_t rx;
  uint64_t rx_bytes;
  uint64_t rx_unicast;
  uint64_t rx_unicast_bytes;
  uint64_t rx_multicast;
  uint64_t rx_multicast_bytes;
  uint64_t rx_broadcast;
  uint64_t rx_broadcast_bytes;

  uint64_t tx;
  uint64_t tx_bytes;
  uint64_t tx_unicast;
  uint64_t tx_unicast_bytes;
  uint64_t tx_multicast;
  uint64_t tx_multicast_bytes;
  uint64_t tx_broadcast;
  uint64_t tx_broadcast_bytes;

} vpp_interface_stats_t;

int vpp_intf_stats_query(const char *intf_name, vpp_interface_stats_t *stats);

#ifdef __cplusplus
}
#endif

#endif
