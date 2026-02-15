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

sai_status_t MockableSaiInterface::apiInitialize(
    _In_ uint64_t flags,
    _In_ const sai_service_method_table_t *service_method_table)
{
    SWSS_LOG_ENTER();
    return SAI_STATUS_SUCCESS;
}

sai_status_t MockableSaiInterface::apiUninitialize()
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

sai_status_t MockableSaiInterface::bulkGet(
        _In_ sai_object_type_t object_type,
        _In_ uint32_t object_count,
        _In_ const sai_object_id_t *object_id,
        _In_ const uint32_t *attr_count,
        _Inout_ sai_attribute_t **attr_list,
        _In_ sai_bulk_op_error_mode_t mode,
        _Out_ sai_status_t *object_statuses)
{
    SWSS_LOG_ENTER();

    SWSS_LOG_ERROR("not implemented, FIXME");

    return SAI_STATUS_NOT_IMPLEMENTED;
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

sai_status_t MockableSaiInterface::queryStatsStCapability(
    _In_ sai_object_id_t switch_id,
    _In_ sai_object_type_t object_type,
    _Inout_ sai_stat_st_capability_list_t *stats_capability)
{
    SWSS_LOG_ENTER();
    if (mock_queryStatsStCapability)
    {
        return mock_queryStatsStCapability(switch_id, object_type, stats_capability);
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

sai_status_t MockableSaiInterface::bulkGetStats(
    _In_ sai_object_id_t switchId,
    _In_ sai_object_type_t object_type,
    _In_ uint32_t object_count,
    _In_ const sai_object_key_t *object_key,
    _In_ uint32_t number_of_counters,
    _In_ const sai_stat_id_t *counter_ids,
    _In_ sai_stats_mode_t mode,
    _Inout_ sai_status_t *object_statuses,
    _Out_ uint64_t *counters)
{
    SWSS_LOG_ENTER();
    if (mock_bulkGetStats)
    {
        return mock_bulkGetStats(switchId, object_type, object_count, object_key, number_of_counters, counter_ids, mode, object_statuses, counters);
    }

    return SAI_STATUS_SUCCESS;
}

sai_status_t MockableSaiInterface::bulkClearStats(
    _In_ sai_object_id_t switchId,
    _In_ sai_object_type_t object_type,
    _In_ uint32_t object_count,
    _In_ const sai_object_key_t *object_key,
    _In_ uint32_t number_of_counters,
    _In_ const sai_stat_id_t *counter_ids,
    _In_ sai_stats_mode_t mode,
    _Inout_ sai_status_t *object_statuses)
{
    SWSS_LOG_ENTER();
    if (mock_bulkClearStats)
    {
        return mock_bulkClearStats(switchId, object_type, object_count, object_key, number_of_counters, counter_ids, mode, object_statuses);
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

sai_status_t MockableSaiInterface::switchMdioRead(
    _In_ sai_object_id_t switchId,
    _In_ uint32_t device_addr,
    _In_ uint32_t start_reg_addr,
    _In_ uint32_t number_of_registers,
    _Out_ uint32_t *reg_val)
{
    SWSS_LOG_ENTER();
    if (mock_switchMdioRead)
    {
        return mock_switchMdioRead(switchId, device_addr, start_reg_addr, number_of_registers, reg_val);
    }

    return SAI_STATUS_SUCCESS;
}

sai_status_t MockableSaiInterface::switchMdioWrite(
    _In_ sai_object_id_t switchId,
    _In_ uint32_t device_addr,
    _In_ uint32_t start_reg_addr,
    _In_ uint32_t number_of_registers,
    _In_ const uint32_t *reg_val)
{
    SWSS_LOG_ENTER();
    if (mock_switchMdioWrite)
    {
        return mock_switchMdioWrite(switchId, device_addr, start_reg_addr, number_of_registers, reg_val);
    }

    return SAI_STATUS_SUCCESS;
}

sai_status_t MockableSaiInterface::switchMdioCl22Read(
    _In_ sai_object_id_t switchId,
    _In_ uint32_t device_addr,
    _In_ uint32_t start_reg_addr,
    _In_ uint32_t number_of_registers,
    _Out_ uint32_t *reg_val)
{
    SWSS_LOG_ENTER();
    if (mock_switchMdioCl22Read)
    {
        return mock_switchMdioCl22Read(switchId, device_addr, start_reg_addr, number_of_registers, reg_val);
    }

    return SAI_STATUS_SUCCESS;
}

sai_status_t MockableSaiInterface::switchMdioCl22Write(
    _In_ sai_object_id_t switchId,
    _In_ uint32_t device_addr,
    _In_ uint32_t start_reg_addr,
    _In_ uint32_t number_of_registers,
    _In_ const uint32_t *reg_val)
{
    SWSS_LOG_ENTER();
    if (mock_switchMdioCl22Write)
    {
        return mock_switchMdioCl22Write(switchId, device_addr, start_reg_addr, number_of_registers, reg_val);
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

sai_status_t MockableSaiInterface::queryAttributeEnumValuesCapability(
    _In_ sai_object_id_t switch_id,
    _In_ sai_object_type_t object_type,
    _In_ sai_attr_id_t attr_id,
    _Inout_ sai_s32_list_t *enum_values_capability)
{
    SWSS_LOG_ENTER();
    if (mock_queryAttributeEnumValuesCapability)
    {
        return mock_queryAttributeEnumValuesCapability(switch_id, object_type, attr_id, enum_values_capability);
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
