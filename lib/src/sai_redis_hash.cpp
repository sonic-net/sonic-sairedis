#include "sai_redis.h"

std::set<sai_object_id_t> local_hashes_set;

/**
 * Routine Description:
 *    @brief Create hash
 *
 * Arguments:
 *    @param[out] hash_id - hash id
 *    @param[in] attr_count - number of attributes
 *    @param[in] attr_list - array of attributes
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *            Failure status code on error
 *
 */
sai_status_t redis_create_hash(
    _Out_ sai_object_id_t* hash_id,
    _In_ uint32_t attr_count,
    _In_ const sai_attribute_t *attr_list)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    // no mandatory attributes

    const sai_attribute_t* attr_native_field_list = redis_get_attribute_by_id(SAI_HASH_ATTR_NATIVE_FIELD_LIST, attr_count, attr_list);

    if (attr_native_field_list != NULL)
    {
        sai_s32_list_t native_field_list = attr_native_field_list->value.s32list;

        if (native_field_list.list == NULL)
        {
            SWSS_LOG_ERROR("native field list is NULL");

            return SAI_STATUS_INVALID_PARAMETER;
        }

        uint32_t count = native_field_list.count;

        sai_native_hash_field_t* list = (sai_native_hash_field_t*)native_field_list.list;

        for (uint32_t i = 0; i < count; i++)
        {
            sai_native_hash_field_t field = list[i];

            switch (field)
            {
                case SAI_NATIVE_HASH_FIELD_SRC_IP:
                case SAI_NATIVE_HASH_FIELD_DST_IP:
                case SAI_NATIVE_HASH_FIELD_INNER_SRC_IP:
                case SAI_NATIVE_HASH_FIELD_INNER_DST_IP:
                case SAI_NATIVE_HASH_FIELD_VLAN_ID:
                case SAI_NATIVE_HASH_FIELD_IP_PROTOCOL:
                case SAI_NATIVE_HASH_FIELD_ETHERTYPE:
                case SAI_NATIVE_HASH_FIELD_L4_SRC_PORT:
                case SAI_NATIVE_HASH_FIELD_L4_DST_PORT:
                case SAI_NATIVE_HASH_FIELD_SRC_MAC:
                case SAI_NATIVE_HASH_FIELD_DST_MAC:
                case SAI_NATIVE_HASH_FIELD_IN_PORT:
                    // ok
                    break;

                default:

                    SWSS_LOG_ERROR("invalid hash field value: %d", field);

                    return SAI_STATUS_INVALID_PARAMETER;
            }
        }

        // TODO validate if fields don't repeat
    }

    const sai_attribute_t* attr_udf_group_list = redis_get_attribute_by_id(SAI_HASH_ATTR_UDF_GROUP_LIST, attr_count, attr_list);

    if (attr_udf_group_list != NULL)
    {
        sai_object_list_t udf_group_list = attr_udf_group_list->value.objlist;

        if (udf_group_list.list == NULL)
        {
            SWSS_LOG_ERROR("udf group list is NULL");

            return SAI_STATUS_INVALID_PARAMETER;
        }

        for (uint32_t i = 0; i < udf_group_list.count; ++i)
        {
            sai_object_id_t udf_group = udf_group_list.list[i];

            if (local_udf_groups_set.find(udf_group) == local_udf_groups_set.end())
            {
                SWSS_LOG_ERROR("udf group %llx is missing", udf_group);

                return SAI_STATUS_INVALID_PARAMETER;
            }
        }

        // TODO check for list repetition (metadata not allow repeat)
    }

    sai_status_t status = redis_generic_create(
            SAI_OBJECT_TYPE_HASH,
            hash_id,
            attr_count,
            attr_list);

    if (status == SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_DEBUG("inserting hash %llx to local state", *hash_id);

        local_hashes_set.insert(*hash_id);

        // TODO increase reference count for used object ids
    }

    return status;
}

/**
 * Routine Description:
 *    @brief Remove hash
 *
 * Arguments:
 *    @param[in] hash_id - hash id
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *            Failure status code on error
 */
sai_status_t redis_remove_hash(
    _In_ sai_object_id_t hash_id)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    // TODO check if it safe to remove hash
    // default created hash can't be removed and it needs to be
    // obtained from switch

    if (local_hashes_set.find(hash_id) == local_hashes_set.end())
    {
        SWSS_LOG_ERROR("hash %llx is missing", hash_id);

        return SAI_STATUS_INVALID_PARAMETER;
    }

    sai_status_t status = redis_generic_remove(
            SAI_OBJECT_TYPE_HASH,
            hash_id);

    if (status == SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_DEBUG("erasing hash %llx from local state", hash_id);

        local_hashes_set.erase(hash_id);
    }

    return status;
}

/**
 * Routine Description:
 *    @brief Set hash attribute
 *
 * Arguments:
 *    @param[in] hash_id - hash id
 *    @param[in] attr - attribute
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *            Failure status code on error
 */
