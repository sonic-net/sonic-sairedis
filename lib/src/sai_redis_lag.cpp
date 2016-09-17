#include "sai_redis.h"

std::set<sai_object_id_t> local_lags_set;
std::set<sai_object_id_t> local_lag_members_set;

/**
 * Routine Description:
 *    @brief Create LAG
 *
 * Arguments:
 *    @param[out] lag_id - LAG id
 *    @param[in] attr_count - number of attributes
 *    @param[in] attr_list - array of attributes
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *            Failure status code on error
 */
sai_status_t redis_create_lag(
    _Out_ sai_object_id_t* lag_id,
    _In_ uint32_t attr_count,
    _In_ const sai_attribute_t *attr_list)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    if (attr_count != 0)
    {
        SWSS_LOG_ERROR("attribute count should be zero");

        return SAI_STATUS_INVALID_PARAMETER;
    }

    sai_status_t status = redis_generic_create(
            SAI_OBJECT_TYPE_LAG,
            lag_id,
            attr_count,
            attr_list);

    if (status == SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_DEBUG("inserting lag %llx to local state", *lag_id);

        local_lags_set.insert(*lag_id);

        // TODO increase reference count for used object ids
    }

    return status;
}

/**
 * Routine Description:
 *    @brief Remove LAG
 *
 * Arguments:
 *    @param[in] lag_id - LAG id
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *            Failure status code on error
 */
sai_status_t redis_remove_lag(
    _In_ sai_object_id_t lag_id)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    if (local_lags_set.find(lag_id) == local_lags_set.end())
    {
        SWSS_LOG_ERROR("lag %llx is missing", lag_id);

        return SAI_STATUS_INVALID_PARAMETER;
    }

    // TODO check dependencies if lag can be safely removed

    sai_status_t status = redis_generic_remove(
            SAI_OBJECT_TYPE_LAG,
            lag_id);

    if (status == SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_DEBUG("erasing lag %llx from local state", lag_id);

        local_lags_set.erase(lag_id);
    }

    return status;
}

/**
 * Routine Description:
 *    @brief Set LAG attribute
 *
 * Arguments:
 *    @param[in] lag_id - LAG id
 *    @param[in] attr - Structure containing ID and value to be set
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *            Failure status code on error
 */
sai_status_t redis_set_lag_attribute(
    _In_ sai_object_id_t lag_id,
    _In_ const sai_attribute_t *attr)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    if (attr == NULL)
    {
        SWSS_LOG_ERROR("attribute parameter is NULL");

        return SAI_STATUS_INVALID_PARAMETER;
    }

    if (local_lags_set.find(lag_id) == local_lags_set.end())
    {
        SWSS_LOG_ERROR("lag %llx is missing", lag_id);

        return SAI_STATUS_INVALID_PARAMETER;
    }

    switch (attr->id)
    {
        // currently lag don't have attributes that can be set
        default:

            SWSS_LOG_ERROR("setting attribute id %d is not supported", attr->id);

            return SAI_STATUS_INVALID_PARAMETER;
    }

    sai_status_t status = redis_generic_set(
            SAI_OBJECT_TYPE_LAG,
            lag_id,
            attr);

    return status;
}

/**
 * Routine Description:
 *    @brief Get LAG attribute
 *
 * Arguments:
 *    @param[in] lag_id - LAG id
 *    @param[in] attr_count - number of attributes
 *    @param[inout] attr_list - array of attributes
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *            Failure status code on error
 */
sai_status_t redis_get_lag_attribute(
    _In_ sai_object_id_t lag_id,
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

    if (local_lags_set.find(lag_id) == local_lags_set.end())
    {
        SWSS_LOG_ERROR("lag %llx is missing", lag_id);

        return SAI_STATUS_INVALID_PARAMETER;
    }

    for (uint32_t i = 0; i < attr_count; ++i)
    {
        sai_attribute_t* attr = &attr_list[i];

        switch (attr->id)
        {
            case SAI_LAG_ATTR_PORT_LIST:

                {
                    sai_object_list_t port_list = attr->value.objlist;

                    // TODO check if null is supported to obtain correct count
                    if (port_list.list == NULL)
                    {
                        SWSS_LOG_ERROR("port list is null in attribute %d", attr->id);

                        return SAI_STATUS_INVALID_PARAMETER;
                    }
                }

                break;

            default:

                SWSS_LOG_ERROR("getting attribute id %d is not supported", attr->id);

                return SAI_STATUS_INVALID_PARAMETER;
        }
    }

    sai_status_t status = redis_generic_get(
            SAI_OBJECT_TYPE_LAG,
            lag_id,
            attr_count,
            attr_list);

    return status;
}

/**
 * Routine Description:
 *    @brief Create LAG member
 *
 * Arguments:
 *    @param[out] lag_member_id - LAG member id
 *    @param[in] attr_count - number of attributes
 *    @param[in] attr_list - array of attributes
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *            Failure status code on error
 */
