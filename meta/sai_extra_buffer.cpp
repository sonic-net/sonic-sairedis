#include "sai_meta.h"
#include "sai_extra.h"

sai_status_t meta_pre_create_buffer_pool(
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_SUCCESS;
}

sai_status_t meta_pre_remove_buffer_pool(
        _In_ sai_object_id_t pool_id)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_SUCCESS;
}

sai_status_t meta_pre_set_buffer_pool_attr(
        _In_ sai_object_id_t pool_id,
        _In_ const sai_attribute_t *attr)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_SUCCESS;
}

sai_status_t meta_pre_get_buffer_pool_attr(
        _In_ sai_object_id_t pool_id,
        _In_ uint32_t attr_count,
        _Inout_ sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_SUCCESS;
}

sai_status_t meta_pre_create_buffer_profile(
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    // TODO extra logic on checking profile buffer size may be needed
    // TODO we need to query other attribute pool id assigned and check wheter mode is dynamic/static

    const sai_attribute_t* attr_shared_dynamic_th = get_attribute_by_id(SAI_BUFFER_PROFILE_ATTR_SHARED_DYNAMIC_TH, attr_count, attr_list);

    if (attr_shared_dynamic_th != NULL)
    {
        // TODO check size
    }

    const sai_attribute_t* attr_shared_static_th = get_attribute_by_id(SAI_BUFFER_PROFILE_ATTR_SHARED_STATIC_TH, attr_count, attr_list);

    if (attr_shared_static_th != NULL)
    {
        // TODO check size
    }

    // TODO additional logic can be required for size check
    // TODO check xoff xon thresholds

    return SAI_STATUS_SUCCESS;
}

sai_status_t meta_pre_remove_buffer_profile(
        _In_ sai_object_id_t buffer_profile_id)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_SUCCESS;
}

sai_status_t meta_pre_set_buffer_profile_attr(
        _In_ sai_object_id_t buffer_profile_id,
        _In_ const sai_attribute_t *attr)
{
    SWSS_LOG_ENTER();

    // TODO on set, changing buffer pool on profiles should noe be possible
    // to change dynamic to static on the fly
    // pool_id on profile should be create_only ?

    return SAI_STATUS_SUCCESS;
}

sai_status_t meta_pre_get_buffer_profile_attr(
        _In_ sai_object_id_t buffer_profile_id,
        _In_ uint32_t attr_count,
        _Inout_ sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_SUCCESS;
}
