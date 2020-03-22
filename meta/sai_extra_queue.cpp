#include "sai_meta.h"
#include "sai_extra.h"

sai_status_t meta_pre_create_queue(
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    const sai_attribute_t* attr_index = get_attribute_by_id(SAI_QUEUE_ATTR_INDEX, attr_count, attr_list);

    uint8_t index = attr_index->value.u8;

    if (index > 16) // TODO see where we can get actual value
    {
        SWSS_LOG_ERROR("invalid queue index value: %u", index);

        return SAI_STATUS_INVALID_PARAMETER;
    }

    // TODO scheduler and profile are assigned by default from some internal objects
    // they must be here set as mandatory_on create, or is it possible to disable scheduler or profile?

    return SAI_STATUS_SUCCESS;
}

sai_status_t meta_pre_remove_queue(
        _In_ sai_object_id_t queue_id)
{
    SWSS_LOG_ENTER();

    // TODO check if it safe to remove queue
    // it may require some extra logic sine queues
    // are pointed by index
    // some internal queues may not be possible to remove

    return SAI_STATUS_SUCCESS;
}

sai_status_t meta_pre_set_queue_attribute(
        _In_ sai_object_id_t queue_id,
        _In_ const sai_attribute_t *attr)
{
    SWSS_LOG_ENTER();

    // TODO double check if it's possible to change that attributes on the fly

    return SAI_STATUS_SUCCESS;
}

sai_status_t meta_pre_get_queue_attribute(
        _In_ sai_object_id_t queue_id,
        _In_ uint32_t attr_count,
        _Inout_ sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_SUCCESS;
}

