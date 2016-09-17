#include "sai_redis.h"

std::set<sai_object_id_t> local_udf_groups_set;
std::set<sai_object_id_t> local_udf_matches_set;
std::set<sai_object_id_t> local_udfs_set;

/**
 * Routine Description:
 *    @brief Create UDF
 *
 * Arguments:
 *    @param[out] udf_id - UDF id
 *    @param[in] attr_count - number of attributes
 *    @param[in] attr_list - array of attributes
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *            Failure status code on error
 *
 */
sai_status_t redis_create_udf(
    _Out_ sai_object_id_t* udf_id,
    _In_ uint32_t attr_count,
    _In_ const sai_attribute_t *attr_list)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    if (attr_list == NULL)
    {
        SWSS_LOG_ERROR("attribute list parameter is NULL");

        return SAI_STATUS_INVALID_PARAMETER;
    }

    if (attr_count < 3)
    {
        SWSS_LOG_ERROR("attribute count must be at least 3");

        // SAI_UDF_ATTR_MATCH_ID
        // SAI_UDF_ATTR_GROUP_ID
        // SAI_UDF_ATTR_OFFSET

        return SAI_STATUS_INVALID_PARAMETER;
    }

    const sai_attribute_t* attr_match_id = redis_get_attribute_by_id(SAI_UDF_ATTR_MATCH_ID, attr_count, attr_list);
    const sai_attribute_t* attr_group_id = redis_get_attribute_by_id(SAI_UDF_ATTR_GROUP_ID, attr_count, attr_list);
    const sai_attribute_t* attr_offset = redis_get_attribute_by_id(SAI_UDF_ATTR_OFFSET, attr_count, attr_list);

    if (attr_match_id == NULL)
    {
        SWSS_LOG_ERROR("missing match id attribute");

        return SAI_STATUS_MANDATORY_ATTRIBUTE_MISSING;
    }

    sai_object_id_t match_id = attr_match_id->value.oid;

    if (local_udf_matches_set.find(match_id) == local_udf_matches_set.end())
    {
        SWSS_LOG_ERROR("match id %llx is missing", match_id);

        return SAI_STATUS_INVALID_PARAMETER;
    }

    if (attr_group_id == NULL)
    {
        SWSS_LOG_ERROR("missing group id attribute");

        return SAI_STATUS_MANDATORY_ATTRIBUTE_MISSING;
    }

    sai_object_id_t group_id = attr_group_id->value.oid;

    if (local_udf_groups_set.find(group_id) == local_udf_groups_set.end())
    {
        SWSS_LOG_ERROR("group id %llx is missing", group_id);

        return SAI_STATUS_INVALID_PARAMETER;
    }

    if (attr_offset == NULL)
    {
        SWSS_LOG_ERROR("missing offset attribute");

        return SAI_STATUS_MANDATORY_ATTRIBUTE_MISSING;
    }

    // TODO validate offset value range


    const sai_attribute_t* attr_base = redis_get_attribute_by_id(SAI_UDF_ATTR_BASE, attr_count, attr_list);

    sai_udf_base_t udf_base = SAI_UDF_BASE_L2; // default value

    if (attr_base != NULL)
    {
        udf_base = (sai_udf_base_t)attr_base->value.s32;
    }

    switch (udf_base)
    {
        case SAI_UDF_BASE_L2:
        case SAI_UDF_BASE_L3:
        case SAI_UDF_BASE_L4:
            // ok
            break;

        default:

            SWSS_LOG_ERROR("invalid udf base value: %d", udf_base);

            return SAI_STATUS_INVALID_PARAMETER;
    }

    const sai_attribute_t* attr_hash_mask = redis_get_attribute_by_id(SAI_UDF_ATTR_HASH_MASK, attr_count, attr_list);

    if (attr_hash_mask != NULL)
    {
        sai_u8_list_t list = attr_hash_mask->value.u8list;

        if (list.list == NULL)
        {
            SWSS_LOG_ERROR("hash mask list is NULL");

            return SAI_STATUS_INVALID_PARAMETER;
        }

        // TODO length must be quual to group attr length ?

        // TODO extra validation may be required here
    }

    sai_status_t status = redis_generic_create(
            SAI_OBJECT_TYPE_UDF,
            udf_id,
            attr_count,
            attr_list);

    if (status == SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_DEBUG("inserting udf %llx to local state", *udf_id);

        local_udfs_set.insert(*udf_id);

        // TODO increase reference count for used object ids
    }

    return status;
}

