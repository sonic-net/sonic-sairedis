/*
 * $Id: phymod_ids.h,v 1.1.2.1 Broadcom SDK $
 * $Copyright: (c) 2020 Broadcom.
 * Broadcom Proprietary and Confidential. All rights reserved.$
 *
 */

#ifndef _PHY_PHYMOD_IDS_H_
#define _PHY_PHYMOD_IDS_H_

#include <soc/types.h>

extern int
soc_phymod_core_id_get(int unit, int port, uint32 max_cores, uint32 max_phys,
                       uint32 *num_cores, uint32 *core_id, uint32 *phy_ids);

extern int 
soc_phymod_broadcast_id_offset(int unit, uint32 *broadcast_id_offset);

#endif /* _PHY_PHYMOD_IDS_H_ */
