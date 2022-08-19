/*
 *
 * $Id: brcm_pai_switch.h $
 *
 * $Copyright: (c) 2021 Broadcom.
 * Broadcom Proprietary and Confidential. All rights reserved.$
 *
 *
 */

#ifndef BRCM_PAI_SWITCH_H_
#define BRCM_PAI_SWITCH_H_

#include "saiswitch.h"

sai_status_t pai_get_switch_attribute(
        _In_ sai_object_id_t switch_id,
        _In_ uint32_t attr_count,
        _Inout_ sai_attribute_t *attr_list);

sai_status_t brcm_pai_get_switch_info(void *platform_ctxt,
        pai_phy_init_t *phy_init);

#endif /* BRCM_PAI_SWITCH_H_ */
