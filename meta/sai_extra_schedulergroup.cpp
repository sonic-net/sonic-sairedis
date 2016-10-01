#include "sai_meta.h"
#include "sai_extra.h"

sai_status_t meta_pre_create_scheduler_group(
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    const sai_attribute_t* attr_level = get_attribute_by_id(SAI_SCHEDULER_GROUP_ATTR_LEVEL, attr_count, attr_list);
    const sai_attribute_t* attr_max_childs = get_attribute_by_id(SAI_SCHEDULER_GROUP_ATTR_MAX_CHILDS , attr_count, attr_list);

    uint8_t level = attr_level->value.u8;

    if (level > 16)
    {
        SWSS_LOG_ERROR("invalid level value: %u <%u..%u>", level, 0, 16);

        return SAI_STATUS_INVALID_PARAMETER;
    }

    // TODO level will require some additional validation to not crete loops

    uint8_t max_childs = attr_max_childs->value.u8;

    if (max_childs > 64)
    {
        SWSS_LOG_ERROR("invalid max childs value: %u <%u..%u>", max_childs, 0, 64);

        return SAI_STATUS_INVALID_PARAMETER;
    }

    // TODO max childs may require more validation

    return SAI_STATUS_SUCCESS;
}

sai_status_t meta_pre_remove_scheduler_group(
        _In_ sai_object_id_t scheduler_group_id)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_SUCCESS;
}

sai_status_t meta_pre_set_scheduler_group_attribute(
        _In_ sai_object_id_t scheduler_group_id,
        _In_ const sai_attribute_t *attr)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_SUCCESS;
}

sai_status_t meta_pre_get_scheduler_group_attribute(
        _In_ sai_object_id_t scheduler_group_id,
        _In_ uint32_t attr_count,
        _Inout_ sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_SUCCESS;
}