/**
 * Routine Description:
 *    @brief Remove UDF
 *
 * Arguments:
 *    @param[in] udf_id - UDF id
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *            Failure status code on error
 */
sai_status_t redis_remove_udf(
    _In_ sai_object_id_t udf_id)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    // TODO check if it safe to remove udf

    if (local_udfs_set.find(udf_id) == local_udfs_set.end())
    {
        SWSS_LOG_ERROR("udf %llx is missing", udf_id);

        return SAI_STATUS_INVALID_PARAMETER;
    }

    sai_status_t status = redis_generic_remove(
            SAI_OBJECT_TYPE_UDF,
            udf_id);

    if (status == SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_DEBUG("erasing udf %llx from local state", udf_id);

        local_udfs_set.erase(udf_id);
    }

    return status;
}

/**
 * Routine Description:
 *    @brief Set UDF attribute
 *
 * Arguments:
 *    @param[in] udf_id - UDF id
 *    @param[in] attr - attribute
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *            Failure status code on error
 */
sai_status_t redis_set_udf_attribute(
    _In_ sai_object_id_t udf_id,
    _In_ const sai_attribute_t *attr)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    if (attr == NULL)
    {
        SWSS_LOG_ERROR("attribute parameter is NULL");

        return SAI_STATUS_INVALID_PARAMETER;
    }

    if (local_udfs_set.find(udf_id) == local_udfs_set.end())
    {
        SWSS_LOG_ERROR("udf %llx is missing", udf_id);

        return SAI_STATUS_INVALID_PARAMETER;
    }

    switch (attr->id)
    {
        case SAI_UDF_ATTR_BASE:

            {
                sai_udf_base_t udf_base = (sai_udf_base_t)attr->value.s32;

                switch (udf_base)
                {
                    case SAI_UDF_BASE_L2:
                    case SAI_UDF_BASE_L3:
                    case SAI_UDF_BASE_L4:
                        // ok
                        break;

                    default:

                        SWSS_LOG_ERROR("invalid udf base value: %d", udf_base);

                        return SAI_STATUS_INVALID_PARAMETER;
                }
            }

            break;

        case SAI_UDF_ATTR_HASH_MASK:
            break;

        default:

            SWSS_LOG_ERROR("setting attribute id %d is not supported", attr->id);

            return SAI_STATUS_INVALID_PARAMETER;
    }

    sai_status_t status = redis_generic_set(
            SAI_OBJECT_TYPE_UDF,
            udf_id,
            attr);

    return status;
}

/**
 * Routine Description:
 *    @brief Get UDF attribute value
 *
 * Arguments:
 *    @param[in] udf_id - UDF id
 *    @param[in] attr_count - number of attributes
 *    @param[inout] attrs - array of attributes
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *            Failure status code on error
 */
sai_status_t redis_get_udf_attribute(
    _In_ sai_object_id_t udf_id,
    _In_ uint32_t attr_count,
    _Inout_ sai_attribute_t *attr_list)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    if (attr_list == NULL)
    {
        SWSS_LOG_ERROR("attribute list parameter is NULL");

        return SAI_STATUS_INVALID_PARAMETER;
    }

    if (attr_count < 1)
    {
        SWSS_LOG_ERROR("attribute count must be at least 1");

        return SAI_STATUS_INVALID_PARAMETER;
    }

    if (local_udfs_set.find(udf_id) == local_udfs_set.end())
    {
        SWSS_LOG_ERROR("udf %llx is missing", udf_id);

        return SAI_STATUS_INVALID_PARAMETER;
    }

    for (uint32_t i = 0; i < attr_count; ++i)
    {
        sai_attribute_t* attr = &attr_list[i];

        switch (attr->id)
        {
            case SAI_UDF_ATTR_MATCH_ID:
            case SAI_UDF_ATTR_GROUP_ID:
            case SAI_UDF_ATTR_BASE:
            case SAI_UDF_ATTR_OFFSET:
                break;

            case SAI_UDF_ATTR_HASH_MASK:

                // TODO check for null list
                // ok
                break;

            default:

                SWSS_LOG_ERROR("getting attribute id %d is not supported", attr->id);

                return SAI_STATUS_INVALID_PARAMETER;
        }
    }

    sai_status_t status = redis_generic_get(
            SAI_OBJECT_TYPE_UDF,
            udf_id,
            attr_count,
            attr_list);

    return status;
}

