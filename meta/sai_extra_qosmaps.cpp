#include "sai_meta.h"
#include "sai_extra.h"

sai_status_t meta_pre_create_qos_map(
        _Out_ sai_object_id_t* qos_map_id,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    // TODO is that CREATE_ONLY or CREATE_AND_SET ?
    const sai_attribute_t* attr_map_to_value_list = get_attribute_by_id(SAI_QOS_MAP_ATTR_MAP_TO_VALUE_LIST, attr_count, attr_list);

    if (attr_map_to_value_list != NULL)
    {
        // TODO some extra validation may be required here

        sai_qos_map_list_t map_list = attr_map_to_value_list->value.qosmap;

        uint32_t count = map_list.count;

        sai_qos_map_t* list = map_list.list;

        if (list == NULL)
        {
            SWSS_LOG_ERROR("qos map list is NULL");

            return SAI_STATUS_INVALID_PARAMETER;
        }

        if (count < 1)
        {
            SWSS_LOG_ERROR("qos map count is zero");

            return SAI_STATUS_INVALID_PARAMETER;
        }

        SWSS_LOG_DEBUG("qos map count value: %u", count);

        for (uint32_t i = 0; i < count; ++i)
        {
            sai_qos_map_t qos_map = list[i];

            // TODO queue index needs validation

            switch (qos_map.key.color)
            {
                case SAI_PACKET_COLOR_GREEN:
                case SAI_PACKET_COLOR_YELLOW:
                case SAI_PACKET_COLOR_RED:
                    // ok
                    break;

                default:

                    SWSS_LOG_ERROR("qos map packet color invalid value: %d", qos_map.key.color);

                    return SAI_STATUS_INVALID_PARAMETER;
            }

            switch (qos_map.value.color)
            {
                case SAI_PACKET_COLOR_GREEN:
                case SAI_PACKET_COLOR_YELLOW:
                case SAI_PACKET_COLOR_RED:
                    // ok
                    break;

                default:

                    SWSS_LOG_ERROR("qos map packet color invalid value: %d", qos_map.value.color);

                    return SAI_STATUS_INVALID_PARAMETER;
            }
        }
    }

    return SAI_STATUS_SUCCESS;
}

sai_status_t meta_pre_remove_qos_map(
        _In_ sai_object_id_t qos_map_id)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_SUCCESS;
}

sai_status_t meta_pre_set_qos_map_attribute(
        _In_ sai_object_id_t qos_map_id,
        _In_ const sai_attribute_t *attr)
{
    SWSS_LOG_ENTER();

    // TODO double check if it's possible to change that attributes on the fly
    // case SAI_QOS_MAP_ATTR_MAP_TO_VALUE_LIST:

    return SAI_STATUS_SUCCESS;
}

sai_status_t meta_pre_get_qos_map_attribute(
        _In_ sai_object_id_t qos_map_id,
        _In_ uint32_t attr_count,
        _Inout_ sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_SUCCESS;
}
