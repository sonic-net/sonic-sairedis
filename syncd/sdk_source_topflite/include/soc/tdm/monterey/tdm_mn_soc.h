/*
 * $Id: tdm_mn_soc.h$
 * $Copyright: (c) 2020 Broadcom.
 * Broadcom Proprietary and Confidential. All rights reserved.$
 * All Rights Reserved.$
 *
 * TDM soc header for BCM56860
 */

#ifndef TDM_BCM56670_PREPROCESSOR_SOC_DEFINES_H
#define TDM_BCM56670_PREPROCESSOR_SOC_DEFINES_H

#ifdef _TDM_STANDALONE
	#include <tdm_mn_defines.h>
#else
	#include <soc/tdm/monterey/tdm_mn_defines.h>
#endif

typedef struct {
	int cur_idx;
	int pgw_tdm_idx;
	int ovs_tdm_idx;
	int tdm_stk_idx;
} mn_pgw_pntrs_t;

typedef struct {
	int subport;
	int cur_idx_max;
	int first_port;
	int last_port;
	int swap_array[MN_OS_LLS_GRP_LEN];
} mn_pgw_scheduler_vars_t;


#endif /* TDM_BCM56670_PREPROCESSOR_SOC_DEFINES_H */