/**
 * Routine Description:
 *    @brief Create UDF match
 *
 * Arguments:
 *    @param[out] udf_match_id - UDF match id
 *    @param[in] attr_count - number of attributes
 *    @param[in] attr_list - array of attributes
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *            Failure status code on error
 *
 */
sai_status_t redis_create_udf_match(
    _Out_ sai_object_id_t* udf_match_id,
    _In_ uint32_t attr_count,
    _In_ const sai_attribute_t *attr_list)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    // no mandatory attributes

    // const sai_attribute_t* attr_l2_type = redis_get_attribute_by_id(SAI_UDF_MATCH_ATTR_L2_TYPE, attr_count, attr_list);
    // const sai_attribute_t* attr_l3_type = redis_get_attribute_by_id(SAI_UDF_MATCH_ATTR_L3_TYPE, attr_count, attr_list);
    // const sai_attribute_t* attr_gre_type = redis_get_attribute_by_id(SAI_UDF_MATCH_ATTR_GRE_TYPE, attr_count, attr_list);
    // const sai_attribute_t* attr_priority = redis_get_attribute_by_id(SAI_UDF_MATCH_ATTR_PRIORITY, attr_count, attr_list);

    // sai_acl_field_data_t(uint16_t)
    // sai_acl_field_data_t(uint8_t)
    // sai_acl_field_data_t(uint16_t)

    // TODO default is to NONE, what this mean? is disabled or what ?

    sai_status_t status = redis_generic_create(
            SAI_OBJECT_TYPE_UDF_MATCH,
            udf_match_id,
            attr_count,
            attr_list);

    if (status == SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_DEBUG("inserting udf match %llx to local state", *udf_match_id);

        local_udf_matches_set.insert(*udf_match_id);

        // TODO increase reference count for used object ids
    }

    return status;
}

/**
 * Routine Description:
 *    @brief Remove UDF match
 *
 * Arguments:
 *    @param[in] udf_match_id - UDF match id
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *            Failure status code on error
 */
sai_status_t redis_remove_udf_match(
    _In_ sai_object_id_t udf_match_id)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    // TODO check if it safe to remove udf match

    if (local_udf_matches_set.find(udf_match_id) == local_udf_matches_set.end())
    {
        SWSS_LOG_ERROR("udf match %llx is missing", udf_match_id);

        return SAI_STATUS_INVALID_PARAMETER;
    }

    sai_status_t status = redis_generic_remove(
            SAI_OBJECT_TYPE_UDF_MATCH,
            udf_match_id);

    if (status == SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_DEBUG("erasing udf match %llx from local state", udf_match_id);

        local_udf_matches_set.erase(udf_match_id);
    }

    return status;
}

/**
 * Routine Description:
 *    @brief Set UDF match attribute
 *
 * Arguments:
 *    @param[in] udf_match_id - UDF match id
 *    @param[in] attr - attribute
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *            Failure status code on error
 */
sai_status_t redis_set_udf_match_attribute(
    _In_ sai_object_id_t udf_match_id,
    _In_ const sai_attribute_t *attr)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    if (attr == NULL)
    {
        SWSS_LOG_ERROR("attribute parameter is NULL");

        return SAI_STATUS_INVALID_PARAMETER;
    }

    if (local_udf_matches_set.find(udf_match_id) == local_udf_matches_set.end())
    {
        SWSS_LOG_ERROR("udf match %llx is missing", udf_match_id);

        return SAI_STATUS_INVALID_PARAMETER;
    }

    switch (attr->id)
    {
        // no attributes with set
        default:

            SWSS_LOG_ERROR("setting attribute id %d is not supported", attr->id);

            return SAI_STATUS_INVALID_PARAMETER;
    }

    sai_status_t status = redis_generic_set(
            SAI_OBJECT_TYPE_UDF_MATCH,
            udf_match_id,
            attr);

    return status;
}

/**
 * Routine Description:
 *    @brief Get UDF match attribute value
 *
 * Arguments:
 *    @param[in] udf_match_id - UDF match id
 *    @param[in] attr_count - number of attributes
 *    @param[inout] attrs - array of attributes
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *            Failure status code on error
 */
