/*
 * $Id: sat.h,v 1.12 Broadcom SDK $
 * $Copyright: (c) 2020 Broadcom.
 * Broadcom Proprietary and Confidential. All rights reserved.$
 * 
 * Common internal definitions for BCM sat module
 */

#ifndef _BCM_INT_SAT_H_
#define _BCM_INT_SAT_H_

#include <sal/types.h>
#include <soc/defs.h>
#include <sal/core/sync.h>

#define _BCM_SAT_GTF_OBJ_COMMON          -1
#define _BCM_SAT_GTF_OBJ_CIR             0
#define _BCM_SAT_GTF_OBJ_EIR             1

extern int bcm_common_sat_init(int unit);

extern int bcm_common_sat_detach(int unit);
extern int bcm_common_oamp_port_enable_get(int unit, bcm_port_t port);
extern int bcm_common_sat_gtf_inuse(int unit);
extern int bcm_common_sat_ctf_inuse(int unit);
extern int bcm_common_sat_session_inuse( int unit,int session_id);
extern int bcm_common_sat_trap_idx_get(int unit, uint32 trap_id,int *trap_idx);

#ifdef BCM_WARM_BOOT_SUPPORT
int bcm_common_sat_wb_sync(int unit, int sync);
#endif

#endif /* _BCM_INT_SAT_H_ */
