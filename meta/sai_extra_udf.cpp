#include "sai_meta.h"
#include "sai_extra.h"

sai_status_t meta_pre_create_udf(
        _Out_ sai_object_id_t* udf_id,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    // TODO validate offset value range
    // TODO length must be quual to group attr length ?
    // TODO extra validation may be required here on mask

    return SAI_STATUS_SUCCESS;
}

sai_status_t meta_pre_remove_udf(
        _In_ sai_object_id_t udf_id)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_SUCCESS;
}

sai_status_t meta_pre_set_udf_attribute(
        _In_ sai_object_id_t udf_id,
        _In_ const sai_attribute_t *attr)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_SUCCESS;
}

sai_status_t meta_pre_get_udf_attribute(
        _In_ sai_object_id_t udf_id,
        _In_ uint32_t attr_count,
        _Inout_ sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_SUCCESS;
}

sai_status_t meta_pre_create_udf_match(
        _Out_ sai_object_id_t* udf_match_id,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    // const sai_attribute_t* attr_l2_type = get_attribute_by_id(SAI_UDF_MATCH_ATTR_L2_TYPE, attr_count, attr_list);
    // const sai_attribute_t* attr_l3_type = get_attribute_by_id(SAI_UDF_MATCH_ATTR_L3_TYPE, attr_count, attr_list);
    // const sai_attribute_t* attr_gre_type = get_attribute_by_id(SAI_UDF_MATCH_ATTR_GRE_TYPE, attr_count, attr_list);
    // const sai_attribute_t* attr_priority = get_attribute_by_id(SAI_UDF_MATCH_ATTR_PRIORITY, attr_count, attr_list);

    // sai_acl_field_data_t(uint16_t)
    // sai_acl_field_data_t(uint8_t)
    // sai_acl_field_data_t(uint16_t)

    // TODO default is to NONE, what this mean? is disabled or what ?

    return SAI_STATUS_SUCCESS;
}

sai_status_t meta_pre_remove_udf_match(
        _In_ sai_object_id_t udf_match_id)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_SUCCESS;
}

sai_status_t meta_pre_set_udf_match_attribute(
        _In_ sai_object_id_t udf_match_id,
        _In_ const sai_attribute_t *attr)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_SUCCESS;
}

sai_status_t meta_pre_get_udf_match_attribute(
        _In_ sai_object_id_t udf_match_id,
        _In_ uint32_t attr_count,
        _Inout_ sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_SUCCESS;
}

sai_status_t meta_pre_create_udf_group(
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    // TODO extra validation may be required on length

    return SAI_STATUS_SUCCESS;
}

sai_status_t meta_pre_remove_udf_group(
        _In_ sai_object_id_t udf_group_id)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_SUCCESS;
}

sai_status_t meta_pre_set_udf_group_attribute(
        _In_ sai_object_id_t udf_group_id,
        _In_ const sai_attribute_t *attr)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_SUCCESS;
}

sai_status_t meta_pre_get_udf_group_attribute(
        _In_ sai_object_id_t udf_group_id,
        _In_ uint32_t attr_count,
        _Inout_ sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_SUCCESS;
}
