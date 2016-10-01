#include "sai_meta.h"
#include "sai_extra.h"

sai_status_t meta_pre_create_fdb_entry(
        _In_ const sai_fdb_entry_t *fdb_entry,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    const sai_attribute_t* attr_meta_data = get_attribute_by_id(SAI_FDB_ENTRY_ATTR_META_DATA, attr_count, attr_list);

    if (attr_meta_data != NULL)
    {
        // TODO validation range must be checked via SAI_SWITCH_ATTR_FDB_DST_USER_META_DATA_RANGE / saiswitch

        uint32_t meta_data = attr_meta_data->value.u32;

        SWSS_LOG_DEBUG("fdb metadata value: %u", meta_data);
    }

    return SAI_STATUS_SUCCESS;
}

sai_status_t meta_pre_remove_fdb_entry(
        _In_ const sai_fdb_entry_t* fdb_entry)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_SUCCESS;
}

sai_status_t meta_pre_set_fdb_entry_attribute(
        _In_ const sai_fdb_entry_t* fdb_entry,
        _In_ const sai_attribute_t *attr)
{
    SWSS_LOG_ENTER();

    // TODO validate metadata SAI_FDB_ENTRY_ATTR_META_DATA
    // depends on SAI_SWITCH_ATTR_FDB_DST_USER_META_DATA_RANGE

    return SAI_STATUS_SUCCESS;
}

sai_status_t meta_pre_get_fdb_entry_attribute(
        _In_ const sai_fdb_entry_t* fdb_entry,
        _In_ uint32_t attr_count,
        _Inout_ sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_SUCCESS;
}
