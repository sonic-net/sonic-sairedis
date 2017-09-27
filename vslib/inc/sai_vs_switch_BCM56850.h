#ifndef __SAI_VS_SWITCH_BCM56850__
#define __SAI_VS_SWITCH_BCM56850__

#include "meta/sai_meta.h"
#include "meta/saiserialize.h"
#include "meta/saiattributelist.h"

void init_switch_BCM56850(
        _In_ sai_object_id_t switch_id);

void uninit_switch_BCM56850(
        _In_ sai_object_id_t switch_id);

sai_status_t refresh_read_only_BCM56850(
        _In_ const sai_attr_metadata_t *meta,
        _In_ sai_object_id_t object_id,
        _In_ sai_object_id_t switch_id);

#endif // __SAI_VS_SWITCH_BCM56850__
