#include "sai_redis.h"
#include "sairedis.h"
#include "meta/sai_serialize.h"
#include "meta/saiattributelist.h"

sai_status_t redis_dummy_create_route_entry(
        _In_ const sai_route_entry_t *route_entry,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    /*
     * Since we are using validation for each route in bulk operations, we
     * can't execute actual CREATE, we need to do dummy create and then introduce
     * internal bulk_create operation that will only touch redis db only once.
     * So we are returning success here.
     */

    return SAI_STATUS_SUCCESS;
}

sai_status_t redis_bulk_create_route_entry(
        _In_ uint32_t object_count,
        _In_ const sai_route_entry_t *route_entry,
        _In_ const uint32_t *attr_count,
        _In_ const sai_attribute_t **attr_list,
        _In_ sai_bulk_op_error_mode_t mode,
        _Out_ sai_status_t *object_statuses)
{
    MUTEX();

    SWSS_LOG_ENTER();

    if (object_count < 1)
    {
        SWSS_LOG_ERROR("expected at least 1 object to create");

        return SAI_STATUS_INVALID_PARAMETER;
    }

    if (route_entry == NULL)
    {
        SWSS_LOG_ERROR("route_entry is NULL");

        return SAI_STATUS_INVALID_PARAMETER;
    }

    if (attr_count == NULL)
    {
        SWSS_LOG_ERROR("attr_count is NULL");

        return SAI_STATUS_INVALID_PARAMETER;
    }

    if (attr_list == NULL)
    {
        SWSS_LOG_ERROR("attr_list is NULL");

        return SAI_STATUS_INVALID_PARAMETER;
    }

    switch (mode)
    {
        case SAI_BULK_OP_ERROR_MODE_STOP_ON_ERROR:
        case SAI_BULK_OP_ERROR_MODE_IGNORE_ERROR:
             // ok
             break;

        default:

             SWSS_LOG_ERROR("invalid bulk operation mode %d", mode);

             return SAI_STATUS_INVALID_PARAMETER;
    }

    if (object_statuses == NULL)
    {
        SWSS_LOG_ERROR("object_statuses is NULL");

        return SAI_STATUS_INVALID_PARAMETER;
    }

    std::vector<std::string> serialized_object_ids;

    for (uint32_t idx = 0; idx < object_count; ++idx)
    {
        /*
         * At the beginning set all statuses to not executed.
         */

        object_statuses[idx] = SAI_STATUS_NOT_EXECUTED;

        serialized_object_ids.push_back(
                sai_serialize_route_entry(route_entry[idx]));
    }

    for (uint32_t idx = 0; idx < object_count; ++idx)
    {
        sai_status_t status =
            meta_sai_create_route_entry(
                    &route_entry[idx],
                    attr_count[idx],
                    attr_list[idx],
                    &redis_dummy_create_route_entry);

        object_statuses[idx] = status;

        if (status != SAI_STATUS_SUCCESS)
        {
            // TODO add attr id and value

            SWSS_LOG_ERROR("failed on index %u: %s",
                    idx,
                    serialized_object_ids[idx].c_str());

            if (mode == SAI_BULK_OP_ERROR_MODE_STOP_ON_ERROR)
            {
                SWSS_LOG_NOTICE("stop on error since previous operation failed");
                break;
            }
        }
    }

    /*
     * TODO: we need to record operation type
     */

    return internal_redis_bulk_generic_create(
            SAI_OBJECT_TYPE_ROUTE_ENTRY,
            serialized_object_ids,
            attr_count,
            attr_list,
            object_statuses);
}

sai_status_t redis_dummy_remove_route_entry(
        _In_ const sai_route_entry_t *route_entry)
{
    SWSS_LOG_ENTER();

    /*
     * Since we are using validation for each route in bulk operations, we
     * can't execute actual REMOVE, we need to do dummy remove and then introduce
     * internal bulk_remove operation that will only touch redis db only once.
     * So we are returning success here.
     */

    return SAI_STATUS_SUCCESS;
}

sai_status_t redis_bulk_remove_route_entry(
        _In_ uint32_t object_count,
        _In_ const sai_route_entry_t *route_entry,
        _In_ sai_bulk_op_error_mode_t mode,
        _Out_ sai_status_t *object_statuses)
{
    MUTEX();

    SWSS_LOG_ENTER();

    if (object_count < 1)
    {
        SWSS_LOG_ERROR("expected at least 1 object to create");

        return SAI_STATUS_INVALID_PARAMETER;
    }

    if (route_entry == NULL)
    {
        SWSS_LOG_ERROR("route_entry is NULL");

        return SAI_STATUS_INVALID_PARAMETER;
    }

    switch (mode)
    {
        case SAI_BULK_OP_ERROR_MODE_STOP_ON_ERROR:
        case SAI_BULK_OP_ERROR_MODE_IGNORE_ERROR:
             // ok
             break;

        default:

             SWSS_LOG_ERROR("invalid bulk operation mode %d", mode);

             return SAI_STATUS_INVALID_PARAMETER;
    }

    if (object_statuses == NULL)
    {
        SWSS_LOG_ERROR("object_statuses is NULL");

        return SAI_STATUS_INVALID_PARAMETER;
    }

    std::vector<std::string> serialized_object_ids;

    for (uint32_t idx = 0; idx < object_count; ++idx)
    {
        /*
         * At the beginning set all statuses to not executed.
         */

        object_statuses[idx] = SAI_STATUS_NOT_EXECUTED;

        serialized_object_ids.push_back(
                sai_serialize_route_entry(route_entry[idx]));
    }

    for (uint32_t idx = 0; idx < object_count; ++idx)
    {
        sai_status_t status =
            meta_sai_remove_route_entry(
                    &route_entry[idx],
                    &redis_dummy_remove_route_entry);

        object_statuses[idx] = status;

        if (status != SAI_STATUS_SUCCESS)
        {
            // TODO add attr id and value

            SWSS_LOG_ERROR("failed on index %u: %s",
                    idx,
                    serialized_object_ids[idx].c_str());

            if (mode == SAI_BULK_OP_ERROR_MODE_STOP_ON_ERROR)
            {
                SWSS_LOG_NOTICE("stop on error since previous operation failed");
                break;
            }
        }
    }

    return internal_redis_bulk_generic_remove(SAI_OBJECT_TYPE_ROUTE_ENTRY, serialized_object_ids, object_statuses);
}

