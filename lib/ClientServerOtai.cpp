#include "ClientServerOtai.h"
#include "ClientOtai.h"
#include "ServerOtai.h"
#include "OtaiInternal.h"
#include "Otai.h"

#include "swss/logger.h"

#include "meta/otai_serialize.h"

using namespace otairedis;
using namespace std::placeholders;

#define REDIS_CHECK_API_INITIALIZED()                                       \
    if (!m_apiInitialized) {                                                \
        SWSS_LOG_ERROR("%s: api not initialized", __PRETTY_FUNCTION__);     \
        return OTAI_STATUS_FAILURE; }

ClientServerOtai::ClientServerOtai()
{
    SWSS_LOG_ENTER();

    m_apiInitialized = false;
}

ClientServerOtai::~ClientServerOtai()
{
    SWSS_LOG_ENTER();

    if (m_apiInitialized)
    {
        apiUninitialize();
    }
}

// INITIALIZE UNINITIALIZE

otai_status_t ClientServerOtai::apiInitialize(
        _In_ uint64_t flags,
        _In_ const otai_service_method_table_t *service_method_table)
{
    MUTEX();
    SWSS_LOG_ENTER();

    if (m_apiInitialized)
    {
        SWSS_LOG_ERROR("%s: api already initialized", __PRETTY_FUNCTION__);

        return OTAI_STATUS_FAILURE;
    }

    if (flags != 0)
    {
        SWSS_LOG_ERROR("invalid flags passed to OTAI API initialize");

        return OTAI_STATUS_INVALID_PARAMETER;
    }

    if ((service_method_table == NULL) ||
            (service_method_table->profile_get_next_value == NULL) ||
            (service_method_table->profile_get_value == NULL))
    {
        SWSS_LOG_ERROR("invalid service_method_table handle passed to OTAI API initialize");

        return OTAI_STATUS_INVALID_PARAMETER;
    }

    memcpy(&m_service_method_table, service_method_table, sizeof(m_service_method_table));

    const char* enableClient = service_method_table->profile_get_value(0, OTAI_REDIS_KEY_ENABLE_CLIENT);

    if (enableClient && strcmp(enableClient, "true") == 0)
    {
        SWSS_LOG_NOTICE("acting as a otairedis CLIENT");

        m_otai = std::make_shared<ClientOtai>();
    }
    else
    {
        SWSS_LOG_NOTICE("acting as a otairedis SERVER");

        m_otai = std::make_shared<ServerOtai>();
    }

    auto status = m_otai->apiInitialize(flags, service_method_table);

    SWSS_LOG_NOTICE("init client/server otai: %s", otai_serialize_status(status).c_str());

    if (status == OTAI_STATUS_SUCCESS)
    {
        m_apiInitialized = true;
    }

    return status;
}

otai_status_t ClientServerOtai::apiUninitialize(void)
{
    SWSS_LOG_ENTER();
    REDIS_CHECK_API_INITIALIZED();

    SWSS_LOG_NOTICE("begin");

    m_apiInitialized = false;

    m_otai = nullptr;

    SWSS_LOG_NOTICE("end");

    return OTAI_STATUS_SUCCESS;
}

// QUAD OID

otai_status_t ClientServerOtai::create(
        _In_ otai_object_type_t objectType,
        _Out_ otai_object_id_t* objectId,
        _In_ otai_object_id_t switchId,
        _In_ uint32_t attr_count,
        _In_ const otai_attribute_t *attr_list)
{
    MUTEX();
    SWSS_LOG_ENTER();
    REDIS_CHECK_API_INITIALIZED();

    return m_otai->create(
            objectType,
            objectId,
            switchId,
            attr_count,
            attr_list);
}

otai_status_t ClientServerOtai::remove(
        _In_ otai_object_type_t objectType,
        _In_ otai_object_id_t objectId)
{
    MUTEX();
    SWSS_LOG_ENTER();
    REDIS_CHECK_API_INITIALIZED();

    return m_otai->remove(objectType, objectId);
}

