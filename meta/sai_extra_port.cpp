#include "sai_meta.h"
#include "sai_extra.h"

sai_status_t meta_pre_set_port_attribute(
        _In_ sai_object_id_t port_id,
        _In_ const sai_attribute_t *attr)
{
    SWSS_LOG_ENTER();

    // case SAI_PORT_ATTR_SPEED:
    // TODO validate speed's

    // TODO additional validation may be required on QOS

    return SAI_STATUS_SUCCESS;
}

sai_status_t meta_pre_get_port_attribute(
        _In_ sai_object_id_t port_id,
        _In_ uint32_t attr_count,
        _Inout_ sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    // SAI_PORT_ATTR_HW_LANE_LIST
    // just log on post check

    return SAI_STATUS_SUCCESS;
}
