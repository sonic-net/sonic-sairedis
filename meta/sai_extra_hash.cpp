#include "sai_meta.h"
#include "sai_extra.h"

sai_status_t meta_pre_create_hash(
    _In_ uint32_t attr_count,
    _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    // TODO validate if fields don't repeat (need param allow repeat)
    // const sai_attribute_t* attr_native_field_list = get_attribute_by_id(SAI_HASH_ATTR_NATIVE_FIELD_LIST, attr_count, attr_list);

    return SAI_STATUS_SUCCESS;
}

sai_status_t meta_pre_remove_hash(
    _In_ sai_object_id_t hash_id)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_SUCCESS;
}

sai_status_t meta_pre_set_hash_attribute(
    _In_ sai_object_id_t hash_id,
    _In_ const sai_attribute_t *attr)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_SUCCESS;
}

sai_status_t meta_pre_get_hash_attribute(
    _In_ sai_object_id_t hash_id,
    _In_ uint32_t attr_count,
    _Inout_ sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_SUCCESS;
}
