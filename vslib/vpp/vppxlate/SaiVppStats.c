/*
 *------------------------------------------------------------------
 * SaiVppStats.c
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

#include <vpp-api/client/stat_client.h>
#include <vlib/vlib.h>
#include "SaiVppStats.h"

static int is_stats_inited = 0;
static __thread stat_client_main_t vpp_stat_client_main;

static int vpp_stats_init()
{
  if (is_stats_inited) return 0;

  is_stats_inited = 1;
  return 0;
}

int
vpp_stats_dump (char *query_path, vpp_stat_one one, vpp_stat_two two, void *data)
{
  u8 **patterns = 0;
  int rv;
  const char *sname;

  vpp_stats_init();

  vec_add1 (patterns, (u8*)query_path);

  rv = stat_segment_connect_r (STAT_SEGMENT_SOCKET_FILE, &vpp_stat_client_main);
  if (rv)
    {
      SAIVPP_STAT_ERR("Couldn't connect to vpp, does %s exist?\n",
		      STAT_SEGMENT_SOCKET_FILE);
      vec_free (patterns);
      return -1;
    }

  u32 *dir;
  uint i, j, k;
  stat_segment_data_t *res;

  dir = stat_segment_ls_r (patterns, &vpp_stat_client_main);
  vec_free (patterns);
  if (!dir || vec_len (dir) == 0)
    {
      if (dir)
        vec_free (dir);
      stat_segment_disconnect_r (&vpp_stat_client_main);
      return -1;
    }
  res = stat_segment_dump_r (dir, &vpp_stat_client_main);
  vec_free (dir);
  if (!res)
    {
      stat_segment_disconnect_r (&vpp_stat_client_main);
      return -1;
    }

  for (i = 0; i < vec_len (res); i++)
    {
      switch (res[i].type)
	{
	case STAT_DIR_TYPE_COUNTER_VECTOR_SIMPLE:
	  if (res[i].simple_counter_vec == 0)
	    continue;
	  for (k = 0; k < vec_len (res[i].simple_counter_vec); k++)
	    for (j = 0; j < vec_len (res[i].simple_counter_vec[k]); j++)
	      {
		SAIVPP_STAT_DBG("[%d @ %d]: %lu packets %s\n",
			 j, k, res[i].simple_counter_vec[k][j],
			 res[i].name);
		sname = strrchr(res[i].name, '/');
		if (sname)
		  {
		    sname++;
		    if (one)
		      {
			one(sname, j, res[i].simple_counter_vec[k][j], data);
		      }
		  }
	      }
	  break;

	case STAT_DIR_TYPE_COUNTER_VECTOR_COMBINED:
	  if (res[i].combined_counter_vec == 0)
	    continue;
	  for (k = 0; k < vec_len (res[i].combined_counter_vec); k++)
	    for (j = 0; j < vec_len (res[i].combined_counter_vec[k]); j++)
	      {
		SAIVPP_STAT_DBG("[%d @ %d]: %lu packets, %lu bytes %s\n",
			 j, k, res[i].combined_counter_vec[k][j].packets,
			 res[i].combined_counter_vec[k][j].bytes,
			 res[i].name);
		sname = strrchr(res[i].name, '/');
		if (sname)
		  {
		    sname++;
		    if (two)
		      {
			two(sname, j, res[i].combined_counter_vec[k][j].packets,
			    res[i].combined_counter_vec[k][j].bytes, data);
		      }
		  }
	      }
	  break;

	case STAT_DIR_TYPE_SCALAR_INDEX:
	  SAIVPP_STAT_DBG("%.2f %s\n", res[i].scalar_value, res[i].name);
	  break;

	case STAT_DIR_TYPE_NAME_VECTOR:
	  if (res[i].name_vector == 0)
	    continue;
	  for (k = 0; k < vec_len (res[i].name_vector); k++)
	    if (res[i].name_vector[k])
	      SAIVPP_STAT_DBG("[%d]: %s %s\n", k, res[i].name_vector[k],
		       res[i].name);
	  break;

	case STAT_DIR_TYPE_EMPTY:
	  break;

	default:
	  ;
	}
    }
  stat_segment_data_free (res);

  stat_segment_disconnect_r (&vpp_stat_client_main);

  return 0;
}

/*
 * fd.io coding-style-patch-verification: ON
 *
 * Local Variables:
 * eval: (c-set-style "gnu")
 * End:
 */