otai_status_t ClientServerOtai::set(
        _In_ otai_object_type_t objectType,
        _In_ otai_object_id_t objectId,
        _In_ const otai_attribute_t *attr)
{
    MUTEX();
    SWSS_LOG_ENTER();
    REDIS_CHECK_API_INITIALIZED();

    return m_otai->set(objectType, objectId, attr);
}

otai_status_t ClientServerOtai::get(
        _In_ otai_object_type_t objectType,
        _In_ otai_object_id_t objectId,
        _In_ uint32_t attr_count,
        _Inout_ otai_attribute_t *attr_list)
{
    MUTEX();
    SWSS_LOG_ENTER();
    REDIS_CHECK_API_INITIALIZED();

    return m_otai->get(
            objectType,
            objectId,
            attr_count,
            attr_list);
}

// QUAD ENTRY

#define DECLARE_CREATE_ENTRY(OT,ot)                                 \
otai_status_t ClientServerOtai::create(                               \
        _In_ const otai_ ## ot ## _t* entry,                         \
        _In_ uint32_t attr_count,                                   \
        _In_ const otai_attribute_t *attr_list)                      \
{                                                                   \
    MUTEX();                                                        \
    SWSS_LOG_ENTER();                                               \
    REDIS_CHECK_API_INITIALIZED();                                  \
    return m_otai->create(entry, attr_count, attr_list);             \
}

OTAIREDIS_DECLARE_EVERY_ENTRY(DECLARE_CREATE_ENTRY);

