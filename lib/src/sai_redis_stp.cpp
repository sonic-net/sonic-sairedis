#include "sai_redis.h"

std::set<sai_object_id_t> local_stp_instances_set;

/**
 * @brief Create stp instance with default port state as forwarding.
 *
 * @param[out] stp_id stp instance id
 * @param[in] attr_count Number of attributes
 * @param[in] attr_list Value of attributes
 * @return SAI_STATUS_SUCCESS if operation is successful otherwise a different
 *   error code is returned.
 */
sai_status_t redis_create_stp(
    _Out_ sai_object_id_t *stp_id,
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

    sai_status_t status = redis_generic_create(
            SAI_OBJECT_TYPE_STP_INSTANCE,
            stp_id,
            attr_count,
            attr_list);

    if (status == SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_DEBUG("inserting stp %llx to local state", *stp_id);

        local_stp_instances_set.insert(*stp_id);

        // TODO increase reference count for used object ids
    }

    return status;
}

/**
 * @brief Remove stp instance.
 *
 * @param[in] stp_id stp instance id
 * @return SAI_STATUS_SUCCESS if operation is successful otherwise a different
 *   error code is returned.
 */
sai_status_t redis_remove_stp(
       _In_ sai_object_id_t stp_id)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    // TODO check if it safe to remove stp

    if (local_stp_instances_set.find(stp_id) == local_stp_instances_set.end())
    {
        SWSS_LOG_ERROR("stp %llx is missing", stp_id);

        return SAI_STATUS_INVALID_PARAMETER;
    }

    sai_status_t status = redis_generic_remove(
            SAI_OBJECT_TYPE_STP_INSTANCE,
            stp_id);

    if (status == SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_DEBUG("erasing stp %llx from local state", stp_id);

        // TODO decrease reference count

        local_stp_instances_set.erase(stp_id);
    }

    return status;
}

/**
 * @brief Set the attribute of STP instance.
 *
 * @param[in] stp_id stp instance id
 * @param[in] attr attribute value
 * @return SAI_STATUS_SUCCESS if operation is successful otherwise a different
 *   error code is returned.
 */
sai_status_t redis_set_stp_attribute(
    _In_ sai_object_id_t stp_id,
    _In_ const sai_attribute_t *attr)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    if (attr == NULL)
    {
        SWSS_LOG_ERROR("attribute parameter is NULL");

        return SAI_STATUS_INVALID_PARAMETER;
    }

    if (local_stp_instances_set.find(stp_id) == local_stp_instances_set.end())
    {
        SWSS_LOG_ERROR("stp %llx is missing", stp_id);

        return SAI_STATUS_INVALID_PARAMETER;
    }

    switch (attr->id)
    {
        // currently we don't have SET attributes
        default:

            SWSS_LOG_ERROR("setting attribute id %d is not supported", attr->id);

            return SAI_STATUS_INVALID_PARAMETER;
    }

    sai_status_t status = redis_generic_set(
            SAI_OBJECT_TYPE_STP_INSTANCE,
            stp_id,
            attr);

    return status;
}

/**
 * @brief Get the attribute of STP instance.
 *
 * @param[in] stp_id stp instance id
 * @param[in] attr_count number of the attribute
 * @param[in] attr_list attribute value
 * @return SAI_STATUS_SUCCESS if operation is successful otherwise a different
 *   error code is returned.
 */
sai_status_t redis_get_stp_attribute(
    _In_ sai_object_id_t stp_id,
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

    if (local_stp_instances_set.find(stp_id) == local_stp_instances_set.end())
    {
        SWSS_LOG_ERROR("stp %llx is missing", stp_id);

        return SAI_STATUS_INVALID_PARAMETER;
    }

    for (uint32_t i = 0; i < attr_count; ++i)
    {
        sai_attribute_t* attr = &attr_list[i];

        switch (attr->id)
        {
            case SAI_STP_ATTR_VLAN_LIST:

                if (attr->value.vlanlist.list == NULL)
                {
                    SWSS_LOG_ERROR("vlan list is NULL");

                    return SAI_STATUS_INVALID_PARAMETER;
                }

                break;

            default:

                SWSS_LOG_ERROR("getting attribute id %d is not supported", attr->id);

                return SAI_STATUS_INVALID_PARAMETER;
        }
    }

    sai_status_t status = redis_generic_get(
            SAI_OBJECT_TYPE_STP_INSTANCE,
            stp_id,
            attr_count,
            attr_list);

    return status;
}

/**
 * @brief Update stp state of a port in specified stp instance.
 *
 * @param[in] stp_id stp instance id
 * @param[in] port_id port id
 * @param[in] stp_port_state stp state of the port
 * @return SAI_STATUS_SUCCESS if operation is successful otherwise a different
 *   error code is returned.
 */
sai_status_t redis_set_stp_port_state(
    _In_ sai_object_id_t stp_id,
    _In_ sai_object_id_t port_id,
    _In_ sai_port_stp_port_state_t stp_port_state)
{
    SWSS_LOG_ENTER();

    // TODO function signature must be changed to match other types (PR in place)
    SWSS_LOG_ERROR("not implemented");

    return SAI_STATUS_NOT_IMPLEMENTED;
}

/**
 * @brief Retrieve stp state of a port in specified stp instance.
 *
 * @param[in] stp_id stp instance id
 * @param[in] port_id port id
 * @param[out] stp_port_state stp state of the port
 * @return SAI_STATUS_SUCCESS if operation is successful otherwise a different
 *   error code is returned.
 */
sai_status_t redis_get_stp_port_state(
    _In_ sai_object_id_t stp_id,
    _In_ sai_object_id_t port_id,
    _Out_ sai_port_stp_port_state_t *stp_port_state)
{
    SWSS_LOG_ENTER();

    // TODO function signature must be changed to match other types (PR in place)
    SWSS_LOG_ERROR("not implemented");

    return SAI_STATUS_NOT_IMPLEMENTED;
}

/**
 * @brief STP method table retrieved with sai_api_query()
 */
const sai_stp_api_t redis_stp_api = {
    redis_create_stp,
    redis_remove_stp,
    redis_set_stp_attribute,
    redis_get_stp_attribute,
    redis_set_stp_port_state,
    redis_get_stp_port_state,
};
