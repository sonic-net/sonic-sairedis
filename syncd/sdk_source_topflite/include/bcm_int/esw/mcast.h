/*
 * $Id: mcast.h,v 1.11 Broadcom SDK $
 * $Copyright: (c) 2020 Broadcom.
 * Broadcom Proprietary and Confidential. All rights reserved.$
 *
 * File:        mcast.h
 * Purpose:     
 */

#ifndef   _BCM_INT_MCAST_H_
#define   _BCM_INT_MCAST_H_

#include <bcm/types.h>

extern int _bcm_esw_mcast_stk_update(int unit, uint32 flags);

extern int  _bcm_esw_mcast_detach(int unit);

extern int _bcm_esw_mcast_idx_ret_type_set(int unit, int arg);
extern int _bcm_esw_mcast_idx_ret_type_get(int unit, int *arg);

#ifndef BCM_SW_STATE_DUMP_DISABLE
extern void _bcm_mcast_sw_dump(int unit);
#endif /* BCM_SW_STATE_DUMP_DISABLE */

#endif	/* !_BCM_INT_MCAST_H_ */