sai_status_t redis_get_udf_match_attribute(
    _In_ sai_object_id_t udf_match_id,
    _In_ uint32_t attr_count,
    _Inout_ sai_attribute_t *attr_list)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    if (attr_list == NULL)
    {
        SWSS_LOG_ERROR("attribute list parameter is NULL");

        return SAI_STATUS_INVALID_PARAMETER;
    }

    if (attr_count < 1)
    {
        SWSS_LOG_ERROR("attribute count must be at least 1");

        return SAI_STATUS_INVALID_PARAMETER;
    }

    if (local_udf_matches_set.find(udf_match_id) == local_udf_matches_set.end())
    {
        SWSS_LOG_ERROR("udf match %llx is missing", udf_match_id);

        return SAI_STATUS_INVALID_PARAMETER;
    }

    for (uint32_t i = 0; i < attr_count; ++i)
    {
        sai_attribute_t* attr = &attr_list[i];

        switch (attr->id)
        {
            case SAI_UDF_MATCH_ATTR_L2_TYPE:
            case SAI_UDF_MATCH_ATTR_L3_TYPE:
            case SAI_UDF_MATCH_ATTR_GRE_TYPE:
            case SAI_UDF_MATCH_ATTR_PRIORITY:
                // ok
                break;

            default:

                SWSS_LOG_ERROR("getting attribute id %d is not supported", attr->id);

                return SAI_STATUS_INVALID_PARAMETER;
        }
    }

    sai_status_t status = redis_generic_get(
            SAI_OBJECT_TYPE_UDF_MATCH,
            udf_match_id,
            attr_count,
            attr_list);

    return status;
}

/**
 * Routine Description:
 *    @brief Create UDF group
 *
 * Arguments:
 *    @param[out] udf_group_id - UDF group id
 *    @param[in] attr_count - number of attributes
 *    @param[in] attr_list - array of attributes
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *            Failure status code on error
 *
 */
sai_status_t redis_create_udf_group(
    _Out_ sai_object_id_t* udf_group_id,
    _In_ uint32_t attr_count,
    _In_ const sai_attribute_t *attr_list)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    if (attr_list == NULL)
    {
        SWSS_LOG_ERROR("attribute list parameter is NULL");

        return SAI_STATUS_INVALID_PARAMETER;
    }

    if (attr_count < 1)
    {
        SWSS_LOG_ERROR("attribute count must be at least 1");

        // SAI_UDF_GROUP_ATTR_LENGTH

        return SAI_STATUS_INVALID_PARAMETER;
    }

    const sai_attribute_t* attr_type = redis_get_attribute_by_id(SAI_UDF_GROUP_ATTR_TYPE, attr_count, attr_list);
    const sai_attribute_t* attr_length = redis_get_attribute_by_id(SAI_UDF_GROUP_ATTR_LENGTH, attr_count, attr_list);

    sai_udf_group_type_t type = SAI_UDF_GROUP_GENERICi; // default value

    if (attr_type != NULL)
    {
        type = (sai_udf_group_type_t)attr_type->value.s32;
    }

    switch (type)
    {
        //case SAI_UDF_GROUP_GENERIC:
        case SAI_UDF_GROUP_GENERICi:
        case SAI_UDF_GROUP_HASH:
            // ok
            break;

        default:

            SWSS_LOG_ERROR("invalid udf group type value: %d", type);

            return SAI_STATUS_INVALID_PARAMETER;
    }

    if (attr_length == NULL)
    {
        SWSS_LOG_ERROR("attribute length parameter is NULL");

        return SAI_STATUS_INVALID_PARAMETER;
    }

    // TODO extra validation may be required on length

    sai_status_t status = redis_generic_create(
            SAI_OBJECT_TYPE_UDF_GROUP,
            udf_group_id,
            attr_count,
            attr_list);

    if (status == SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_DEBUG("inserting udf %llx to local state", *udf_group_id);

        local_udf_groups_set.insert(*udf_group_id);

        // TODO increase reference count for used object ids
    }

    return status;
}

/**
 * Routine Description:
 *    @brief Remove UDF group
 *
 * Arguments:
 *    @param[in] udf_group_id - UDF group id
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *            Failure status code on error
 */