sai_status_t redis_create_lag_member(
    _Out_ sai_object_id_t* lag_member_id,
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

    if (attr_count < 2)
    {
        SWSS_LOG_ERROR("attribute count must be at least 2");

        // SAI_LAG_MEMBER_ATTR_LAG_ID
        // SAI_LAG_MEMBER_ATTR_PORT_ID

        return SAI_STATUS_INVALID_PARAMETER;
    }

    const sai_attribute_t* attr_lag_id = redis_get_attribute_by_id(SAI_LAG_MEMBER_ATTR_LAG_ID, attr_count, attr_list);
    const sai_attribute_t* attr_port_id = redis_get_attribute_by_id(SAI_LAG_MEMBER_ATTR_PORT_ID, attr_count, attr_list);

    if (attr_lag_id == NULL)
    {
        SWSS_LOG_ERROR("missing lag id attribute");

        return SAI_STATUS_MANDATORY_ATTRIBUTE_MISSING;
    }

    sai_object_id_t lag_id = attr_lag_id->value.oid;

    if (local_lags_set.find(lag_id) == local_lags_set.end())
    {
        SWSS_LOG_ERROR("lag %llx is missing", lag_id);

        return SAI_STATUS_INVALID_PARAMETER;
    }

    if (attr_port_id == NULL)
    {
        SWSS_LOG_ERROR("missing port id attribute");

        return SAI_STATUS_MANDATORY_ATTRIBUTE_MISSING;
    }

    sai_object_id_t port_id = attr_port_id->value.oid;

    sai_object_type_t port_type = sai_object_type_query(port_id);

    if (port_type != SAI_OBJECT_TYPE_PORT)
    {
        SWSS_LOG_ERROR("port id type is not SAI_OBJECT_TYPE_PORT: %d, id: %llx", port_type, port_id);

        return SAI_STATUS_INVALID_PARAMETER;
    }

    // TODO check if this port actually exists (it can be logical port)
    // TODO check if this port is not already a member in this LAG, can it be duplicated?

    sai_status_t status = redis_generic_create(
            SAI_OBJECT_TYPE_LAG_MEMBER,
            lag_member_id,
            attr_count,
            attr_list);

    if (status == SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_DEBUG("inserting lag member %llx to local state", *lag_member_id);

        local_lag_members_set.insert(*lag_member_id);

        // TODO increase reference count for used object ids
    }

    return status;
}

/**
 * Routine Description:
 *    @brief Remove LAG member
 *
 * Arguments:
 *    @param[in] lag_member_id - lag member id
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *            Failure status code on error
 */
sai_status_t redis_remove_lag_member(
    _In_ sai_object_id_t lag_member_id)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    // TODO check if it safe to remove lag member
    // since lag member is leaf it should be always safe to remove lag member
    // but can there exist lag without members ?

    if (local_lag_members_set.find(lag_member_id) == local_lag_members_set.end())
    {
        SWSS_LOG_ERROR("lag member %llx is missing", lag_member_id);

        return SAI_STATUS_INVALID_PARAMETER;
    }

    sai_status_t status = redis_generic_remove(
            SAI_OBJECT_TYPE_LAG_MEMBER,
            lag_member_id);

    if (status == SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_DEBUG("erasing lag member %llx from local state", lag_member_id);

        local_lag_members_set.erase(lag_member_id);
    }

    return status;
}

/**
 * Routine Description:
 *    @brief Set LAG member attribute
 *
 * Arguments:
 *    @param[in] lag_member_id - LAG member id
 *    @param[in] attr - attribute
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *            Failure status code on error
 */
sai_status_t redis_set_lag_member_attribute(
    _In_ sai_object_id_t lag_member_id,
    _In_ const sai_attribute_t *attr)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    if (attr == NULL)
    {
        SWSS_LOG_ERROR("attribute parameter is NULL");

        return SAI_STATUS_INVALID_PARAMETER;
    }

    if (local_lag_members_set.find(lag_member_id) == local_lag_members_set.end())
    {
        SWSS_LOG_ERROR("lag member %llx is missing", lag_member_id);

        return SAI_STATUS_INVALID_PARAMETER;
    }

    switch (attr->id)
    {
        case SAI_LAG_MEMBER_ATTR_EGRESS_DISABLE:
        case SAI_LAG_MEMBER_ATTR_INGRESS_DISABLE:
            // ok
            break;

        default:

            SWSS_LOG_ERROR("setting attribute id %d is not supported", attr->id);

            return SAI_STATUS_INVALID_PARAMETER;
    }

    sai_status_t status = redis_generic_set(
            SAI_OBJECT_TYPE_LAG_MEMBER,
            lag_member_id,
            attr);

    return status;
}

/**
 * Routine Description:
 *    @brief Get LAG attribute
 *
 * Arguments:
 *    @param[in] lag_member_id - LAG member id
 *    @param[in] attr_count - number of attributes
 *    @param[inout] attr_list - array of attributes
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *            Failure status code on error
 */
sai_status_t redis_get_lag_member_attribute(
    _In_ sai_object_id_t lag_member_id,
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

    if (local_lag_members_set.find(lag_member_id) == local_lag_members_set.end())
    {
        SWSS_LOG_ERROR("lag member %llx is missing", lag_member_id);

        return SAI_STATUS_INVALID_PARAMETER;
    }

    for (uint32_t i = 0; i < attr_count; ++i)
    {
        sai_attribute_t* attr = &attr_list[i];

        switch (attr->id)
        {
            case SAI_LAG_MEMBER_ATTR_LAG_ID:
            case SAI_LAG_MEMBER_ATTR_PORT_ID:
            case SAI_LAG_MEMBER_ATTR_EGRESS_DISABLE:
            case SAI_LAG_MEMBER_ATTR_INGRESS_DISABLE:
                // ok
                break;

            default:

                SWSS_LOG_ERROR("getting attribute id %d is not supported", attr->id);

                return SAI_STATUS_INVALID_PARAMETER;
        }
    }

    sai_status_t status = redis_generic_get(
            SAI_OBJECT_TYPE_LAG_MEMBER,
            lag_member_id,
            attr_count,
            attr_list);

    return status;
}

/**
 * @brief LAG methods table retrieved with sai_api_query()
 */
const sai_lag_api_t redis_lag_api = {
   redis_create_lag,
   redis_remove_lag,
   redis_set_lag_attribute,
   redis_get_lag_attribute,
   redis_create_lag_member,
   redis_remove_lag_member,
   redis_set_lag_member_attribute,
   redis_get_lag_member_attribute,
};