sai_status_t redis_dummy_set_route_entry(
        _In_ const sai_route_entry_t* unicast_route_entry,
        _In_ const sai_attribute_t *attr)
{
    SWSS_LOG_ENTER();

    /*
     * Since we are using validation for each route in bulk operations, we
     * can't execute actual SET, we need to do dummy set and then introduce
     * internal bulk_set operation that will only touch redis db only once.
     * So we are returning success here.
     */

    return SAI_STATUS_SUCCESS;
}

sai_status_t redis_bulk_set_route_entry_attribute(
        _In_ uint32_t object_count,
        _In_ const sai_route_entry_t *route_entry,
        _In_ const sai_attribute_t *attr_list,
        _In_ sai_bulk_op_error_mode_t mode,
        _Out_ sai_status_t *object_statuses)
{
    MUTEX();

    SWSS_LOG_ENTER();

    if (object_count < 1)
    {
        SWSS_LOG_ERROR("expected at least 1 object to set");

        return SAI_STATUS_INVALID_PARAMETER;
    }

    if (route_entry == NULL)
    {
        SWSS_LOG_ERROR("route_entry is NULL");

        return SAI_STATUS_INVALID_PARAMETER;
    }

    if (attr_list == NULL)
    {
        SWSS_LOG_ERROR("attr_list is NULL");

        return SAI_STATUS_INVALID_PARAMETER;
    }

    switch (mode)
    {
        case SAI_BULK_OP_ERROR_MODE_STOP_ON_ERROR:
        case SAI_BULK_OP_ERROR_MODE_IGNORE_ERROR:
             // ok
             break;

        default:

             SWSS_LOG_ERROR("invalid bulk operation mode %d", mode);
             return SAI_STATUS_INVALID_PARAMETER;
    }

    if (object_statuses == NULL)
    {
        SWSS_LOG_ERROR("object_statuses is NULL");

        return SAI_STATUS_INVALID_PARAMETER;
    }

    std::vector<std::string> serialized_object_ids;

    for (uint32_t idx = 0; idx < object_count; ++idx)
    {
        /*
         * At the beginning set all statuses to not executed.
         */

        object_statuses[idx] = SAI_STATUS_NOT_EXECUTED;

        serialized_object_ids.push_back(
                sai_serialize_route_entry(route_entry[idx]));
    }

    for (uint32_t idx = 0; idx < object_count; ++idx)
    {
        sai_status_t status =
            meta_sai_set_route_entry(
                    &route_entry[idx],
                    &attr_list[idx],
                    &redis_dummy_set_route_entry);

        object_statuses[idx] = status;

        if (status != SAI_STATUS_SUCCESS)
        {
            // TODO add attr id and value

            SWSS_LOG_ERROR("failed on index %u: %s",
                    idx,
                    serialized_object_ids[idx].c_str());

            if (mode == SAI_BULK_OP_ERROR_MODE_STOP_ON_ERROR)
            {
                SWSS_LOG_NOTICE("stop on error since previous operation failed");
                break;
            }
        }
    }

    /*
     * TODO: we need to record operation type
     */

    return internal_redis_bulk_generic_set(
            SAI_OBJECT_TYPE_ROUTE_ENTRY,
            serialized_object_ids,
            attr_list,
            object_statuses);
}

sai_status_t redis_bulk_get_route_entry_attribute(
        _In_ uint32_t object_count,
        _In_ const sai_route_entry_t *route_entry,
        _In_ const uint32_t *attr_count,
        _Inout_ sai_attribute_t **attr_list,
        _In_ sai_bulk_op_error_mode_t mode,
        _Out_ sai_status_t *object_statuses)
{
    MUTEX();

    SWSS_LOG_ENTER();

    return SAI_STATUS_NOT_IMPLEMENTED;
}

REDIS_GENERIC_QUAD_ENTRY(ROUTE_ENTRY,route_entry);

const sai_route_api_t redis_route_api = {

    REDIS_GENERIC_QUAD_API(route_entry)

    redis_bulk_create_route_entry,
    redis_bulk_remove_route_entry,
    redis_bulk_set_route_entry_attribute,
    redis_bulk_get_route_entry_attribute,
};
