/*
 * $Id: pscan.h,v 1.2 Broadcom SDK $
 * 
 * $Copyright: (c) 2020 Broadcom.
 * Broadcom Proprietary and Confidential. All rights reserved.$
 * 
 */

#ifndef __BCM_PSCAN_H__
#define __BCM_PSCAN_H__

#include <sal/types.h>

extern int soc_pscan_init(int unit);
extern int soc_pscan_update(int unit);
extern int soc_pscan_delay(int unit, int delay);
extern int soc_pscan_port_config(int unit, uint32 port, uint32 flags);
extern int soc_pscan_port_enable(int unit, uint32 port, int enable);
extern int soc_pscan_detach(int unit);

#endif /* __BCM_PSCAN_H__ */