sai_status_t redis_remove_udf_group(
    _In_ sai_object_id_t udf_group_id)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    // TODO check if it safe to remove udf group

    if (local_udf_groups_set.find(udf_group_id) == local_udf_groups_set.end())
    {
        SWSS_LOG_ERROR("udf group %llx is missing", udf_group_id);

        return SAI_STATUS_INVALID_PARAMETER;
    }

    sai_status_t status = redis_generic_remove(
            SAI_OBJECT_TYPE_UDF_GROUP,
            udf_group_id);

    if (status == SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_DEBUG("erasing udf group %llx from local state", udf_group_id);

        local_udf_groups_set.erase(udf_group_id);
    }

    return status;
}

/**
 * Routine Description:
 *    @brief Set UDF group attribute
 *
 * Arguments:
 *    @param[in] udf_group_id - UDF group id
 *    @param[in] attr - attribute
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *            Failure status code on error
 */
sai_status_t redis_set_udf_group_attribute(
    _In_ sai_object_id_t udf_group_id,
    _In_ const sai_attribute_t *attr)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    if (attr == NULL)
    {
        SWSS_LOG_ERROR("attribute parameter is NULL");

        return SAI_STATUS_INVALID_PARAMETER;
    }

    if (local_udf_groups_set.find(udf_group_id) == local_udf_groups_set.end())
    {
        SWSS_LOG_ERROR("udf group %llx is missing", udf_group_id);

        return SAI_STATUS_INVALID_PARAMETER;
    }

    switch (attr->id)
    {
        // no attributes with set
        default:

            SWSS_LOG_ERROR("setting attribute id %d is not supported", attr->id);

            return SAI_STATUS_INVALID_PARAMETER;
    }

    sai_status_t status = redis_generic_set(
            SAI_OBJECT_TYPE_UDF_GROUP,
            udf_group_id,
            attr);

    return status;
}

/**
 * Routine Description:
 *    @brief Get UDF group attribute value
 *
 * Arguments:
 *    @param[in] udf_group_id - UDF group id
 *    @param[in] attr_count - number of attributes
 *    @param[inout] attrs - array of attributes
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *            Failure status code on error
 */
sai_status_t redis_get_udf_group_attribute(
    _In_ sai_object_id_t udf_group_id,
    _In_ uint32_t attr_count,
    _Inout_ sai_attribute_t *attr_list)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    if (attr_list == NULL)
    {
        SWSS_LOG_ERROR("attribute list parameter is NULL");

        return SAI_STATUS_INVALID_PARAMETER;
    }

    if (attr_count < 1)
    {
        SWSS_LOG_ERROR("attribute count must be at least 1");

        return SAI_STATUS_INVALID_PARAMETER;
    }

    if (local_udf_groups_set.find(udf_group_id) == local_udf_groups_set.end())
    {
        SWSS_LOG_ERROR("udf group %llx is missing", udf_group_id);

        return SAI_STATUS_INVALID_PARAMETER;
    }

    for (uint32_t i = 0; i < attr_count; ++i)
    {
        sai_attribute_t* attr = &attr_list[i];

        switch (attr->id)
        {
            case SAI_UDF_GROUP_ATTR_UDF_LIST:
                // TODO check for list null
                break;

            case SAI_UDF_GROUP_ATTR_TYPE:
            case SAI_UDF_GROUP_ATTR_LENGTH:
                // ok
                break;

            default:

                SWSS_LOG_ERROR("getting attribute id %d is not supported", attr->id);

                return SAI_STATUS_INVALID_PARAMETER;
        }
    }

    sai_status_t status = redis_generic_get(
            SAI_OBJECT_TYPE_UDF_GROUP,
            udf_group_id,
            attr_count,
            attr_list);

    return status;
}

/**
 * @brief UDF methods, retrieved via sai_api_query()
 */
const sai_udf_api_t redis_udf_api = {
    redis_create_udf,
    redis_remove_udf,
    redis_set_udf_attribute,
    redis_get_udf_attribute,

    redis_create_udf_match,
    redis_remove_udf_match,
    redis_set_udf_match_attribute,
    redis_get_udf_match_attribute,

    redis_create_udf_group,
    redis_remove_udf_group,
    redis_set_udf_group_attribute,
    redis_get_udf_group_attribute,
};
