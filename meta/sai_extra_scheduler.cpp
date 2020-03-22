#include "sai_meta.h"
#include "sai_extra.h"

// could be in metadata
#define MIN_SCHEDULING_WEIGHT 1
#define MAX_SCHEDULING_WEIGHT 100

sai_status_t meta_pre_create_scheduler_profile(
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    const sai_attribute_t* attr_scheduling_weight = get_attribute_by_id(SAI_SCHEDULER_ATTR_SCHEDULING_WEIGHT, attr_count, attr_list);

    if (attr_scheduling_weight != NULL)
    {
        uint8_t weight = attr_scheduling_weight->value.u8;

        if (weight < MIN_SCHEDULING_WEIGHT || weight > MAX_SCHEDULING_WEIGHT)
        {
            SWSS_LOG_ERROR("invalid scheduling weight %u <%u..%u>", weight, MIN_SCHEDULING_WEIGHT, MAX_SCHEDULING_WEIGHT);

            return SAI_STATUS_INVALID_PARAMETER;
        }
    }

    return SAI_STATUS_SUCCESS;
}

sai_status_t meta_pre_remove_scheduler_profile(
        _In_ sai_object_id_t scheduler_id)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_SUCCESS;
}

sai_status_t meta_pre_set_scheduler_attribute(
        _In_ sai_object_id_t scheduler_id,
        _In_ const sai_attribute_t *attr)
{
    SWSS_LOG_ENTER();

    if (attr->id == SAI_SCHEDULER_ATTR_SCHEDULING_WEIGHT)
    {
        uint8_t weight = attr->value.u8;

        if (weight < MIN_SCHEDULING_WEIGHT || weight > MAX_SCHEDULING_WEIGHT)
        {
            SWSS_LOG_ERROR("invalid scheduling weight %u <%u..%u>", weight, MIN_SCHEDULING_WEIGHT, MAX_SCHEDULING_WEIGHT);

            return SAI_STATUS_INVALID_PARAMETER;
        }
    }

    return SAI_STATUS_SUCCESS;
}

sai_status_t meta_pre_get_scheduler_attribute(
        _In_ sai_object_id_t scheduler_id,
        _In_ uint32_t attr_count,
        _Inout_ sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_SUCCESS;
}
