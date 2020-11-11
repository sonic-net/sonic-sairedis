#include "DummySaiInterface.h"

#include "swss/logger.h"

#include <memory>

using namespace saimeta;

DummySaiInterface::DummySaiInterface()
{
    SWSS_LOG_ENTER();

    m_status = SAI_STATUS_SUCCESS;
}

void DummySaiInterface::setStatus(
        _In_ sai_status_t status)
{
    SWSS_LOG_ENTER();

    m_status = status;
}

sai_status_t DummySaiInterface::initialize(
        _In_ uint64_t flags,
        _In_ const sai_service_method_table_t *service_method_table)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_SUCCESS;
}

sai_status_t  DummySaiInterface::uninitialize(void)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_SUCCESS;
}

sai_status_t DummySaiInterface::create(
        _In_ sai_object_type_t objectType,
        _Out_ sai_object_id_t* objectId,
        _In_ sai_object_id_t switchId,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    return m_status;
}

sai_status_t DummySaiInterface::remove(
        _In_ sai_object_type_t objectType,
        _In_ sai_object_id_t objectId)
{
    SWSS_LOG_ENTER();

    return m_status;
}

sai_status_t DummySaiInterface::set(
        _In_ sai_object_type_t objectType,
        _In_ sai_object_id_t objectId,
        _In_ const sai_attribute_t *attr)
{
    SWSS_LOG_ENTER();

    return m_status;
}

sai_status_t DummySaiInterface::get(
        _In_ sai_object_type_t objectType,
        _In_ sai_object_id_t objectId,
        _In_ uint32_t attr_count,
        _Inout_ sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    return m_status;
}

