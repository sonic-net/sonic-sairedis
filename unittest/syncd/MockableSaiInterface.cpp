#include "MockableSaiInterface.h"
#include "swss/logger.h"


MockableSaiInterface::MockableSaiInterface()
{
    SWSS_LOG_ENTER();
}

MockableSaiInterface::~MockableSaiInterface()
{
    SWSS_LOG_ENTER();
}

sai_status_t MockableSaiInterface::initialize(
    _In_ uint64_t flags,
    _In_ const sai_service_method_table_t *service_method_table)
{
    SWSS_LOG_ENTER();
    return SAI_STATUS_SUCCESS;
}

sai_status_t MockableSaiInterface::uninitialize()
{
    SWSS_LOG_ENTER();
    return SAI_STATUS_SUCCESS;
}


sai_status_t MockableSaiInterface::create(
    _In_ sai_object_type_t objectType,
    _Out_ sai_object_id_t* objectId,
    _In_ sai_object_id_t switchId,
    _In_ uint32_t attr_count,
    _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();
    if (mock_create)
    {
        return mock_create(objectType, objectId, switchId, attr_count, attr_list);
    }

    return SAI_STATUS_SUCCESS;
}


sai_status_t MockableSaiInterface::remove(
    _In_ sai_object_type_t objectType,
    _In_ sai_object_id_t objectId)
{
    SWSS_LOG_ENTER();
    if (mock_remove)
    {
        return mock_remove(objectType, objectId);
    }

    return SAI_STATUS_SUCCESS;
}

sai_status_t MockableSaiInterface::set(
    _In_ sai_object_type_t objectType,
    _In_ sai_object_id_t objectId,
    _In_ const sai_attribute_t *attr)
{
    SWSS_LOG_ENTER();
    if (mock_set)
    {
        return mock_set(objectType, objectId, attr);
    }

    return SAI_STATUS_SUCCESS;
}

sai_status_t MockableSaiInterface::get(
    _In_ sai_object_type_t objectType,
    _In_ sai_object_id_t objectId,
    _In_ uint32_t attr_count,
    _Inout_ sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();
    if (mock_get)
    {
        return mock_get(objectType, objectId, attr_count, attr_list);
    }

    return SAI_STATUS_SUCCESS;
}

sai_status_t MockableSaiInterface::bulkCreate(
    _In_ sai_object_type_t object_type,
    _In_ sai_object_id_t switch_id,
    _In_ uint32_t object_count,
    _In_ const uint32_t *attr_count,
    _In_ const sai_attribute_t **attr_list,
    _In_ sai_bulk_op_error_mode_t mode,
    _Out_ sai_object_id_t *object_id,
    _Out_ sai_status_t *object_statuses)
{
    SWSS_LOG_ENTER();
    if (mock_bulkCreate)
    {
        return mock_bulkCreate(object_type, switch_id, object_count, attr_count, attr_list, mode, object_id, object_statuses);
    }

    return SAI_STATUS_SUCCESS;
}

sai_status_t MockableSaiInterface::bulkRemove(
    _In_ sai_object_type_t object_type,
    _In_ uint32_t object_count,
    _In_ const sai_object_id_t *object_id,
    _In_ sai_bulk_op_error_mode_t mode,
    _Out_ sai_status_t *object_statuses)
{
    SWSS_LOG_ENTER();
    if (mock_bulkRemove)
    {
        return mock_bulkRemove(object_type, object_count, object_id, mode, object_statuses);
    }

    return SAI_STATUS_SUCCESS;
}

sai_status_t MockableSaiInterface::bulkSet(
    _In_ sai_object_type_t object_type,
    _In_ uint32_t object_count,
    _In_ const sai_object_id_t *object_id,
    _In_ const sai_attribute_t *attr_list,
    _In_ sai_bulk_op_error_mode_t mode,
    _Out_ sai_status_t *object_statuses)
{
    SWSS_LOG_ENTER();
    if (mock_bulkSet)
    {
        return mock_bulkSet(object_type, object_count, object_id, attr_list, mode, object_statuses);
    }

    return SAI_STATUS_SUCCESS;
}

sai_status_t MockableSaiInterface::getStats(
    _In_ sai_object_type_t object_type,
    _In_ sai_object_id_t object_id,
    _In_ uint32_t number_of_counters,
    _In_ const sai_stat_id_t *counter_ids,
    _Out_ uint64_t *counters)
{
    SWSS_LOG_ENTER();
    if (mock_getStats)
    {
        return mock_getStats(object_type, object_id, number_of_counters, counter_ids, counters);
    }

    return SAI_STATUS_SUCCESS;
}

