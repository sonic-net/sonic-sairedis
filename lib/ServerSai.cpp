#include "ServerSai.h"
#include "SaiInternal.h"
#include "Sai.h"
#include "ServerConfig.h"
#include "sairediscommon.h"

#include "meta/sai_serialize.h"
#include "meta/SaiAttributeList.h"
#include "meta/ZeroMQSelectableChannel.h"

#include "swss/logger.h"
#include "swss/select.h"
#include "swss/tokenize.h"

#include <iterator>
#include <algorithm>

using namespace sairedis;
using namespace saimeta;
using namespace std::placeholders;

#define REDIS_CHECK_API_INITIALIZED()                                       \
    if (!m_apiInitialized) {                                                \
        SWSS_LOG_ERROR("%s: api not initialized", __PRETTY_FUNCTION__);     \
        return SAI_STATUS_FAILURE; }

ServerSai::ServerSai()
{
    SWSS_LOG_ENTER();

    m_apiInitialized = false;

    m_runServerThread = false;
}

ServerSai::~ServerSai()
{
    SWSS_LOG_ENTER();

    if (m_apiInitialized)
    {
        apiUninitialize();
    }
}

// INITIALIZE UNINITIALIZE

sai_status_t ServerSai::apiInitialize(
        _In_ uint64_t flags,
        _In_ const sai_service_method_table_t *service_method_table)
{
    MUTEX();
    SWSS_LOG_ENTER();

    if (m_apiInitialized)
    {
        SWSS_LOG_ERROR("%s: api already initialized", __PRETTY_FUNCTION__);

        return SAI_STATUS_FAILURE;
    }

    if (flags != 0)
    {
        SWSS_LOG_ERROR("invalid flags passed to SAI API initialize");

        return SAI_STATUS_INVALID_PARAMETER;
    }

    if ((service_method_table == NULL) ||
            (service_method_table->profile_get_next_value == NULL) ||
            (service_method_table->profile_get_value == NULL))
    {
        SWSS_LOG_ERROR("invalid service_method_table handle passed to SAI API initialize");

        return SAI_STATUS_INVALID_PARAMETER;
    }

    memcpy(&m_service_method_table, service_method_table, sizeof(m_service_method_table));

    m_sai = std::make_shared<Sai>(); // actual SAI to talk to syncd

    auto status = m_sai->apiInitialize(flags, service_method_table);

    SWSS_LOG_NOTICE("init client/server sai: %s", sai_serialize_status(status).c_str());

    if (status == SAI_STATUS_SUCCESS)
    {
        auto serverConfig = service_method_table->profile_get_value(0, SAI_REDIS_KEY_SERVER_CONFIG);

        auto cc = ServerConfig::loadFromFile(serverConfig);

        m_selectableChannel = std::make_shared<ZeroMQSelectableChannel>(cc->m_zmqEndpoint);

        SWSS_LOG_NOTICE("starting server thread");

        m_runServerThread = true;

        m_serverThread = std::make_shared<std::thread>(&ServerSai::serverThreadFunction, this);

        m_apiInitialized = true;
    }

    return status;
}

sai_status_t ServerSai::apiUninitialize(void)
{
    SWSS_LOG_ENTER();
    REDIS_CHECK_API_INITIALIZED();

    SWSS_LOG_NOTICE("begin");

    m_apiInitialized = false;

    if (m_serverThread)
    {
        SWSS_LOG_NOTICE("end server thread begin");

        m_runServerThread = false;

        m_serverThreadThreadShouldEndEvent.notify();

        m_serverThread->join();

        SWSS_LOG_NOTICE("end server thread end");
    }

    m_sai = nullptr;

    SWSS_LOG_NOTICE("end");

    return SAI_STATUS_SUCCESS;
}

// QUAD OID

sai_status_t ServerSai::create(
        _In_ sai_object_type_t objectType,
        _Out_ sai_object_id_t* objectId,
        _In_ sai_object_id_t switchId,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    MUTEX();
    SWSS_LOG_ENTER();
    REDIS_CHECK_API_INITIALIZED();

    return m_sai->create(
            objectType,
            objectId,
            switchId,
            attr_count,
            attr_list);
}

sai_status_t ServerSai::remove(
        _In_ sai_object_type_t objectType,
        _In_ sai_object_id_t objectId)
{
    MUTEX();
    SWSS_LOG_ENTER();
    REDIS_CHECK_API_INITIALIZED();

    return m_sai->remove(objectType, objectId);
}

sai_status_t ServerSai::set(
        _In_ sai_object_type_t objectType,
        _In_ sai_object_id_t objectId,
        _In_ const sai_attribute_t *attr)
{
    MUTEX();
    SWSS_LOG_ENTER();
    REDIS_CHECK_API_INITIALIZED();

    return m_sai->set(objectType, objectId, attr);
}

sai_status_t ServerSai::get(
        _In_ sai_object_type_t objectType,
        _In_ sai_object_id_t objectId,
        _In_ uint32_t attr_count,
        _Inout_ sai_attribute_t *attr_list)
{
    MUTEX();
    SWSS_LOG_ENTER();
    REDIS_CHECK_API_INITIALIZED();

    return m_sai->get(
            objectType,
            objectId,
            attr_count,
            attr_list);
}

// QUAD ENTRY

#define DECLARE_CREATE_ENTRY(OT,ot)                                 \
sai_status_t ServerSai::create(                                     \
        _In_ const sai_ ## ot ## _t* entry,                         \
        _In_ uint32_t attr_count,                                   \
        _In_ const sai_attribute_t *attr_list)                      \
{                                                                   \
    MUTEX();                                                        \
    SWSS_LOG_ENTER();                                               \
    REDIS_CHECK_API_INITIALIZED();                                  \
    return m_sai->create(entry, attr_count, attr_list);             \
}

SAIREDIS_DECLARE_EVERY_ENTRY(DECLARE_CREATE_ENTRY);