#define DECLARE_REMOVE_ENTRY(OT,ot)                 \
sai_status_t DummySaiInterface::remove(             \
        _In_ const sai_ ## ot ## _t* ot)            \
{                                                   \
    SWSS_LOG_ENTER();                               \
    return m_status;                                \
}

#define DECLARE_CREATE_ENTRY(OT,ot)                 \
sai_status_t DummySaiInterface::create(             \
        _In_ const sai_ ## ot ## _t* ot,            \
        _In_ uint32_t attr_count,                   \
        _In_ const sai_attribute_t *attr_list)      \
{                                                   \
    SWSS_LOG_ENTER();                               \
    return m_status;                                \
}

#define DECLARE_SET_ENTRY(OT,ot)                    \
sai_status_t DummySaiInterface::set(                \
        _In_ const sai_ ## ot ## _t* ot,            \
        _In_ const sai_attribute_t *attr)           \
{                                                   \
    SWSS_LOG_ENTER();                               \
    return m_status;                                \
}

#define DECLARE_GET_ENTRY(OT,ot)                    \
sai_status_t DummySaiInterface::get(                \
        _In_ const sai_ ## ot ## _t* ot,            \
        _In_ uint32_t attr_count,                   \
        _Inout_ sai_attribute_t *attr_list)         \
{                                                   \
    SWSS_LOG_ENTER();                               \
    return m_status;                                \
}

#define DECLARE_ALL(X)                  \
    X(FDB_ENTRY,fdb_entry)              \
    X(INSEG_ENTRY,inseg_entry);         \
    X(IPMC_ENTRY,ipmc_entry);           \
    X(L2MC_ENTRY,l2mc_entry);           \
    X(MCAST_FDB_ENTRY,mcast_fdb_entry); \
    X(NEIGHBOR_ENTRY,neighbor_entry);   \
    X(ROUTE_ENTRY,route_entry);         \
    X(NAT_ENTRY,nat_entry);

DECLARE_ALL(DECLARE_REMOVE_ENTRY);
DECLARE_ALL(DECLARE_CREATE_ENTRY);
DECLARE_ALL(DECLARE_SET_ENTRY);
DECLARE_ALL(DECLARE_GET_ENTRY);

sai_status_t DummySaiInterface::flushFdbEntries(
        _In_ sai_object_id_t switchId,
        _In_ uint32_t attrCount,
        _In_ const sai_attribute_t *attrList)
{
    SWSS_LOG_ENTER();

    return m_status;
}

sai_status_t DummySaiInterface::objectTypeGetAvailability(
        _In_ sai_object_id_t switchId,
        _In_ sai_object_type_t objectType,
        _In_ uint32_t attrCount,
        _In_ const sai_attribute_t *attrList,
        _Out_ uint64_t *count)
{
    SWSS_LOG_ENTER();

    return m_status;
}

sai_status_t DummySaiInterface::queryAttributeCapability(
        _In_ sai_object_id_t switchId,
        _In_ sai_object_type_t objectType,
        _In_ sai_attr_id_t attrId,
        _Out_ sai_attr_capability_t *capability)
{
    SWSS_LOG_ENTER();

    return m_status;
}

sai_status_t DummySaiInterface::queryAattributeEnumValuesCapability(
        _In_ sai_object_id_t switchId,
        _In_ sai_object_type_t objectType,
        _In_ sai_attr_id_t attrId,
        _Inout_ sai_s32_list_t *enumValuesCapability)
{
    SWSS_LOG_ENTER();

    return m_status;
}

sai_status_t DummySaiInterface::getStats(
        _In_ sai_object_type_t object_type,
        _In_ sai_object_id_t object_id,
        _In_ uint32_t number_of_counters,
        _In_ const sai_stat_id_t *counter_ids,
        _Out_ uint64_t *counters)
{
    SWSS_LOG_ENTER();

    // TODO add recording

    return m_status;
}

sai_status_t DummySaiInterface::getStatsExt(
        _In_ sai_object_type_t object_type,
        _In_ sai_object_id_t object_id,
        _In_ uint32_t number_of_counters,
        _In_ const sai_stat_id_t *counter_ids,
        _In_ sai_stats_mode_t mode,
        _Out_ uint64_t *counters)
{
    SWSS_LOG_ENTER();

    return m_status;
}

sai_status_t DummySaiInterface::clearStats(
        _In_ sai_object_type_t object_type,
        _In_ sai_object_id_t object_id,
        _In_ uint32_t number_of_counters,
        _In_ const sai_stat_id_t *counter_ids)
{
    SWSS_LOG_ENTER();

    return m_status;
}

// bulk QUAD

sai_status_t DummySaiInterface::bulkRemove(
        _In_ sai_object_type_t object_type,
        _In_ uint32_t object_count,
        _In_ const sai_object_id_t *object_id,
        _In_ sai_bulk_op_error_mode_t mode,
        _Out_ sai_status_t *object_statuses)
{
    SWSS_LOG_ENTER();

    for (uint32_t idx = 0; idx < object_count; idx++)
        object_statuses[idx] = m_status;

    return m_status;
}

sai_status_t DummySaiInterface::bulkRemove(
        _In_ uint32_t object_count,
        _In_ const sai_route_entry_t *route_entry,
        _In_ sai_bulk_op_error_mode_t mode,
        _Out_ sai_status_t *object_statuses)
{
    SWSS_LOG_ENTER();

    for (uint32_t idx = 0; idx < object_count; idx++)
        object_statuses[idx] = m_status;

    return m_status;
}

sai_status_t DummySaiInterface::bulkRemove(
        _In_ uint32_t object_count,
        _In_ const sai_nat_entry_t *nat_entry,
        _In_ sai_bulk_op_error_mode_t mode,
        _Out_ sai_status_t *object_statuses)
{
    SWSS_LOG_ENTER();

    for (uint32_t idx = 0; idx < object_count; idx++)
        object_statuses[idx] = m_status;

    return m_status;
}

sai_status_t DummySaiInterface::bulkRemove(
        _In_ uint32_t object_count,
        _In_ const sai_fdb_entry_t *fdb_entry,
        _In_ sai_bulk_op_error_mode_t mode,
        _Out_ sai_status_t *object_statuses)
{
    SWSS_LOG_ENTER();

    for (uint32_t idx = 0; idx < object_count; idx++)
        object_statuses[idx] = m_status;

    return m_status;
}


sai_status_t DummySaiInterface::bulkSet(
        _In_ sai_object_type_t object_type,
        _In_ uint32_t object_count,
        _In_ const sai_object_id_t *object_id,
        _In_ const sai_attribute_t *attr_list,
        _In_ sai_bulk_op_error_mode_t mode,
        _Out_ sai_status_t *object_statuses)
{
    SWSS_LOG_ENTER();

    for (uint32_t idx = 0; idx < object_count; idx++)
        object_statuses[idx] = m_status;

    return m_status;
}

sai_status_t DummySaiInterface::bulkSet(
        _In_ uint32_t object_count,
        _In_ const sai_route_entry_t *route_entry,
        _In_ const sai_attribute_t *attr_list,
        _In_ sai_bulk_op_error_mode_t mode,
        _Out_ sai_status_t *object_statuses)
{
    SWSS_LOG_ENTER();

    for (uint32_t idx = 0; idx < object_count; idx++)
        object_statuses[idx] = m_status;

    return m_status;
}

sai_status_t DummySaiInterface::bulkSet(
        _In_ uint32_t object_count,
        _In_ const sai_nat_entry_t *nat_entry,
        _In_ const sai_attribute_t *attr_list,
        _In_ sai_bulk_op_error_mode_t mode,
        _Out_ sai_status_t *object_statuses)
{
    SWSS_LOG_ENTER();

    for (uint32_t idx = 0; idx < object_count; idx++)
        object_statuses[idx] = m_status;

    return m_status;
}

sai_status_t DummySaiInterface::bulkSet(
        _In_ uint32_t object_count,
        _In_ const sai_fdb_entry_t *fdb_entry,
        _In_ const sai_attribute_t *attr_list,
        _In_ sai_bulk_op_error_mode_t mode,
        _Out_ sai_status_t *object_statuses)
{
    SWSS_LOG_ENTER();

    for (uint32_t idx = 0; idx < object_count; idx++)
        object_statuses[idx] = m_status;

    return m_status;
}

sai_status_t DummySaiInterface::bulkCreate(
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

    for (uint32_t idx = 0; idx < object_count; idx++)
        object_statuses[idx] = m_status;

    return m_status;
}

sai_status_t DummySaiInterface::bulkCreate(
        _In_ uint32_t object_count,
        _In_ const sai_route_entry_t *route_entry,
        _In_ const uint32_t *attr_count,
        _In_ const sai_attribute_t **attr_list,
        _In_ sai_bulk_op_error_mode_t mode,
        _Out_ sai_status_t *object_statuses)
{
    SWSS_LOG_ENTER();

    for (uint32_t idx = 0; idx < object_count; idx++)
        object_statuses[idx] = m_status;

    return m_status;
}

sai_status_t DummySaiInterface::bulkCreate(
        _In_ uint32_t object_count,
        _In_ const sai_fdb_entry_t *fdb_entry,
        _In_ const uint32_t *attr_count,
        _In_ const sai_attribute_t **attr_list,
        _In_ sai_bulk_op_error_mode_t mode,
        _Out_ sai_status_t *object_statuses)
{
    SWSS_LOG_ENTER();

    for (uint32_t idx = 0; idx < object_count; idx++)
        object_statuses[idx] = m_status;

    return m_status;
}

sai_status_t DummySaiInterface::bulkCreate(
        _In_ uint32_t object_count,
        _In_ const sai_nat_entry_t *nat_entry,
        _In_ const uint32_t *attr_count,
        _In_ const sai_attribute_t **attr_list,
        _In_ sai_bulk_op_error_mode_t mode,
        _Out_ sai_status_t *object_statuses)
{
    SWSS_LOG_ENTER();

    for (uint32_t idx = 0; idx < object_count; idx++)
        object_statuses[idx] = m_status;

    return m_status;
}

sai_object_type_t DummySaiInterface::objectTypeQuery(
        _In_ sai_object_id_t objectId)
{
    SWSS_LOG_ENTER();

    if (m_status != SAI_STATUS_SUCCESS)
        SWSS_LOG_THROW("not implemented");

    return SAI_OBJECT_TYPE_NULL;
}

sai_object_id_t DummySaiInterface::switchIdQuery(
        _In_ sai_object_id_t objectId)
{
    SWSS_LOG_ENTER();

    if (m_status != SAI_STATUS_SUCCESS)
        SWSS_LOG_THROW("not implemented");

    return SAI_NULL_OBJECT_ID;
}

sai_status_t DummySaiInterface::logSet(
        _In_ sai_api_t api,
        _In_ sai_log_level_t log_level)
{
    SWSS_LOG_ENTER();

    return m_status;
}