#define DECLARE_REMOVE_ENTRY(OT,ot)                         \
otai_status_t ClientServerOtai::remove(                       \
        _In_ const otai_ ## ot ## _t* entry)                 \
{                                                           \
    MUTEX();                                                \
    SWSS_LOG_ENTER();                                       \
    REDIS_CHECK_API_INITIALIZED();                          \
    return m_otai->remove(entry);                            \
}

OTAIREDIS_DECLARE_EVERY_ENTRY(DECLARE_REMOVE_ENTRY);

#define DECLARE_SET_ENTRY(OT,ot)                            \
otai_status_t ClientServerOtai::set(                          \
        _In_ const otai_ ## ot ## _t* entry,                 \
        _In_ const otai_attribute_t *attr)                   \
{                                                           \
    MUTEX();                                                \
    SWSS_LOG_ENTER();                                       \
    REDIS_CHECK_API_INITIALIZED();                          \
    return m_otai->set(entry, attr);                         \
}

OTAIREDIS_DECLARE_EVERY_ENTRY(DECLARE_SET_ENTRY);

#define DECLARE_GET_ENTRY(OT,ot)                                \
otai_status_t ClientServerOtai::get(                              \
        _In_ const otai_ ## ot ## _t* entry,                     \
        _In_ uint32_t attr_count,                               \
        _Inout_ otai_attribute_t *attr_list)                     \
{                                                               \
    MUTEX();                                                    \
    SWSS_LOG_ENTER();                                           \
    REDIS_CHECK_API_INITIALIZED();                              \
    return m_otai->get(entry, attr_count, attr_list);            \
}

OTAIREDIS_DECLARE_EVERY_ENTRY(DECLARE_GET_ENTRY);

// STATS

otai_status_t ClientServerOtai::getStats(
        _In_ otai_object_type_t object_type,
        _In_ otai_object_id_t object_id,
        _In_ uint32_t number_of_counters,
        _In_ const otai_stat_id_t *counter_ids,
        _Out_ uint64_t *counters)
{
    MUTEX();
    SWSS_LOG_ENTER();
    REDIS_CHECK_API_INITIALIZED();

    return m_otai->getStats(
            object_type,
            object_id,
            number_of_counters,
            counter_ids,
            counters);
}

otai_status_t ClientServerOtai::getStatsExt(
        _In_ otai_object_type_t object_type,
        _In_ otai_object_id_t object_id,
        _In_ uint32_t number_of_counters,
        _In_ const otai_stat_id_t *counter_ids,
        _In_ otai_stats_mode_t mode,
        _Out_ uint64_t *counters)
{
    MUTEX();
    SWSS_LOG_ENTER();
    REDIS_CHECK_API_INITIALIZED();

    return m_otai->getStatsExt(
            object_type,
            object_id,
            number_of_counters,
            counter_ids,
            mode,
            counters);
}

otai_status_t ClientServerOtai::clearStats(
        _In_ otai_object_type_t object_type,
        _In_ otai_object_id_t object_id,
        _In_ uint32_t number_of_counters,
        _In_ const otai_stat_id_t *counter_ids)
{
    MUTEX();
    SWSS_LOG_ENTER();
    REDIS_CHECK_API_INITIALIZED();

    return m_otai->clearStats(
            object_type,
            object_id,
            number_of_counters,
            counter_ids);
}

otai_status_t ClientServerOtai::bulkGetStats(
        _In_ otai_object_id_t switchId,
        _In_ otai_object_type_t object_type,
        _In_ uint32_t object_count,
        _In_ const otai_object_key_t *object_key,
        _In_ uint32_t number_of_counters,
        _In_ const otai_stat_id_t *counter_ids,
        _In_ otai_stats_mode_t mode,
        _Inout_ otai_status_t *object_statuses,
        _Out_ uint64_t *counters)
{
    MUTEX();
    SWSS_LOG_ENTER();
    REDIS_CHECK_API_INITIALIZED();

    return m_otai->bulkGetStats(switchId,
                               object_type,
                               object_count,
                               object_key,
                               number_of_counters,
                               counter_ids,
                               mode,
                               object_statuses,
                               counters);
}

otai_status_t ClientServerOtai::bulkClearStats(
        _In_ otai_object_id_t switchId,
        _In_ otai_object_type_t object_type,
        _In_ uint32_t object_count,
        _In_ const otai_object_key_t *object_key,
        _In_ uint32_t number_of_counters,
        _In_ const otai_stat_id_t *counter_ids,
        _In_ otai_stats_mode_t mode,
        _Inout_ otai_status_t *object_statuses)
{
    MUTEX();
    SWSS_LOG_ENTER();
    REDIS_CHECK_API_INITIALIZED();

    return m_otai->bulkClearStats(switchId,
                                 object_type,
                                 object_count,
                                 object_key,
                                 number_of_counters,
                                 counter_ids,
                                 mode,
                                 object_statuses);
}

// BULK QUAD OID

otai_status_t ClientServerOtai::bulkCreate(
        _In_ otai_object_type_t object_type,
        _In_ otai_object_id_t switch_id,
        _In_ uint32_t object_count,
        _In_ const uint32_t *attr_count,
        _In_ const otai_attribute_t **attr_list,
        _In_ otai_bulk_op_error_mode_t mode,
        _Out_ otai_object_id_t *object_id,
        _Out_ otai_status_t *object_statuses)
{
    MUTEX();
    SWSS_LOG_ENTER();
    REDIS_CHECK_API_INITIALIZED();

    return m_otai->bulkCreate(
            object_type,
            switch_id,
            object_count,
            attr_count,
            attr_list,
            mode,
            object_id,
            object_statuses);
}

otai_status_t ClientServerOtai::bulkRemove(
        _In_ otai_object_type_t object_type,
        _In_ uint32_t object_count,
        _In_ const otai_object_id_t *object_id,
        _In_ otai_bulk_op_error_mode_t mode,
        _Out_ otai_status_t *object_statuses)
{
    MUTEX();
    SWSS_LOG_ENTER();
    REDIS_CHECK_API_INITIALIZED();

    return m_otai->bulkRemove(
            object_type,
            object_count,
            object_id,
            mode,
            object_statuses);
}

otai_status_t ClientServerOtai::bulkSet(
        _In_ otai_object_type_t object_type,
        _In_ uint32_t object_count,
        _In_ const otai_object_id_t *object_id,
        _In_ const otai_attribute_t *attr_list,
        _In_ otai_bulk_op_error_mode_t mode,
        _Out_ otai_status_t *object_statuses)
{
    MUTEX();
    SWSS_LOG_ENTER();
    REDIS_CHECK_API_INITIALIZED();

    return m_otai->bulkSet(
            object_type,
            object_count,
            object_id,
            attr_list,
            mode,
            object_statuses);
}

otai_status_t ClientServerOtai::bulkGet(
        _In_ otai_object_type_t object_type,
        _In_ uint32_t object_count,
        _In_ const otai_object_id_t *object_id,
        _In_ const uint32_t *attr_count,
        _Inout_ otai_attribute_t **attr_list,
        _In_ otai_bulk_op_error_mode_t mode,
        _Out_ otai_status_t *object_statuses)
{
    SWSS_LOG_ENTER();

    SWSS_LOG_ERROR("not implemented, FIXME");

    return OTAI_STATUS_NOT_IMPLEMENTED;
}

// BULK QUAD ENTRY

#define DECLARE_BULK_CREATE_ENTRY(OT,ot)                    \
otai_status_t ClientServerOtai::bulkCreate(                   \
        _In_ uint32_t object_count,                         \
        _In_ const otai_ ## ot ## _t* entries,               \
        _In_ const uint32_t *attr_count,                    \
        _In_ const otai_attribute_t **attr_list,             \
        _In_ otai_bulk_op_error_mode_t mode,                 \
        _Out_ otai_status_t *object_statuses)                \
{                                                           \
    MUTEX();                                                \
    SWSS_LOG_ENTER();                                       \
    REDIS_CHECK_API_INITIALIZED();                          \
    return m_otai->bulkCreate(                               \
            object_count,                                   \
            entries,                                        \
            attr_count,                                     \
            attr_list,                                      \
            mode,                                           \
            object_statuses);                               \
}

OTAIREDIS_DECLARE_EVERY_BULK_ENTRY(DECLARE_BULK_CREATE_ENTRY);

// BULK REMOVE

#define DECLARE_BULK_REMOVE_ENTRY(OT,ot)                    \
otai_status_t ClientServerOtai::bulkRemove(                   \
        _In_ uint32_t object_count,                         \
        _In_ const otai_ ## ot ## _t *entries,               \
        _In_ otai_bulk_op_error_mode_t mode,                 \
        _Out_ otai_status_t *object_statuses)                \
{                                                           \
    MUTEX();                                                \
    SWSS_LOG_ENTER();                                       \
    REDIS_CHECK_API_INITIALIZED();                          \
    return m_otai->bulkRemove(                               \
            object_count,                                   \
            entries,                                        \
            mode,                                           \
            object_statuses);                               \
}

OTAIREDIS_DECLARE_EVERY_BULK_ENTRY(DECLARE_BULK_REMOVE_ENTRY);

// BULK SET

#define DECLARE_BULK_SET_ENTRY(OT,ot)                       \
otai_status_t ClientServerOtai::bulkSet(                      \
        _In_ uint32_t object_count,                         \
        _In_ const otai_ ## ot ## _t *entries,               \
        _In_ const otai_attribute_t *attr_list,              \
        _In_ otai_bulk_op_error_mode_t mode,                 \
        _Out_ otai_status_t *object_statuses)                \
{                                                           \
    MUTEX();                                                \
    SWSS_LOG_ENTER();                                       \
    REDIS_CHECK_API_INITIALIZED();                          \
    return m_otai->bulkSet(                                  \
            object_count,                                   \
            entries,                                        \
            attr_list,                                      \
            mode,                                           \
            object_statuses);                               \
}

OTAIREDIS_DECLARE_EVERY_BULK_ENTRY(DECLARE_BULK_SET_ENTRY);

// BULK GET

#define DECLARE_BULK_GET_ENTRY(OT,ot)                       \
otai_status_t ClientServerOtai::bulkGet(                      \
        _In_ uint32_t object_count,                         \
        _In_ const otai_ ## ot ## _t *ot,                    \
        _In_ const uint32_t *attr_count,                    \
        _Inout_ otai_attribute_t **attr_list,                \
        _In_ otai_bulk_op_error_mode_t mode,                 \
        _Out_ otai_status_t *object_statuses)                \
{                                                           \
    MUTEX();                                                \
    SWSS_LOG_ENTER();                                       \
    REDIS_CHECK_API_INITIALIZED();                          \
    SWSS_LOG_ERROR("FIXME not implemented");                \
    return OTAI_STATUS_NOT_IMPLEMENTED;                      \
}

OTAIREDIS_DECLARE_EVERY_BULK_ENTRY(DECLARE_BULK_GET_ENTRY);

// NON QUAD API

otai_status_t ClientServerOtai::flushFdbEntries(
        _In_ otai_object_id_t switch_id,
        _In_ uint32_t attr_count,
        _In_ const otai_attribute_t *attr_list)
{
    MUTEX();
    SWSS_LOG_ENTER();
    REDIS_CHECK_API_INITIALIZED();

    return m_otai->flushFdbEntries(switch_id, attr_count, attr_list);
}

// OTAI API

otai_status_t ClientServerOtai::objectTypeGetAvailability(
        _In_ otai_object_id_t switchId,
        _In_ otai_object_type_t objectType,
        _In_ uint32_t attrCount,
        _In_ const otai_attribute_t *attrList,
        _Out_ uint64_t *count)
{
    MUTEX();
    SWSS_LOG_ENTER();
    REDIS_CHECK_API_INITIALIZED();

    return m_otai->objectTypeGetAvailability(
            switchId,
            objectType,
            attrCount,
            attrList,
            count);
}


otai_status_t ClientServerOtai::queryAttributeEnumValuesCapability(
        _In_ otai_object_id_t switch_id,
        _In_ otai_object_type_t object_type,
        _In_ otai_attr_id_t attr_id,
        _Inout_ otai_s32_list_t *enum_values_capability)
{
    MUTEX();
    SWSS_LOG_ENTER();
    REDIS_CHECK_API_INITIALIZED();

    return m_otai->queryAttributeEnumValuesCapability(
            switch_id,
            object_type,
            attr_id,
            enum_values_capability);
}

otai_object_type_t ClientServerOtai::objectTypeQuery(
        _In_ otai_object_id_t objectId)
{
    SWSS_LOG_ENTER();

    if (!m_apiInitialized)
    {
        SWSS_LOG_ERROR("%s: OTAI API not initialized", __PRETTY_FUNCTION__);

        return OTAI_OBJECT_TYPE_NULL;
    }

    return m_otai->objectTypeQuery(objectId);
}

otai_object_id_t ClientServerOtai::switchIdQuery(
        _In_ otai_object_id_t objectId)
{
    SWSS_LOG_ENTER();

    if (!m_apiInitialized)
    {
        SWSS_LOG_ERROR("%s: OTAI API not initialized", __PRETTY_FUNCTION__);

        return OTAI_NULL_OBJECT_ID;
    }

    return m_otai->switchIdQuery(objectId);
}

otai_status_t ClientServerOtai::logSet(
        _In_ otai_api_t api,
        _In_ otai_log_level_t log_level)
{
    MUTEX();
    SWSS_LOG_ENTER();
    REDIS_CHECK_API_INITIALIZED();

    return m_otai->logSet(api, log_level);
}