sai_status_t MockableSaiInterface::queryStatsCapability(
    _In_ sai_object_id_t switch_id,
    _In_ sai_object_type_t object_type,
    _Inout_ sai_stat_capability_list_t *stats_capability)
{
    SWSS_LOG_ENTER();
    if (mock_queryStatsCapability)
    {
        return mock_queryStatsCapability(switch_id, object_type, stats_capability);
    }

    return SAI_STATUS_SUCCESS;
}

sai_status_t MockableSaiInterface::getStatsExt(
    _In_ sai_object_type_t object_type,
    _In_ sai_object_id_t object_id,
    _In_ uint32_t number_of_counters,
    _In_ const sai_stat_id_t *counter_ids,
    _In_ sai_stats_mode_t mode,
    _Out_ uint64_t *counters)
{
    SWSS_LOG_ENTER();
    if (mock_getStatsExt)
    {
        return mock_getStatsExt(object_type, object_id, number_of_counters, counter_ids, mode, counters);
    }

    return SAI_STATUS_SUCCESS;
}

sai_status_t MockableSaiInterface::clearStats(
    _In_ sai_object_type_t object_type,
    _In_ sai_object_id_t object_id,
    _In_ uint32_t number_of_counters,
    _In_ const sai_stat_id_t *counter_ids)
{
    SWSS_LOG_ENTER();
    if (mock_clearStats)
    {
        return mock_clearStats(object_type, object_id, number_of_counters, counter_ids);
    }

    return SAI_STATUS_SUCCESS;
}

sai_status_t MockableSaiInterface::flushFdbEntries(
    _In_ sai_object_id_t switchId,
    _In_ uint32_t attrCount,
    _In_ const sai_attribute_t *attrList)
{
    SWSS_LOG_ENTER();
    if (mock_flushFdbEntries)
    {
        return mock_flushFdbEntries(switchId, attrCount, attrList);
    }

    return SAI_STATUS_SUCCESS;
}

sai_status_t MockableSaiInterface::objectTypeGetAvailability(
    _In_ sai_object_id_t switchId,
    _In_ sai_object_type_t objectType,
    _In_ uint32_t attrCount,
    _In_ const sai_attribute_t *attrList,
    _Out_ uint64_t *count)
{
    SWSS_LOG_ENTER();
    if (mock_objectTypeGetAvailability)
    {
        return mock_objectTypeGetAvailability(switchId, objectType, attrCount, attrList, count);
    }

    return SAI_STATUS_SUCCESS;
}

sai_status_t MockableSaiInterface::queryAttributeCapability(
    _In_ sai_object_id_t switch_id,
    _In_ sai_object_type_t object_type,
    _In_ sai_attr_id_t attr_id,
    _Out_ sai_attr_capability_t *capability)
{
    SWSS_LOG_ENTER();
    if (mock_queryAttributeCapability)
    {
        return mock_queryAttributeCapability(switch_id, object_type, attr_id, capability);
    }

    return SAI_STATUS_SUCCESS;
}

sai_status_t MockableSaiInterface::queryAattributeEnumValuesCapability(
    _In_ sai_object_id_t switch_id,
    _In_ sai_object_type_t object_type,
    _In_ sai_attr_id_t attr_id,
    _Inout_ sai_s32_list_t *enum_values_capability)
{
    SWSS_LOG_ENTER();
    if (mock_queryAattributeEnumValuesCapability)
    {
        return mock_queryAattributeEnumValuesCapability(switch_id, object_type, attr_id, enum_values_capability);
    }

    return SAI_STATUS_SUCCESS;
}

sai_object_type_t MockableSaiInterface::objectTypeQuery(
    _In_ sai_object_id_t objectId)
{
    SWSS_LOG_ENTER();
    if (mock_objectTypeQuery)
    {
        return mock_objectTypeQuery(objectId);
    }

    return SAI_OBJECT_TYPE_NULL;
}

sai_object_id_t MockableSaiInterface::switchIdQuery(
    _In_ sai_object_id_t objectId)
{
    SWSS_LOG_ENTER();
    if (mock_switchIdQuery)
    {
        return mock_switchIdQuery(objectId);
    }

    return 0;
}

sai_status_t MockableSaiInterface::logSet(
    _In_ sai_api_t api,
    _In_ sai_log_level_t log_level)
{
    SWSS_LOG_ENTER();
    if (mock_logSet)
    {
        return mock_logSet(api, log_level);
    }

    return SAI_STATUS_SUCCESS;
}