#define DECLARE_REMOVE_ENTRY(OT,ot)                         \
sai_status_t ServerSai::remove(                             \
        _In_ const sai_ ## ot ## _t* entry)                 \
{                                                           \
    MUTEX();                                                \
    SWSS_LOG_ENTER();                                       \
    REDIS_CHECK_API_INITIALIZED();                          \
    return m_sai->remove(entry);                            \
}

SAIREDIS_DECLARE_EVERY_ENTRY(DECLARE_REMOVE_ENTRY);

#define DECLARE_SET_ENTRY(OT,ot)                            \
sai_status_t ServerSai::set(                                \
        _In_ const sai_ ## ot ## _t* entry,                 \
        _In_ const sai_attribute_t *attr)                   \
{                                                           \
    MUTEX();                                                \
    SWSS_LOG_ENTER();                                       \
    REDIS_CHECK_API_INITIALIZED();                          \
    return m_sai->set(entry, attr);                         \
}

SAIREDIS_DECLARE_EVERY_ENTRY(DECLARE_SET_ENTRY);

#define DECLARE_GET_ENTRY(OT,ot)                                \
sai_status_t ServerSai::get(                                    \
        _In_ const sai_ ## ot ## _t* entry,                     \
        _In_ uint32_t attr_count,                               \
        _Inout_ sai_attribute_t *attr_list)                     \
{                                                               \
    MUTEX();                                                    \
    SWSS_LOG_ENTER();                                           \
    REDIS_CHECK_API_INITIALIZED();                              \
    return m_sai->get(entry, attr_count, attr_list);            \
}

SAIREDIS_DECLARE_EVERY_ENTRY(DECLARE_GET_ENTRY);

// STATS

sai_status_t ServerSai::getStats(
        _In_ sai_object_type_t object_type,
        _In_ sai_object_id_t object_id,
        _In_ uint32_t number_of_counters,
        _In_ const sai_stat_id_t *counter_ids,
        _Out_ uint64_t *counters)
{
    MUTEX();
    SWSS_LOG_ENTER();
    REDIS_CHECK_API_INITIALIZED();

    return m_sai->getStats(
            object_type,
            object_id,
            number_of_counters,
            counter_ids,
            counters);
}

sai_status_t ServerSai::queryStatsCapability(
        _In_ sai_object_id_t switchId,
        _In_ sai_object_type_t objectType,
        _Inout_ sai_stat_capability_list_t *stats_capability)
{
    MUTEX();
    SWSS_LOG_ENTER();
    REDIS_CHECK_API_INITIALIZED();

    return m_sai->queryStatsCapability(
            switchId,
            objectType,
            stats_capability);
}

sai_status_t ServerSai::queryStatsStCapability(
    _In_ sai_object_id_t switchId,
    _In_ sai_object_type_t objectType,
    _Inout_ sai_stat_st_capability_list_t *stats_capability)
{
    MUTEX();
    SWSS_LOG_ENTER();
    REDIS_CHECK_API_INITIALIZED();

    return m_sai->queryStatsStCapability(
        switchId,
        objectType,
        stats_capability);
}

sai_status_t ServerSai::getStatsExt(
        _In_ sai_object_type_t object_type,
        _In_ sai_object_id_t object_id,
        _In_ uint32_t number_of_counters,
        _In_ const sai_stat_id_t *counter_ids,
        _In_ sai_stats_mode_t mode,
        _Out_ uint64_t *counters)
{
    MUTEX();
    SWSS_LOG_ENTER();
    REDIS_CHECK_API_INITIALIZED();

    return m_sai->getStatsExt(
            object_type,
            object_id,
            number_of_counters,
            counter_ids,
            mode,
            counters);
}

sai_status_t ServerSai::clearStats(
        _In_ sai_object_type_t object_type,
        _In_ sai_object_id_t object_id,
        _In_ uint32_t number_of_counters,
        _In_ const sai_stat_id_t *counter_ids)
{
    MUTEX();
    SWSS_LOG_ENTER();
    REDIS_CHECK_API_INITIALIZED();

    return m_sai->clearStats(
            object_type,
            object_id,
            number_of_counters,
            counter_ids);
}

sai_status_t ServerSai::bulkGetStats(
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
    MUTEX();
    SWSS_LOG_ENTER();
    REDIS_CHECK_API_INITIALIZED();

    return m_sai->bulkGetStats(switchId,
                               object_type,
                               object_count,
                               object_key,
                               number_of_counters,
                               counter_ids,
                               mode,
                               object_statuses,
                               counters);
}

sai_status_t ServerSai::bulkClearStats(
        _In_ sai_object_id_t switchId,
        _In_ sai_object_type_t object_type,
        _In_ uint32_t object_count,
        _In_ const sai_object_key_t *object_key,
        _In_ uint32_t number_of_counters,
        _In_ const sai_stat_id_t *counter_ids,
        _In_ sai_stats_mode_t mode,
        _Inout_ sai_status_t *object_statuses)
{
    MUTEX();
    SWSS_LOG_ENTER();
    REDIS_CHECK_API_INITIALIZED();

    return m_sai->bulkClearStats(switchId,
                                 object_type,
                                 object_count,
                                 object_key,
                                 number_of_counters,
                                 counter_ids,
                                 mode,
                                 object_statuses);
}

// BULK QUAD OID

sai_status_t ServerSai::bulkCreate(
        _In_ sai_object_type_t object_type,
        _In_ sai_object_id_t switch_id,
        _In_ uint32_t object_count,
        _In_ const uint32_t *attr_count,
        _In_ const sai_attribute_t **attr_list,
        _In_ sai_bulk_op_error_mode_t mode,
        _Out_ sai_object_id_t *object_id,
        _Out_ sai_status_t *object_statuses)
{
    MUTEX();
    SWSS_LOG_ENTER();
    REDIS_CHECK_API_INITIALIZED();

    return m_sai->bulkCreate(
            object_type,
            switch_id,
            object_count,
            attr_count,
            attr_list,
            mode,
            object_id,
            object_statuses);
}

sai_status_t ServerSai::bulkRemove(
        _In_ sai_object_type_t object_type,
        _In_ uint32_t object_count,
        _In_ const sai_object_id_t *object_id,
        _In_ sai_bulk_op_error_mode_t mode,
        _Out_ sai_status_t *object_statuses)
{
    MUTEX();
    SWSS_LOG_ENTER();
    REDIS_CHECK_API_INITIALIZED();

    return m_sai->bulkRemove(
            object_type,
            object_count,
            object_id,
            mode,
            object_statuses);
}

sai_status_t ServerSai::bulkSet(
        _In_ sai_object_type_t object_type,
        _In_ uint32_t object_count,
        _In_ const sai_object_id_t *object_id,
        _In_ const sai_attribute_t *attr_list,
        _In_ sai_bulk_op_error_mode_t mode,
        _Out_ sai_status_t *object_statuses)
{
    MUTEX();
    SWSS_LOG_ENTER();
    REDIS_CHECK_API_INITIALIZED();

    return m_sai->bulkSet(
            object_type,
            object_count,
            object_id,
            attr_list,
            mode,
            object_statuses);
}

sai_status_t ServerSai::bulkGet(
        _In_ sai_object_type_t object_type,
        _In_ uint32_t object_count,
        _In_ const sai_object_id_t *object_id,
        _In_ const uint32_t *attr_count,
        _Inout_ sai_attribute_t **attr_list,
        _In_ sai_bulk_op_error_mode_t mode,
        _Out_ sai_status_t *object_statuses)
{
    MUTEX();
    SWSS_LOG_ENTER();
    REDIS_CHECK_API_INITIALIZED();

    return m_sai->bulkGet(
            object_type,
            object_count,
            object_id,
            attr_count,
            attr_list,
            mode,
            object_statuses);
}

// BULK QUAD ENTRY

#define DECLARE_BULK_CREATE_ENTRY(OT,ot)                    \
sai_status_t ServerSai::bulkCreate(                         \
        _In_ uint32_t object_count,                         \
        _In_ const sai_ ## ot ## _t* entries,               \
        _In_ const uint32_t *attr_count,                    \
        _In_ const sai_attribute_t **attr_list,             \
        _In_ sai_bulk_op_error_mode_t mode,                 \
        _Out_ sai_status_t *object_statuses)                \
{                                                           \
    MUTEX();                                                \
    SWSS_LOG_ENTER();                                       \
    REDIS_CHECK_API_INITIALIZED();                          \
    return m_sai->bulkCreate(                               \
            object_count,                                   \
            entries,                                        \
            attr_count,                                     \
            attr_list,                                      \
            mode,                                           \
            object_statuses);                               \
}

SAIREDIS_DECLARE_EVERY_BULK_ENTRY(DECLARE_BULK_CREATE_ENTRY);

// BULK REMOVE

#define DECLARE_BULK_REMOVE_ENTRY(OT,ot)                    \
sai_status_t ServerSai::bulkRemove(                         \
        _In_ uint32_t object_count,                         \
        _In_ const sai_ ## ot ## _t *entries,               \
        _In_ sai_bulk_op_error_mode_t mode,                 \
        _Out_ sai_status_t *object_statuses)                \
{                                                           \
    MUTEX();                                                \
    SWSS_LOG_ENTER();                                       \
    REDIS_CHECK_API_INITIALIZED();                          \
    return m_sai->bulkRemove(                               \
            object_count,                                   \
            entries,                                        \
            mode,                                           \
            object_statuses);                               \
}

SAIREDIS_DECLARE_EVERY_BULK_ENTRY(DECLARE_BULK_REMOVE_ENTRY);

// BULK SET

#define DECLARE_BULK_SET_ENTRY(OT,ot)                       \
sai_status_t ServerSai::bulkSet(                            \
        _In_ uint32_t object_count,                         \
        _In_ const sai_ ## ot ## _t *entries,               \
        _In_ const sai_attribute_t *attr_list,              \
        _In_ sai_bulk_op_error_mode_t mode,                 \
        _Out_ sai_status_t *object_statuses)                \
{                                                           \
    MUTEX();                                                \
    SWSS_LOG_ENTER();                                       \
    REDIS_CHECK_API_INITIALIZED();                          \
    return m_sai->bulkSet(                                  \
            object_count,                                   \
            entries,                                        \
            attr_list,                                      \
            mode,                                           \
            object_statuses);                               \
}

SAIREDIS_DECLARE_EVERY_BULK_ENTRY(DECLARE_BULK_SET_ENTRY);

// BULK GET

#define DECLARE_BULK_GET_ENTRY(OT,ot)                       \
sai_status_t ServerSai::bulkGet(                            \
        _In_ uint32_t object_count,                         \
        _In_ const sai_ ## ot ## _t *ot,                    \
        _In_ const uint32_t *attr_count,                    \
        _Inout_ sai_attribute_t **attr_list,                \
        _In_ sai_bulk_op_error_mode_t mode,                 \
        _Out_ sai_status_t *object_statuses)                \
{                                                           \
    MUTEX();                                                \
    SWSS_LOG_ENTER();                                       \
    REDIS_CHECK_API_INITIALIZED();                          \
    SWSS_LOG_ERROR("FIXME not implemented");                \
    return SAI_STATUS_NOT_IMPLEMENTED;                      \
}

SAIREDIS_DECLARE_EVERY_BULK_ENTRY(DECLARE_BULK_GET_ENTRY);

// NON QUAD API

sai_status_t ServerSai::flushFdbEntries(
        _In_ sai_object_id_t switch_id,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    MUTEX();
    SWSS_LOG_ENTER();
    REDIS_CHECK_API_INITIALIZED();

    return m_sai->flushFdbEntries(switch_id, attr_count, attr_list);
}

// SAI API

sai_status_t ServerSai::objectTypeGetAvailability(
        _In_ sai_object_id_t switchId,
        _In_ sai_object_type_t objectType,
        _In_ uint32_t attrCount,
        _In_ const sai_attribute_t *attrList,
        _Out_ uint64_t *count)
{
    MUTEX();
    SWSS_LOG_ENTER();
    REDIS_CHECK_API_INITIALIZED();

    return m_sai->objectTypeGetAvailability(
            switchId,
            objectType,
            attrCount,
            attrList,
            count);
}

sai_status_t ServerSai::queryAttributeCapability(
        _In_ sai_object_id_t switch_id,
        _In_ sai_object_type_t object_type,
        _In_ sai_attr_id_t attr_id,
        _Out_ sai_attr_capability_t *capability)
{
    MUTEX();
    SWSS_LOG_ENTER();
    REDIS_CHECK_API_INITIALIZED();

    return m_sai->queryAttributeCapability(
            switch_id,
            object_type,
            attr_id,
            capability);
}

sai_status_t ServerSai::queryAttributeEnumValuesCapability(
        _In_ sai_object_id_t switch_id,
        _In_ sai_object_type_t object_type,
        _In_ sai_attr_id_t attr_id,
        _Inout_ sai_s32_list_t *enum_values_capability)
{
    MUTEX();
    SWSS_LOG_ENTER();
    REDIS_CHECK_API_INITIALIZED();

    return m_sai->queryAttributeEnumValuesCapability(
            switch_id,
            object_type,
            attr_id,
            enum_values_capability);
}

sai_object_type_t ServerSai::objectTypeQuery(
        _In_ sai_object_id_t objectId)
{
    SWSS_LOG_ENTER();

    if (!m_apiInitialized)
    {
        SWSS_LOG_ERROR("%s: SAI API not initialized", __PRETTY_FUNCTION__);

        return SAI_OBJECT_TYPE_NULL;
    }

    return m_sai->objectTypeQuery(objectId);
}

sai_object_id_t ServerSai::switchIdQuery(
        _In_ sai_object_id_t objectId)
{
    SWSS_LOG_ENTER();

    if (!m_apiInitialized)
    {
        SWSS_LOG_ERROR("%s: SAI API not initialized", __PRETTY_FUNCTION__);

        return SAI_NULL_OBJECT_ID;
    }

    return m_sai->switchIdQuery(objectId);
}

sai_status_t ServerSai::logSet(
        _In_ sai_api_t api,
        _In_ sai_log_level_t log_level)
{
    MUTEX();
    SWSS_LOG_ENTER();
    REDIS_CHECK_API_INITIALIZED();

    return m_sai->logSet(api, log_level);
}

sai_status_t ServerSai::queryApiVersion(
        _Out_ sai_api_version_t *version)
{
    MUTEX();
    SWSS_LOG_ENTER();
    REDIS_CHECK_API_INITIALIZED();

    return m_sai->queryApiVersion(version);
}

void ServerSai::serverThreadFunction()
{
    SWSS_LOG_ENTER();

    SWSS_LOG_NOTICE("begin");

    swss::Select s;

    s.addSelectable(m_selectableChannel.get());
    s.addSelectable(&m_serverThreadThreadShouldEndEvent);

    while (m_runServerThread)
    {
        swss::Selectable *sel;

        int result = s.select(&sel);

        if (sel == &m_serverThreadThreadShouldEndEvent)
        {
            // user requested end server thread
            break;
        }

        if (result == swss::Select::OBJECT)
        {
            processEvent(*m_selectableChannel.get());
        }
        else
        {
            SWSS_LOG_ERROR("select failed: %s", swss::Select::resultToString(result).c_str());
        }
    }

    SWSS_LOG_NOTICE("end");
}

void ServerSai::processEvent(
        _In_ SelectableChannel& consumer)
{
    MUTEX();
    SWSS_LOG_ENTER();

    if (!m_apiInitialized)
    {
        SWSS_LOG_ERROR("%s: SAI API not initialized", __PRETTY_FUNCTION__);

        return;
    }

    do
    {
        swss::KeyOpFieldsValuesTuple kco;

        consumer.pop(kco, false);

        processSingleEvent(kco);
    }
    while (!consumer.empty());
}

sai_status_t ServerSai::processSingleEvent(
        _In_ const swss::KeyOpFieldsValuesTuple &kco)
{
    SWSS_LOG_ENTER();

    auto& key = kfvKey(kco);
    auto& op = kfvOp(kco);

    SWSS_LOG_INFO("key: %s op: %s", key.c_str(), op.c_str());

    if (key.length() == 0)
    {
        SWSS_LOG_DEBUG("no elements in m_buffer");

        return SAI_STATUS_SUCCESS;
    }

    if (op == REDIS_ASIC_STATE_COMMAND_CREATE)
        return processQuadEvent(SAI_COMMON_API_CREATE, kco);

    if (op == REDIS_ASIC_STATE_COMMAND_REMOVE)
        return processQuadEvent(SAI_COMMON_API_REMOVE, kco);

    if (op == REDIS_ASIC_STATE_COMMAND_SET)
        return processQuadEvent(SAI_COMMON_API_SET, kco);

    if (op == REDIS_ASIC_STATE_COMMAND_GET)
        return processQuadEvent(SAI_COMMON_API_GET, kco);

    if (op == REDIS_ASIC_STATE_COMMAND_BULK_CREATE)
        return processBulkQuadEvent(SAI_COMMON_API_BULK_CREATE, kco);

    if (op == REDIS_ASIC_STATE_COMMAND_BULK_REMOVE)
        return processBulkQuadEvent(SAI_COMMON_API_BULK_REMOVE, kco);

    if (op == REDIS_ASIC_STATE_COMMAND_BULK_SET)
        return processBulkQuadEvent(SAI_COMMON_API_BULK_SET, kco);

    if (op == REDIS_ASIC_STATE_COMMAND_GET_STATS)
        return processGetStatsEvent(kco);

    if (op == REDIS_ASIC_STATE_COMMAND_CLEAR_STATS)
        return processClearStatsEvent(kco);

    if (op == REDIS_ASIC_STATE_COMMAND_FLUSH)
        return processFdbFlush(kco);

    if (op == REDIS_ASIC_STATE_COMMAND_ATTR_CAPABILITY_QUERY)
        return processAttrCapabilityQuery(kco);

    if (op == REDIS_ASIC_STATE_COMMAND_ATTR_ENUM_VALUES_CAPABILITY_QUERY)
        return processAttrEnumValuesCapabilityQuery(kco);

    if (op == REDIS_ASIC_STATE_COMMAND_OBJECT_TYPE_GET_AVAILABILITY_QUERY)
        return processObjectTypeGetAvailabilityQuery(kco);

    if (op == REDIS_ASIC_STATE_COMMAND_STATS_CAPABILITY_QUERY)
        return processStatsCapabilityQuery(kco);

    if (op == REDIS_ASIC_STATE_COMMAND_STATS_ST_CAPABILITY_QUERY)
        return processStatsStCapabilityQuery(kco);

    SWSS_LOG_THROW("event op '%s' is not implemented, FIXME", op.c_str());
}

sai_status_t ServerSai::processQuadEvent(
        _In_ sai_common_api_t api,
        _In_ const swss::KeyOpFieldsValuesTuple &kco)
{
    SWSS_LOG_ENTER();

    const std::string& key = kfvKey(kco);
    const std::string& op = kfvOp(kco);

    const std::string& strObjectId = key.substr(key.find(":") + 1);

    sai_object_meta_key_t metaKey;
    sai_deserialize_object_meta_key(key, metaKey);

    if (!sai_metadata_is_object_type_valid(metaKey.objecttype))
    {
        SWSS_LOG_THROW("invalid object type %s", key.c_str());
    }

    SWSS_LOG_NOTICE("got server request: %s : %s", key.c_str(), op.c_str());

    auto& values = kfvFieldsValues(kco);

    for (auto& v: values)
    {
        SWSS_LOG_NOTICE(" - attr: %s: %s", fvField(v).c_str(), fvValue(v).c_str());
    }

    SaiAttributeList list(metaKey.objecttype, values, false);

    sai_attribute_t *attr_list = list.get_attr_list();
    uint32_t attr_count = list.get_attr_count();

    /*
     * NOTE: Need to translate client notification pointers to sairedis
     * pointers since they reside in different memory space.
     */

    if (metaKey.objecttype == SAI_OBJECT_TYPE_SWITCH && (api == SAI_COMMON_API_CREATE || api == SAI_COMMON_API_SET))
    {
        /*
         * Since we will be operating on existing switch, it may happen that
         * client will set notification pointer, which is not actually set in
         * sairedis server.  Then we would need to take extra steps in server
         * to forward those notifications. We could also drop notification
         * request on clients side, since it would be possible only to set only
         * existing pointers.
         *
         * TODO: must be done per switch, and switch may not exists yet
         */

        // TODO m_handler->updateNotificationsPointers(attr_count, attr_list);
        SWSS_LOG_ERROR("TODO, update notification pointers, FIXME!");
    }

    auto info = sai_metadata_get_object_type_info(metaKey.objecttype);

    sai_status_t status;

    // NOTE: for create api (even switch) passed oid will be SAI_NULL_OBJECT_ID

    sai_object_id_t oid = SAI_NULL_OBJECT_ID;

    if (info->isnonobjectid)
    {
        status = processEntry(metaKey, api, attr_count, attr_list);
    }
    else
    {
        sai_deserialize_object_id(strObjectId, oid);

        // In case of create oid object, client don't know what will be created
        // object id before it receive it, so instead of that it will transfer
        // switch id in that place since switch id is needed to create other
        // object in the first place. If create method is for switch, this
        // field will be SAI_NULL_OBJECT_ID.

        sai_object_id_t switchOid = oid;

        status = processOid(metaKey.objecttype, oid, switchOid, api, attr_count, attr_list);
    }

    if (api == SAI_COMMON_API_GET)
    {
        sendGetResponse(metaKey.objecttype, strObjectId, status, attr_count, attr_list);
    }
    else if (status != SAI_STATUS_SUCCESS)
    {
        sendApiResponse(api, status, oid);

        SWSS_LOG_ERROR("api failed: %s (%s): %s",
                key.c_str(),
                op.c_str(),
                sai_serialize_status(status).c_str());

        for (const auto &v: values)
        {
            SWSS_LOG_ERROR("attr: %s: %s", fvField(v).c_str(), fvValue(v).c_str());
        }
    }
    else // non GET api, status is SUCCESS
    {
        sendApiResponse(api, status, oid);
    }

    return status;
}

void ServerSai::sendApiResponse(
        _In_ sai_common_api_t api,
        _In_ sai_status_t status,
        _In_ sai_object_id_t oid)
{
    SWSS_LOG_ENTER();

    switch (api)
    {
        case SAI_COMMON_API_CREATE:
        case SAI_COMMON_API_REMOVE:
        case SAI_COMMON_API_SET:
            break;

        default:
            SWSS_LOG_THROW("api %s not supported by this function",
                    sai_serialize_common_api(api).c_str());
    }

    std::vector<swss::FieldValueTuple> entry;

    if (api == SAI_COMMON_API_CREATE)
    {
        // in case of create api, we need to return OID value that was created
        // to the client

        entry.emplace_back("oid", sai_serialize_object_id(oid));
    }

    std::string strStatus = sai_serialize_status(status);

    SWSS_LOG_INFO("sending response for %s api with status: %s",
            sai_serialize_common_api(api).c_str(),
            strStatus.c_str());

    m_selectableChannel->set(strStatus, entry, REDIS_ASIC_STATE_COMMAND_GETRESPONSE);
}

void ServerSai::sendGetResponse(
        _In_ sai_object_type_t objectType,
        _In_ const std::string& strObjectId,
        _In_ sai_status_t status,
        _In_ uint32_t attr_count,
        _In_ sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    std::vector<swss::FieldValueTuple> entry;

    if (status == SAI_STATUS_SUCCESS)
    {
        entry = SaiAttributeList::serialize_attr_list(
                objectType,
                attr_count,
                attr_list,
                false);
    }
    else if (status == SAI_STATUS_BUFFER_OVERFLOW)
    {
        /*
         * In this case we got correct values for list, but list was too small
         * so serialize only count without list itself, sairedis will need to
         * take this into account when deserialize.
         *
         * If there was a list somewhere, count will be changed to actual value
         * different attributes can have different lists, many of them may
         * serialize only count, and will need to support that on the receiver.
         */

        entry = SaiAttributeList::serialize_attr_list(
                objectType,
                attr_count,
                attr_list,
                true);
    }
    else
    {
        /*
         * Some other error, don't send attributes at all.
         */
    }

    for (const auto &e: entry)
    {
        SWSS_LOG_DEBUG("attr: %s: %s", fvField(e).c_str(), fvValue(e).c_str());
    }

    std::string strStatus = sai_serialize_status(status);

    SWSS_LOG_INFO("sending response for GET api with status: %s", strStatus.c_str());

    /*
     * Since we have only one get at a time, we don't have to serialize object
     * type and object id, only get status is required to be returned.  Get
     * response will not put any data to table, only queue is used.
     */

    m_selectableChannel->set(strStatus, entry, REDIS_ASIC_STATE_COMMAND_GETRESPONSE);

    SWSS_LOG_INFO("response for GET api was send");
}

sai_status_t ServerSai::processEntry(
        _In_ sai_object_meta_key_t metaKey,
        _In_ sai_common_api_t api,
        _In_ uint32_t attr_count,
        _In_ sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    switch (api)
    {
        case SAI_COMMON_API_CREATE:
            return m_sai->create(metaKey, SAI_NULL_OBJECT_ID, attr_count, attr_list);

        case SAI_COMMON_API_REMOVE:
            return m_sai->remove(metaKey);

        case SAI_COMMON_API_SET:
            return m_sai->set(metaKey, attr_list);

        case SAI_COMMON_API_GET:
            return m_sai->get(metaKey, attr_count, attr_list);

        default:

            SWSS_LOG_THROW("api %s not supported", sai_serialize_common_api(api).c_str());
    }
}

sai_status_t ServerSai::processOid(
        _In_ sai_object_type_t objectType,
        _Inout_ sai_object_id_t& oid,
        _In_ sai_object_id_t switchOid,
        _In_ sai_common_api_t api,
        _In_ uint32_t attr_count,
        _In_ sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    SWSS_LOG_DEBUG("calling %s for %s",
            sai_serialize_common_api(api).c_str(),
            sai_serialize_object_type(objectType).c_str());

    auto info = sai_metadata_get_object_type_info(objectType);

    if (info->isnonobjectid)
    {
        SWSS_LOG_THROW("passing non object id %s as generic object", info->objecttypename);
    }

    switch (api)
    {
        case SAI_COMMON_API_CREATE:
            return m_sai->create(objectType, &oid, switchOid, attr_count, attr_list);

        case SAI_COMMON_API_REMOVE:
            return m_sai->remove(objectType, oid);

        case SAI_COMMON_API_SET:
            return m_sai->set(objectType, oid, attr_list);

        case SAI_COMMON_API_GET:
            return m_sai->get(objectType, oid, attr_count, attr_list);

        default:

            SWSS_LOG_THROW("common api (%s) is not implemented", sai_serialize_common_api(api).c_str());
    }
}

sai_status_t ServerSai::processBulkQuadEvent(
        _In_ sai_common_api_t api,
        _In_ const swss::KeyOpFieldsValuesTuple &kco)
{
    SWSS_LOG_ENTER();

    const std::string& key = kfvKey(kco); // objectType:count

    std::string strObjectType = key.substr(0, key.find(":"));

    sai_object_type_t objectType;
    sai_deserialize_object_type(strObjectType, objectType);

    const std::vector<swss::FieldValueTuple> &values = kfvFieldsValues(kco);

    std::vector<std::vector<swss::FieldValueTuple>> strAttributes;

    // field = objectId
    // value = attrid=attrvalue|...

    std::vector<std::string> objectIds;

    std::vector<std::shared_ptr<SaiAttributeList>> attributes;

    for (const auto &fvt: values)
    {
        std::string strObjectId = fvField(fvt);
        std::string joined = fvValue(fvt);

        // decode values

        auto v = swss::tokenize(joined, '|');

        objectIds.push_back(strObjectId);

        std::vector<swss::FieldValueTuple> entries; // attributes per object id

        for (size_t i = 0; i < v.size(); ++i)
        {
            const std::string item = v.at(i);

            auto start = item.find_first_of("=");

            auto field = item.substr(0, start);
            auto value = item.substr(start + 1);

            entries.emplace_back(field, value);
        }

        strAttributes.push_back(entries);

        // since now we converted this to proper list, we can extract attributes

        auto list = std::make_shared<SaiAttributeList>(objectType, entries, false);

        attributes.push_back(list);
    }

    SWSS_LOG_INFO("bulk %s executing with %zu items",
            strObjectType.c_str(),
            objectIds.size());

    auto info = sai_metadata_get_object_type_info(objectType);

    if (info->isobjectid)
    {
        return processBulkOid(objectType, objectIds, api, attributes, strAttributes);
    }
    else
    {
        return processBulkEntry(objectType, objectIds, api, attributes, strAttributes);
    }
}

sai_status_t ServerSai::processBulkOid(
        _In_ sai_object_type_t objectType,
        _In_ const std::vector<std::string>& strObjectIds,
        _In_ sai_common_api_t api,
        _In_ const std::vector<std::shared_ptr<SaiAttributeList>>& attributes,
        _In_ const std::vector<std::vector<swss::FieldValueTuple>>& strAttributes)
{
    SWSS_LOG_ENTER();

    auto info = sai_metadata_get_object_type_info(objectType);

    if (info->isnonobjectid)
    {
        SWSS_LOG_THROW("passing non object id to bulk oid object operation");
    }

    std::vector<sai_status_t> statuses(strObjectIds.size(), SAI_STATUS_FAILURE);

    sai_status_t status = SAI_STATUS_FAILURE;

    sai_bulk_op_error_mode_t mode = SAI_BULK_OP_ERROR_MODE_IGNORE_ERROR;

    std::vector<sai_object_id_t> objectIds(strObjectIds.size());

    for (size_t idx = 0; idx < objectIds.size(); idx++)
    {
        sai_deserialize_object_id(strObjectIds[idx], objectIds[idx]);
    }

    size_t object_count = objectIds.size();

    switch (api)
    {
        case SAI_COMMON_API_BULK_CREATE:

            {
                std::vector<uint32_t> attr_counts(object_count);
                std::vector<const sai_attribute_t*> attr_lists(object_count);

                for (size_t idx = 0; idx < object_count; idx++)
                {
                    attr_counts[idx] = attributes[idx]->get_attr_count();
                    attr_lists[idx] = attributes[idx]->get_attr_list();
                }

                // in case of client/server, all passed oids are switch OID since
                // client don't know what oid will be assigned to created object
                sai_object_id_t switchId = objectIds.at(0);

                status = m_sai->bulkCreate(
                        objectType,
                        switchId,
                        (uint32_t)object_count,
                        attr_counts.data(),
                        attr_lists.data(),
                        mode,
                        objectIds.data(),
                        statuses.data());
            }

            break;

        case SAI_COMMON_API_BULK_REMOVE:

            status = m_sai->bulkRemove(
                    objectType,
                    (uint32_t)objectIds.size(),
                    objectIds.data(),
                    mode,
                    statuses.data());
            break;

        default:
            status = SAI_STATUS_NOT_SUPPORTED;
            SWSS_LOG_ERROR("api %s is not supported in bulk mode", sai_serialize_common_api(api).c_str());
            break;
    }

    sendBulkApiResponse(api, status, (uint32_t)objectIds.size(), objectIds.data(), statuses.data());

    return status;
}

sai_status_t ServerSai::processBulkEntry(
        _In_ sai_object_type_t objectType,
        _In_ const std::vector<std::string>& objectIds,
        _In_ sai_common_api_t api,
        _In_ const std::vector<std::shared_ptr<SaiAttributeList>>& attributes,
        _In_ const std::vector<std::vector<swss::FieldValueTuple>>& strAttributes)
{
    SWSS_LOG_ENTER();

    auto info = sai_metadata_get_object_type_info(objectType);

    if (info->isobjectid)
    {
        SWSS_LOG_THROW("passing oid object to bulk non object id operation");
    }

    std::vector<sai_status_t> statuses(objectIds.size());

    sai_status_t status = SAI_STATUS_SUCCESS;

    switch (api)
    {
        case SAI_COMMON_API_BULK_CREATE:
            status = processBulkCreateEntry(objectType, objectIds, attributes, statuses);
            break;

        case SAI_COMMON_API_BULK_REMOVE:
            status = processBulkRemoveEntry(objectType, objectIds, statuses);
            break;

        case SAI_COMMON_API_BULK_SET:
            status = processBulkSetEntry(objectType, objectIds, attributes, statuses);
            break;

        default:
            SWSS_LOG_ERROR("api %s is not supported in bulk", sai_serialize_common_api(api).c_str());
            status = SAI_STATUS_NOT_SUPPORTED;
    }

    std::vector<sai_object_id_t> oids(objectIds.size());

    sendBulkApiResponse(api, status, (uint32_t)objectIds.size(), oids.data(), statuses.data());

    return status;
}

sai_status_t ServerSai::processBulkCreateEntry(
        _In_ sai_object_type_t objectType,
        _In_ const std::vector<std::string>& objectIds,
        _In_ const std::vector<std::shared_ptr<SaiAttributeList>>& attributes,
        _Out_ std::vector<sai_status_t>& statuses)
{
    SWSS_LOG_ENTER();

    sai_status_t status = SAI_STATUS_SUCCESS;

    uint32_t object_count = (uint32_t) objectIds.size();

    sai_bulk_op_error_mode_t mode = SAI_BULK_OP_ERROR_MODE_IGNORE_ERROR;

    std::vector<uint32_t> attr_counts(object_count);
    std::vector<const sai_attribute_t*> attr_lists(object_count);

    for (uint32_t idx = 0; idx < object_count; idx++)
    {
        attr_counts[idx] = attributes[idx]->get_attr_count();
        attr_lists[idx] = attributes[idx]->get_attr_list();
    }

    switch ((int)objectType)
    {
        case SAI_OBJECT_TYPE_ROUTE_ENTRY:
        {
            std::vector<sai_route_entry_t> entries(object_count);
            for (uint32_t it = 0; it < object_count; it++)
            {
                sai_deserialize_route_entry(objectIds[it], entries[it]);
            }

            status = m_sai->bulkCreate(
                    object_count,
                    entries.data(),
                    attr_counts.data(),
                    attr_lists.data(),
                    mode,
                    statuses.data());

        }
        break;

        case SAI_OBJECT_TYPE_FDB_ENTRY:
        {
            std::vector<sai_fdb_entry_t> entries(object_count);
            for (uint32_t it = 0; it < object_count; it++)
            {
                sai_deserialize_fdb_entry(objectIds[it], entries[it]);
            }

            status = m_sai->bulkCreate(
                    object_count,
                    entries.data(),
                    attr_counts.data(),
                    attr_lists.data(),
                    mode,
                    statuses.data());

        }
        break;

        case SAI_OBJECT_TYPE_NAT_ENTRY:
        {
            std::vector<sai_nat_entry_t> entries(object_count);
            for (uint32_t it = 0; it < object_count; it++)
            {
                sai_deserialize_nat_entry(objectIds[it], entries[it]);
            }

            status = m_sai->bulkCreate(
                    object_count,
                    entries.data(),
                    attr_counts.data(),
                    attr_lists.data(),
                    mode,
                    statuses.data());

        }
        break;

        case SAI_OBJECT_TYPE_INSEG_ENTRY:
        {
            std::vector<sai_inseg_entry_t> entries(object_count);
            for (uint32_t it = 0; it < object_count; it++)
            {
                sai_deserialize_inseg_entry(objectIds[it], entries[it]);
            }

            status = m_sai->bulkCreate(
                    object_count,
                    entries.data(),
                    attr_counts.data(),
                    attr_lists.data(),
                    mode,
                    statuses.data());

        }
        break;

        case SAI_OBJECT_TYPE_MY_SID_ENTRY:
        {
            std::vector<sai_my_sid_entry_t> entries(object_count);

            for (uint32_t it = 0; it < object_count; it++)
            {
                sai_deserialize_my_sid_entry(objectIds[it], entries[it]);
            }

            status = m_sai->bulkCreate(
                    object_count,
                    entries.data(),
                    attr_counts.data(),
                    attr_lists.data(),
                    mode,
                    statuses.data());
        }
        break;

        case SAI_OBJECT_TYPE_DIRECTION_LOOKUP_ENTRY:
        {
            std::vector<sai_direction_lookup_entry_t> entries(object_count);

            for (uint32_t it = 0; it < object_count; it++)
            {
                sai_deserialize_direction_lookup_entry(objectIds[it], entries[it]);
            }

            status = m_sai->bulkCreate(
                    object_count,
                    entries.data(),
                    attr_counts.data(),
                    attr_lists.data(),
                    mode,
                    statuses.data());
        }
        break;

        case SAI_OBJECT_TYPE_ENI_ETHER_ADDRESS_MAP_ENTRY:
        {
            std::vector<sai_eni_ether_address_map_entry_t> entries(object_count);

            for (uint32_t it = 0; it < object_count; it++)
            {
                sai_deserialize_eni_ether_address_map_entry(objectIds[it], entries[it]);
            }

            status = m_sai->bulkCreate(
                    object_count,
                    entries.data(),
                    attr_counts.data(),
                    attr_lists.data(),
                    mode,
                    statuses.data());
        }
        break;

        case SAI_OBJECT_TYPE_VIP_ENTRY:
        {
            std::vector<sai_vip_entry_t> entries(object_count);

            for (uint32_t it = 0; it < object_count; it++)
            {
                sai_deserialize_vip_entry(objectIds[it], entries[it]);
            }

            status = m_sai->bulkCreate(
                    object_count,
                    entries.data(),
                    attr_counts.data(),
                    attr_lists.data(),
                    mode,
                    statuses.data());
        }
        break;

        case SAI_OBJECT_TYPE_INBOUND_ROUTING_ENTRY:
        {
            std::vector<sai_inbound_routing_entry_t> entries(object_count);

            for (uint32_t it = 0; it < object_count; it++)
            {
                sai_deserialize_inbound_routing_entry(objectIds[it], entries[it]);
            }

            status = m_sai->bulkCreate(
                    object_count,
                    entries.data(),
                    attr_counts.data(),
                    attr_lists.data(),
                    mode,
                    statuses.data());
        }
        break;

        case SAI_OBJECT_TYPE_PA_VALIDATION_ENTRY:
        {
            std::vector<sai_pa_validation_entry_t> entries(object_count);

            for (uint32_t it = 0; it < object_count; it++)
            {
                sai_deserialize_pa_validation_entry(objectIds[it], entries[it]);
            }

            status = m_sai->bulkCreate(
                    object_count,
                    entries.data(),
                    attr_counts.data(),
                    attr_lists.data(),
                    mode,
                    statuses.data());
        }
        break;

        case SAI_OBJECT_TYPE_OUTBOUND_ROUTING_ENTRY:
        {
            std::vector<sai_outbound_routing_entry_t> entries(object_count);

            for (uint32_t it = 0; it < object_count; it++)
            {
                sai_deserialize_outbound_routing_entry(objectIds[it], entries[it]);
            }

            status = m_sai->bulkCreate(
                    object_count,
                    entries.data(),
                    attr_counts.data(),
                    attr_lists.data(),
                    mode,
                    statuses.data());
        }
        break;

        case SAI_OBJECT_TYPE_OUTBOUND_CA_TO_PA_ENTRY:
        {
            std::vector<sai_outbound_ca_to_pa_entry_t> entries(object_count);

            for (uint32_t it = 0; it < object_count; it++)
            {
                sai_deserialize_outbound_ca_to_pa_entry(objectIds[it], entries[it]);
            }

            status = m_sai->bulkCreate(
                    object_count,
                    entries.data(),
                    attr_counts.data(),
                    attr_lists.data(),
                    mode,
                    statuses.data());
        }
        break;

        default:
            return SAI_STATUS_NOT_SUPPORTED;
    }

    return status;
}

sai_status_t ServerSai::processBulkRemoveEntry(
        _In_ sai_object_type_t objectType,
        _In_ const std::vector<std::string>& objectIds,
        _Out_ std::vector<sai_status_t>& statuses)
{
    SWSS_LOG_ENTER();

    sai_status_t status = SAI_STATUS_SUCCESS;

    uint32_t object_count = (uint32_t) objectIds.size();

    sai_bulk_op_error_mode_t mode = SAI_BULK_OP_ERROR_MODE_IGNORE_ERROR;

    switch ((int)objectType)
    {
        case SAI_OBJECT_TYPE_ROUTE_ENTRY:
        {
            std::vector<sai_route_entry_t> entries(object_count);
            for (uint32_t it = 0; it < object_count; it++)
            {
                sai_deserialize_route_entry(objectIds[it], entries[it]);
            }

            status = m_sai->bulkRemove(
                    object_count,
                    entries.data(),
                    mode,
                    statuses.data());

        }
        break;

        case SAI_OBJECT_TYPE_FDB_ENTRY:
        {
            std::vector<sai_fdb_entry_t> entries(object_count);
            for (uint32_t it = 0; it < object_count; it++)
            {
                sai_deserialize_fdb_entry(objectIds[it], entries[it]);
            }

            status = m_sai->bulkRemove(
                    object_count,
                    entries.data(),
                    mode,
                    statuses.data());

        }
        break;

        case SAI_OBJECT_TYPE_NAT_ENTRY:
        {
            std::vector<sai_nat_entry_t> entries(object_count);
            for (uint32_t it = 0; it < object_count; it++)
            {
                sai_deserialize_nat_entry(objectIds[it], entries[it]);
            }

            status = m_sai->bulkRemove(
                    object_count,
                    entries.data(),
                    mode,
                    statuses.data());

        }
        break;

        case SAI_OBJECT_TYPE_INSEG_ENTRY:
        {
            std::vector<sai_inseg_entry_t> entries(object_count);
            for (uint32_t it = 0; it < object_count; it++)
            {
                sai_deserialize_inseg_entry(objectIds[it], entries[it]);
            }

            status = m_sai->bulkRemove(
                    object_count,
                    entries.data(),
                    mode,
                    statuses.data());

        }
        break;

        case SAI_OBJECT_TYPE_MY_SID_ENTRY:
        {
            std::vector<sai_my_sid_entry_t> entries(object_count);

            for (uint32_t it = 0; it < object_count; it++)
            {
                sai_deserialize_my_sid_entry(objectIds[it], entries[it]);
            }

            status = m_sai->bulkRemove(
                    object_count,
                    entries.data(),
                    mode,
                    statuses.data());
        }
        break;

        case SAI_OBJECT_TYPE_DIRECTION_LOOKUP_ENTRY:
        {
            std::vector<sai_direction_lookup_entry_t> entries(object_count);
            for (uint32_t it = 0; it < object_count; it++)
            {
                sai_deserialize_direction_lookup_entry(objectIds[it], entries[it]);
            }

            status = m_sai->bulkRemove(
                    object_count,
                    entries.data(),
                    mode,
                    statuses.data());
        }
        break;

        case SAI_OBJECT_TYPE_ENI_ETHER_ADDRESS_MAP_ENTRY:
        {
            std::vector<sai_eni_ether_address_map_entry_t> entries(object_count);
            for (uint32_t it = 0; it < object_count; it++)
            {
                sai_deserialize_eni_ether_address_map_entry(objectIds[it], entries[it]);
            }

            status = m_sai->bulkRemove(
                    object_count,
                    entries.data(),
                    mode,
                    statuses.data());
        }
        break;

        case SAI_OBJECT_TYPE_VIP_ENTRY:
        {
            std::vector<sai_vip_entry_t> entries(object_count);

            for (uint32_t it = 0; it < object_count; it++)
            {
                sai_deserialize_vip_entry(objectIds[it], entries[it]);
            }

            status = m_sai->bulkRemove(
                    object_count,
                    entries.data(),
                    mode,
                    statuses.data());
        }
        break;

        case SAI_OBJECT_TYPE_INBOUND_ROUTING_ENTRY:
        {
            std::vector<sai_inbound_routing_entry_t> entries(object_count);
            for (uint32_t it = 0; it < object_count; it++)
            {
                sai_deserialize_inbound_routing_entry(objectIds[it], entries[it]);
            }

            status = m_sai->bulkRemove(
                    object_count,
                    entries.data(),
                    mode,
                    statuses.data());
        }
        break;

        case SAI_OBJECT_TYPE_PA_VALIDATION_ENTRY:
        {
            std::vector<sai_pa_validation_entry_t> entries(object_count);
            for (uint32_t it = 0; it < object_count; it++)
            {
                sai_deserialize_pa_validation_entry(objectIds[it], entries[it]);
            }

            status = m_sai->bulkRemove(
                    object_count,
                    entries.data(),
                    mode,
                    statuses.data());
        }
        break;

        case SAI_OBJECT_TYPE_OUTBOUND_ROUTING_ENTRY:
        {
            std::vector<sai_outbound_routing_entry_t> entries(object_count);
            for (uint32_t it = 0; it < object_count; it++)
            {
                sai_deserialize_outbound_routing_entry(objectIds[it], entries[it]);
            }
            status = m_sai->bulkRemove(
                    object_count,
                    entries.data(),
                    mode,
                    statuses.data());

        }
        break;

        case SAI_OBJECT_TYPE_OUTBOUND_CA_TO_PA_ENTRY:
        {
            std::vector<sai_outbound_ca_to_pa_entry_t> entries(object_count);
            for (uint32_t it = 0; it < object_count; it++)
            {
                sai_deserialize_outbound_ca_to_pa_entry(objectIds[it], entries[it]);
            }

            status = m_sai->bulkRemove(
                    object_count,
                    entries.data(),
                    mode,
                    statuses.data());
        }
        break;

        default:
            return SAI_STATUS_NOT_SUPPORTED;
    }

    return status;
}

sai_status_t ServerSai::processBulkSetEntry(
        _In_ sai_object_type_t objectType,
        _In_ const std::vector<std::string>& objectIds,
        _In_ const std::vector<std::shared_ptr<SaiAttributeList>>& attributes,
        _Out_ std::vector<sai_status_t>& statuses)
{
    SWSS_LOG_ENTER();

    sai_status_t status = SAI_STATUS_SUCCESS;

    std::vector<sai_attribute_t> attr_lists;

    uint32_t object_count = (uint32_t) objectIds.size();

    sai_bulk_op_error_mode_t mode = SAI_BULK_OP_ERROR_MODE_IGNORE_ERROR;

    for (uint32_t it = 0; it < object_count; it++)
    {
        attr_lists.push_back(attributes[it]->get_attr_list()[0]);
    }

    switch (objectType)
    {
        case SAI_OBJECT_TYPE_ROUTE_ENTRY:
        {
            std::vector<sai_route_entry_t> entries(object_count);
            for (uint32_t it = 0; it < object_count; it++)
            {
                sai_deserialize_route_entry(objectIds[it], entries[it]);
            }

            status = m_sai->bulkSet(
                    object_count,
                    entries.data(),
                    attr_lists.data(),
                    mode,
                    statuses.data());

        }
        break;

        case SAI_OBJECT_TYPE_FDB_ENTRY:
        {
            std::vector<sai_fdb_entry_t> entries(object_count);
            for (uint32_t it = 0; it < object_count; it++)
            {
                sai_deserialize_fdb_entry(objectIds[it], entries[it]);
            }

            status = m_sai->bulkSet(
                    object_count,
                    entries.data(),
                    attr_lists.data(),
                    mode,
                    statuses.data());

        }
        break;

        case SAI_OBJECT_TYPE_NAT_ENTRY:
        {
            std::vector<sai_nat_entry_t> entries(object_count);
            for (uint32_t it = 0; it < object_count; it++)
            {
                sai_deserialize_nat_entry(objectIds[it], entries[it]);
            }

            status = m_sai->bulkSet(
                    object_count,
                    entries.data(),
                    attr_lists.data(),
                    mode,
                    statuses.data());

        }
        break;

        case SAI_OBJECT_TYPE_INSEG_ENTRY:
        {
            std::vector<sai_inseg_entry_t> entries(object_count);
            for (uint32_t it = 0; it < object_count; it++)
            {
                sai_deserialize_inseg_entry(objectIds[it], entries[it]);
            }

            status = m_sai->bulkSet(
                    object_count,
                    entries.data(),
                    attr_lists.data(),
                    mode,
                    statuses.data());

        }
        break;

        case SAI_OBJECT_TYPE_MY_SID_ENTRY:
        {
            std::vector<sai_my_sid_entry_t> entries(object_count);

            for (uint32_t it = 0; it < object_count; it++)
            {
                sai_deserialize_my_sid_entry(objectIds[it], entries[it]);
            }

            status = m_sai->bulkSet(
                    object_count,
                    entries.data(),
                    attr_lists.data(),
                    mode,
                    statuses.data());
        }
        break;

        default:
            return SAI_STATUS_NOT_SUPPORTED;
    }

    return status;
}

void ServerSai::sendBulkApiResponse(
        _In_ sai_common_api_t api,
        _In_ sai_status_t status,
        _In_ uint32_t object_count,
        _In_ const sai_object_id_t* object_ids,
        _In_ const sai_status_t* statuses)
{
    SWSS_LOG_ENTER();

    switch (api)
    {
        case SAI_COMMON_API_BULK_CREATE:
        case SAI_COMMON_API_BULK_REMOVE:
        case SAI_COMMON_API_BULK_SET:
            break;

        default:
            SWSS_LOG_THROW("api %s not supported by this function",
                    sai_serialize_common_api(api).c_str());
    }

    std::vector<swss::FieldValueTuple> entry;

    for (uint32_t idx = 0; idx < object_count; idx++)
    {
        entry.emplace_back(sai_serialize_status(statuses[idx]), "");
    }

    if (api == SAI_COMMON_API_BULK_CREATE)
    {
        // in case of bulk create api, we need to return oids values that was
        // created to the client

        for (uint32_t idx = 0; idx < object_count; idx++)
        {
            entry.emplace_back("oid", sai_serialize_object_id(object_ids[idx]));
        }
    }

    std::string strStatus = sai_serialize_status(status);

    SWSS_LOG_INFO("sending response for %s api with status: %s",
            sai_serialize_common_api(api).c_str(),
            strStatus.c_str());

    m_selectableChannel->set(strStatus, entry, REDIS_ASIC_STATE_COMMAND_GETRESPONSE);
}

sai_status_t ServerSai::processAttrCapabilityQuery(
        _In_ const swss::KeyOpFieldsValuesTuple &kco)
{
    SWSS_LOG_ENTER();

    auto& strSwitchOid = kfvKey(kco);

    sai_object_id_t switchOid;
    sai_deserialize_object_id(strSwitchOid, switchOid);

    auto& values = kfvFieldsValues(kco);

    if (values.size() != 2)
    {
        SWSS_LOG_ERROR("Invalid input: expected 2 arguments, received %zu", values.size());

        m_selectableChannel->set(sai_serialize_status(SAI_STATUS_INVALID_PARAMETER), {}, REDIS_ASIC_STATE_COMMAND_ATTR_CAPABILITY_RESPONSE);

        return SAI_STATUS_INVALID_PARAMETER;
    }

    sai_object_type_t objectType;
    sai_deserialize_object_type(fvValue(values[0]), objectType);

    sai_attr_id_t attrId;
    sai_deserialize_attr_id(fvValue(values[1]), attrId);

    sai_attr_capability_t capability;

    sai_status_t status = m_sai->queryAttributeCapability(switchOid, objectType, attrId, &capability);

    std::vector<swss::FieldValueTuple> entry;

    if (status == SAI_STATUS_SUCCESS)
    {
        entry =
        {
            swss::FieldValueTuple("CREATE_IMPLEMENTED", (capability.create_implemented ? "true" : "false")),
            swss::FieldValueTuple("SET_IMPLEMENTED",    (capability.set_implemented    ? "true" : "false")),
            swss::FieldValueTuple("GET_IMPLEMENTED",    (capability.get_implemented    ? "true" : "false"))
        };

        SWSS_LOG_INFO("Sending response: create_implemented:%d, set_implemented:%d, get_implemented:%d",
            capability.create_implemented, capability.set_implemented, capability.get_implemented);
    }

    m_selectableChannel->set(sai_serialize_status(status), entry, REDIS_ASIC_STATE_COMMAND_ATTR_CAPABILITY_RESPONSE);

    return status;
}

sai_status_t ServerSai::processAttrEnumValuesCapabilityQuery(
        _In_ const swss::KeyOpFieldsValuesTuple &kco)
{
    SWSS_LOG_ENTER();

    auto& strSwitchOid = kfvKey(kco);

    sai_object_id_t switchOid;
    sai_deserialize_object_id(strSwitchOid, switchOid);

    auto& values = kfvFieldsValues(kco);

    if (values.size() != 3)
    {
        SWSS_LOG_ERROR("Invalid input: expected 3 arguments, received %zu", values.size());

        m_selectableChannel->set(sai_serialize_status(SAI_STATUS_INVALID_PARAMETER), {}, REDIS_ASIC_STATE_COMMAND_ATTR_ENUM_VALUES_CAPABILITY_RESPONSE);

        return SAI_STATUS_INVALID_PARAMETER;
    }

    sai_object_type_t objectType;
    sai_deserialize_object_type(fvValue(values[0]), objectType);

    sai_attr_id_t attrId;
    sai_deserialize_attr_id(fvValue(values[1]), attrId);

    uint32_t list_size = std::stoi(fvValue(values[2]));

    std::vector<int32_t> enum_capabilities_list(list_size);

    sai_s32_list_t enumCapList;

    enumCapList.count = list_size;
    enumCapList.list = enum_capabilities_list.data();

    sai_status_t status = m_sai->queryAttributeEnumValuesCapability(switchOid, objectType, attrId, &enumCapList);

    std::vector<swss::FieldValueTuple> entry;

    if (status == SAI_STATUS_SUCCESS)
    {
        std::vector<std::string> vec;
        std::transform(enumCapList.list, enumCapList.list + enumCapList.count,
                std::back_inserter(vec), [](auto&e) { return std::to_string(e); });

        std::ostringstream join;
        std::copy(vec.begin(), vec.end(), std::ostream_iterator<std::string>(join, ","));

        auto strCap = join.str();

        entry =
        {
            swss::FieldValueTuple("ENUM_CAPABILITIES", strCap),
            swss::FieldValueTuple("ENUM_COUNT", std::to_string(enumCapList.count))
        };

        SWSS_LOG_DEBUG("Sending response: capabilities = '%s', count = %d", strCap.c_str(), enumCapList.count);
    }

    m_selectableChannel->set(sai_serialize_status(status), entry, REDIS_ASIC_STATE_COMMAND_ATTR_ENUM_VALUES_CAPABILITY_RESPONSE);

    return status;
}

sai_status_t ServerSai::processObjectTypeGetAvailabilityQuery(
    _In_ const swss::KeyOpFieldsValuesTuple &kco)
{
    SWSS_LOG_ENTER();

    auto& strSwitchOid = kfvKey(kco);

    sai_object_id_t switchOid;
    sai_deserialize_object_id(strSwitchOid, switchOid);

    std::vector<swss::FieldValueTuple> values = kfvFieldsValues(kco);

    // needs to pop the object type off the end of the list in order to
    // retrieve the attribute list

    sai_object_type_t objectType;
    sai_deserialize_object_type(fvValue(values.back()), objectType);

    values.pop_back();

    SaiAttributeList list(objectType, values, false);

    sai_attribute_t *attr_list = list.get_attr_list();

    uint32_t attr_count = list.get_attr_count();

    uint64_t count;

    sai_status_t status = m_sai->objectTypeGetAvailability(
            switchOid,
            objectType,
            attr_count,
            attr_list,
            &count);

    std::vector<swss::FieldValueTuple> entry;

    if (status == SAI_STATUS_SUCCESS)
    {
        entry.push_back(swss::FieldValueTuple("OBJECT_COUNT", std::to_string(count)));

        SWSS_LOG_DEBUG("Sending response: count = %lu", count);
    }

    m_selectableChannel->set(sai_serialize_status(status), entry, REDIS_ASIC_STATE_COMMAND_OBJECT_TYPE_GET_AVAILABILITY_RESPONSE);

    return status;
}

sai_status_t ServerSai::processStatsCapabilityQuery(
        _In_ const swss::KeyOpFieldsValuesTuple &kco)
{
    SWSS_LOG_ENTER();

    auto& strSwitchOid = kfvKey(kco);

    sai_object_id_t switchOid;
    sai_deserialize_object_id(strSwitchOid, switchOid);

    auto& values = kfvFieldsValues(kco);

    if (values.size() != 2)
    {
        SWSS_LOG_ERROR("Invalid input: expected 2 arguments, received %zu", values.size());

        m_selectableChannel->set(sai_serialize_status(SAI_STATUS_INVALID_PARAMETER), {}, REDIS_ASIC_STATE_COMMAND_STATS_CAPABILITY_RESPONSE);

        return SAI_STATUS_INVALID_PARAMETER;
    }

    sai_object_type_t objectType;
    sai_deserialize_object_type(fvValue(values[0]), objectType);

    uint32_t list_size = std::stoi(fvValue(values[1]));

    std::vector<sai_stat_capability_t> stat_capability_list(list_size);

    sai_stat_capability_list_t statCapList;

    statCapList.count = list_size;
    statCapList.list = stat_capability_list.data();

    sai_status_t status = m_sai->queryStatsCapability(switchOid, objectType, &statCapList);

    std::vector<swss::FieldValueTuple> entry;

    if (status == SAI_STATUS_SUCCESS)
    {
        std::vector<std::string> vec_stat_enum;
        std::vector<std::string> vec_stat_modes;

        for (uint32_t it = 0; it < statCapList.count; it++)
        {
                vec_stat_enum.push_back(std::to_string(statCapList.list[it].stat_enum));
                vec_stat_modes.push_back(std::to_string(statCapList.list[it].stat_modes));
        }

        std::ostringstream join_stat_enum;
        std::copy(vec_stat_enum.begin(), vec_stat_enum.end(), std::ostream_iterator<std::string>(join_stat_enum, ","));
        auto strCapEnum = join_stat_enum.str();

        std::ostringstream join_stat_modes;
        std::copy(vec_stat_modes.begin(), vec_stat_modes.end(), std::ostream_iterator<std::string>(join_stat_modes, ","));
        auto strCapModes = join_stat_modes.str();

        entry =
        {
            swss::FieldValueTuple("STAT_ENUM", strCapEnum),
            swss::FieldValueTuple("STAT_MODES", strCapModes),
            swss::FieldValueTuple("STAT_COUNT", std::to_string(statCapList.count))
        };

        SWSS_LOG_DEBUG("Sending response: stat_enums = '%s', stat_modes = '%s', count = %d",
                        strCapEnum.c_str(), strCapModes.c_str(), statCapList.count);
    }
    else if (status == SAI_STATUS_BUFFER_OVERFLOW)
    {
        entry =
        {
            swss::FieldValueTuple("STAT_COUNT", std::to_string(statCapList.count))
        };

        SWSS_LOG_DEBUG("Sending response: count = %u", statCapList.count);
    }

    m_selectableChannel->set(sai_serialize_status(status), entry, REDIS_ASIC_STATE_COMMAND_STATS_CAPABILITY_RESPONSE);

    return status;
}

sai_status_t ServerSai::processStatsStCapabilityQuery(
    _In_ const swss::KeyOpFieldsValuesTuple &kco)
{
    SWSS_LOG_ENTER();

    auto &strSwitchOid = kfvKey(kco);

    sai_object_id_t switchOid;
    sai_deserialize_object_id(strSwitchOid, switchOid);

    auto &values = kfvFieldsValues(kco);

    if (values.size() != 2)
    {
        SWSS_LOG_ERROR("Invalid input: expected 2 arguments, received %zu", values.size());

        m_selectableChannel->set(sai_serialize_status(SAI_STATUS_INVALID_PARAMETER), {}, REDIS_ASIC_STATE_COMMAND_STATS_ST_CAPABILITY_RESPONSE);

        return SAI_STATUS_INVALID_PARAMETER;
    }

    sai_object_type_t objectType;
    sai_deserialize_object_type(fvValue(values[0]), objectType);

    uint32_t list_size = std::stoi(fvValue(values[1]));

    std::vector<sai_stat_st_capability_t> stat_capability_list(list_size);

    sai_stat_st_capability_list_t statCapList;

    statCapList.count = list_size;
    statCapList.list = stat_capability_list.data();

    sai_status_t status = m_sai->queryStatsStCapability(switchOid, objectType, &statCapList);

    std::vector<swss::FieldValueTuple> entry;

    if (status == SAI_STATUS_SUCCESS)
    {
        std::vector<std::string> vec_stat_enum;
        std::vector<std::string> vec_stat_modes;
        std::vector<std::string> vec_minimal_polling_intervals;

        for (uint32_t it = 0; it < statCapList.count; it++)
        {
            vec_stat_enum.push_back(std::to_string(statCapList.list[it].capability.stat_enum));
            vec_stat_modes.push_back(std::to_string(statCapList.list[it].capability.stat_modes));
            vec_minimal_polling_intervals.push_back(std::to_string(statCapList.list[it].minimal_polling_interval));
        }

        std::ostringstream join_stat_enum;
        std::copy(vec_stat_enum.begin(), vec_stat_enum.end(), std::ostream_iterator<std::string>(join_stat_enum, ","));
        auto strCapEnum = join_stat_enum.str();

        std::ostringstream join_stat_modes;
        std::copy(vec_stat_modes.begin(), vec_stat_modes.end(), std::ostream_iterator<std::string>(join_stat_modes, ","));
        auto strCapModes = join_stat_modes.str();

        std::ostringstream join_minimal_polling_intervals;
        std::copy(vec_minimal_polling_intervals.begin(), vec_minimal_polling_intervals.end(), std::ostream_iterator<std::string>(join_minimal_polling_intervals, ","));
        auto strCapMinPollInt = join_minimal_polling_intervals.str();

        entry =
            {
                swss::FieldValueTuple("STAT_ENUM", strCapEnum),
                swss::FieldValueTuple("STAT_MODES", strCapModes),
                swss::FieldValueTuple("MINIMAL_POLLING_INTERVALS", strCapMinPollInt),
                swss::FieldValueTuple("STAT_COUNT", std::to_string(statCapList.count))};

        SWSS_LOG_DEBUG("Sending response: stat_enums = '%s', stat_modes = '%s', minimal_polling_intervals = '%s' count = %d",
                       strCapEnum.c_str(), strCapModes.c_str(), strCapMinPollInt.c_str(), statCapList.count);
    }
    else if (status == SAI_STATUS_BUFFER_OVERFLOW)
    {
        entry = {swss::FieldValueTuple("STAT_COUNT", std::to_string(statCapList.count))};

        SWSS_LOG_DEBUG("Sending response: count = %u", statCapList.count);
    }

    m_selectableChannel->set(sai_serialize_status(status), entry, REDIS_ASIC_STATE_COMMAND_STATS_ST_CAPABILITY_RESPONSE);

    return status;
}

sai_status_t ServerSai::processFdbFlush(
        _In_ const swss::KeyOpFieldsValuesTuple &kco)
{
    SWSS_LOG_ENTER();

    auto& key = kfvKey(kco);
    auto strSwitchOid = key.substr(key.find(":") + 1);

    sai_object_id_t switchOid;
    sai_deserialize_object_id(strSwitchOid, switchOid);

    auto& values = kfvFieldsValues(kco);

    for (const auto &v: values)
    {
        SWSS_LOG_DEBUG("attr: %s: %s", fvField(v).c_str(), fvValue(v).c_str());
    }

    SaiAttributeList list(SAI_OBJECT_TYPE_FDB_FLUSH, values, false);

    sai_attribute_t *attr_list = list.get_attr_list();
    uint32_t attr_count = list.get_attr_count();

    sai_status_t status = m_sai->flushFdbEntries(switchOid, attr_count, attr_list);

    m_selectableChannel->set(sai_serialize_status(status), {} , REDIS_ASIC_STATE_COMMAND_FLUSHRESPONSE);

    return status;
}

sai_status_t ServerSai::processClearStatsEvent(
        _In_ const swss::KeyOpFieldsValuesTuple &kco)
{
    SWSS_LOG_ENTER();

    const std::string &key = kfvKey(kco);

    sai_object_meta_key_t metaKey;
    sai_deserialize_object_meta_key(key, metaKey);

    auto info = sai_metadata_get_object_type_info(metaKey.objecttype);

    if (info->isnonobjectid)
    {
        SWSS_LOG_THROW("non object id not supported on clear stats: %s, FIXME", key.c_str());
    }

    std::vector<sai_stat_id_t> counter_ids;

    for (auto&v: kfvFieldsValues(kco))
    {
        int32_t val;
        sai_deserialize_enum(fvField(v), info->statenum, val);

        counter_ids.push_back(val);
    }

    auto status = m_sai->clearStats(
            metaKey.objecttype,
            metaKey.objectkey.key.object_id,
            (uint32_t)counter_ids.size(),
            counter_ids.data());

    m_selectableChannel->set(sai_serialize_status(status), {}, REDIS_ASIC_STATE_COMMAND_GETRESPONSE);

    return status;
}

sai_status_t ServerSai::processGetStatsEvent(
        _In_ const swss::KeyOpFieldsValuesTuple &kco)
{
    SWSS_LOG_ENTER();

    const std::string &key = kfvKey(kco);

    sai_object_meta_key_t metaKey;
    sai_deserialize_object_meta_key(key, metaKey);

    // TODO get stats on created object in init view mode could fail

    auto info = sai_metadata_get_object_type_info(metaKey.objecttype);

    if (info->isnonobjectid)
    {
        SWSS_LOG_THROW("non object id not supported on clear stats: %s, FIXME", key.c_str());
    }

    std::vector<sai_stat_id_t> counter_ids;

    for (auto&v: kfvFieldsValues(kco))
    {
        int32_t val;
        sai_deserialize_enum(fvField(v), info->statenum, val);

        counter_ids.push_back(val);
    }

    std::vector<uint64_t> result(counter_ids.size());

    auto status = m_sai->getStats(
            metaKey.objecttype,
            metaKey.objectkey.key.object_id,
            (uint32_t)counter_ids.size(),
            counter_ids.data(),
            result.data());

    std::vector<swss::FieldValueTuple> entry;

    if (status != SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_ERROR("Failed to get stats");
    }
    else
    {
        const auto& values = kfvFieldsValues(kco);

        for (size_t i = 0; i < values.size(); i++)
        {
            entry.emplace_back(fvField(values[i]), std::to_string(result[i]));
        }
    }

    m_selectableChannel->set(sai_serialize_status(status), entry, REDIS_ASIC_STATE_COMMAND_GETRESPONSE);

    return status;
}