sai_status_t redis_set_hash_attribute(
    _In_ sai_object_id_t hash_id,
    _In_ const sai_attribute_t *attr)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    if (attr == NULL)
    {
        SWSS_LOG_ERROR("attribute parameter is NULL");

        return SAI_STATUS_INVALID_PARAMETER;
    }

    if (local_hashes_set.find(hash_id) == local_hashes_set.end())
    {
        SWSS_LOG_ERROR("hash %llx is missing", hash_id);

        return SAI_STATUS_INVALID_PARAMETER;
    }

    switch (attr->id)
    {
        case SAI_HASH_ATTR_NATIVE_FIELD_LIST:

            {
                sai_s32_list_t native_field_list = attr->value.s32list;

                if (native_field_list.list == NULL)
                {
                    SWSS_LOG_ERROR("native field list is NULL");

                    return SAI_STATUS_INVALID_PARAMETER;
                }

                uint32_t count = native_field_list.count;

                sai_native_hash_field_t* list = (sai_native_hash_field_t*)native_field_list.list;

                for (uint32_t i = 0; i < count; i++)
                {
                    sai_native_hash_field_t field = list[i];

                    switch (field)
                    {
                        case SAI_NATIVE_HASH_FIELD_SRC_IP:
                        case SAI_NATIVE_HASH_FIELD_DST_IP:
                        case SAI_NATIVE_HASH_FIELD_INNER_SRC_IP:
                        case SAI_NATIVE_HASH_FIELD_INNER_DST_IP:
                        case SAI_NATIVE_HASH_FIELD_VLAN_ID:
                        case SAI_NATIVE_HASH_FIELD_IP_PROTOCOL:
                        case SAI_NATIVE_HASH_FIELD_ETHERTYPE:
                        case SAI_NATIVE_HASH_FIELD_L4_SRC_PORT:
                        case SAI_NATIVE_HASH_FIELD_L4_DST_PORT:
                        case SAI_NATIVE_HASH_FIELD_SRC_MAC:
                        case SAI_NATIVE_HASH_FIELD_DST_MAC:
                        case SAI_NATIVE_HASH_FIELD_IN_PORT:
                            // ok
                            break;

                        default:

                            SWSS_LOG_ERROR("invalid hash field value: %d", field);

                            return SAI_STATUS_INVALID_PARAMETER;
                    }
                }
            }

            break;

        case SAI_HASH_ATTR_UDF_GROUP_LIST:

            {
                sai_object_list_t udf_group_list = attr->value.objlist;

                if (udf_group_list.list == NULL)
                {
                    SWSS_LOG_ERROR("udf group list is NULL");

                    return SAI_STATUS_INVALID_PARAMETER;
                }

                for (uint32_t i = 0; i < udf_group_list.count; ++i)
                {
                    sai_object_id_t udf_group = udf_group_list.list[i];

                    if (local_udf_groups_set.find(udf_group) == local_udf_groups_set.end())
                    {
                        SWSS_LOG_ERROR("udf group %llx is missing", udf_group);

                        return SAI_STATUS_INVALID_PARAMETER;
                    }
                }
            }

            break;

        default:

            SWSS_LOG_ERROR("setting attribute id %d is not supported", attr->id);

            return SAI_STATUS_INVALID_PARAMETER;
    }

    sai_status_t status = redis_generic_set(
            SAI_OBJECT_TYPE_HASH,
            hash_id,
            attr);

    return status;
}

/**
 * Routine Description:
 *    @brief Get hash attribute value
 *
 * Arguments:
 *    @param[in] hash_id - hash id
 *    @param[in] attr_count - number of attributes
 *    @param[inout] attrs - array of attributes
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *            Failure status code on error
 */
sai_status_t redis_get_hash_attribute(
    _In_ sai_object_id_t hash_id,
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

    if (local_hashes_set.find(hash_id) == local_hashes_set.end())
    {
        SWSS_LOG_ERROR("hash %llx is missing", hash_id);

        return SAI_STATUS_INVALID_PARAMETER;
    }

    for (uint32_t i = 0; i < attr_count; ++i)
    {
        sai_attribute_t* attr = &attr_list[i];

        switch (attr->id)
        {
            // TODO check for list NULL
            case SAI_HASH_ATTR_NATIVE_FIELD_LIST:
            case SAI_HASH_ATTR_UDF_GROUP_LIST:
                // ok
                break;

            default:

                SWSS_LOG_ERROR("getting attribute id %d is not supported", attr->id);

                return SAI_STATUS_INVALID_PARAMETER;
        }
    }

    sai_status_t status = redis_generic_get(
            SAI_OBJECT_TYPE_HASH,
            hash_id,
            attr_count,
            attr_list);

    return status;
}

/**
 * @brief hash methods, retrieved via sai_api_query()
 */
const sai_hash_api_t redis_hash_api = {
    redis_create_hash,
    redis_remove_hash,
    redis_set_hash_attribute,
    redis_get_hash_attribute,
};
