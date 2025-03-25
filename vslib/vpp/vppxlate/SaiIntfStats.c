/*
 *------------------------------------------------------------------
 * SaiIntfStats.c
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

#include <vlib/vlib.h>

#include "SaiVppStats.h"
#include "SaiIntfStats.h"

#define DROPS "drops"
#define PUNT "punt"
#define IP4 "ip4"
#define IP6 "ip6"
#define RX_NO_BUF "rx-no-buf"
#define RX_MISS "rx-miss"
#define RX_ERROR "rx-error"
#define TX_ERROR "tx-error"
#define MPLS "mpls"
#define RX "rx"
#define RX_UNICAST "rx-unicast"
#define RX_MULTICAST "rx-multicast"
#define RX_BROADCAST "rx-broadcast"
#define TX "tx"
#define TX_UNICAST "tx-unicast"
#define TX_MULTICAST "tx-multicast"
#define TX_BROADCAST "tx-broadcast"

static void handle_stat_one (const char *stat_name, uint32_t index,
			     uint64_t count, void *data)
{
  vpp_interface_stats_t *st_p = (vpp_interface_stats_t *) data;

  if (!strncmp(stat_name, DROPS, sizeof(DROPS)))
    {
      st_p->drops += count;
    }
  else if (!strncmp(stat_name, PUNT, sizeof(PUNT)))
    {
      st_p->punt += count;
    }
  else if (!strncmp(stat_name, IP4, sizeof(IP4)))
    {
      st_p->ip4 += count;
    }
  else if (!strncmp(stat_name, IP6, sizeof(IP6)))
    {
      st_p->ip6 += count;
    }
  else if (!strncmp(stat_name, RX_NO_BUF, sizeof(RX_NO_BUF)))
    {
      st_p->rx_no_buf += count;
    }
  else if (!strncmp(stat_name, RX_MISS, sizeof(RX_MISS)))
    {
      st_p->rx_miss += count;
    }
  else if (!strncmp(stat_name, RX_ERROR, sizeof(RX_ERROR)))
    {
      st_p->rx_error += count;
    }
  else if (!strncmp(stat_name, TX_ERROR, sizeof(TX_ERROR)))
    {
      st_p->tx_error += count;
    }
  else if (!strncmp(stat_name, MPLS, sizeof(MPLS)))
    {
      st_p->mpls += count;
    }
}

static void handle_stat_two (const char *stat_name, uint32_t index,
			     uint64_t count1, uint64_t count2, void *data)
{
  vpp_interface_stats_t *st_p = (vpp_interface_stats_t *) data;

  if (!strncmp(stat_name, RX, sizeof(RX)))
    {
      st_p->rx += count1;
      st_p->rx_bytes += count2;
    }
  else if (!strncmp(stat_name, TX, sizeof(TX)))
    {
      st_p->tx += count1;
      st_p->tx_bytes += count2;
    }
  else if (!strncmp(stat_name, RX_UNICAST, sizeof(RX_UNICAST)))
    {
      st_p->rx_unicast += count1;
      st_p->rx_unicast_bytes += count2;
    }
  else if (!strncmp(stat_name, RX_MULTICAST, sizeof(RX_MULTICAST)))
    {
      st_p->rx_multicast += count1;
      st_p->rx_multicast_bytes += count2;
    }
  else if (!strncmp(stat_name, RX_BROADCAST, sizeof(RX_BROADCAST)))
    {
      st_p->rx_broadcast += count1;
      st_p->rx_broadcast_bytes += count2;
    }
  else if (!strncmp(stat_name, TX_UNICAST, sizeof(TX_UNICAST)))
    {
      st_p->tx_unicast += count1;
      st_p->tx_unicast_bytes += count2;
    }
  else if (!strncmp(stat_name, TX_MULTICAST, sizeof(TX_MULTICAST)))
    {
      st_p->tx_multicast += count1;
      st_p->tx_multicast_bytes += count2;
    }
  else if (!strncmp(stat_name, TX_BROADCAST, sizeof(TX_BROADCAST)))
    {
      st_p->tx_broadcast += count1;
      st_p->tx_broadcast_bytes += count2;
    }
}

int vpp_intf_stats_query (const char *intf_name, vpp_interface_stats_t *stats)
{
  char pathbuf[256];

  snprintf(pathbuf, sizeof(pathbuf), "/interfaces/%s/", intf_name);

  memset(stats, 0, sizeof(*stats));

  return vpp_stats_dump(pathbuf, handle_stat_one, handle_stat_two, stats);
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
