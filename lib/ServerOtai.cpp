#include "ServerOtai.h"
#include "OtaiInternal.h"
#include "Otai.h"
#include "ServerConfig.h"
#include "otairediscommon.h"

#include "meta/otai_serialize.h"
#include "meta/OtaiAttributeList.h"
#include "meta/ZeroMQSelectableChannel.h"

#include "swss/logger.h"
#include "swss/select.h"
#include "swss/tokenize.h"

#include <iterator>
#include <algorithm>

using namespace otairedis;
using namespace otaimeta;
using namespace std::placeholders;

#define REDIS_CHECK_API_INITIALIZED()                                       \
    if (!m_apiInitialized) {                                                \
        SWSS_LOG_ERROR("%s: api not initialized", __PRETTY_FUNCTION__);     \
        return OTAI_STATUS_FAILURE; }

ServerOtai::ServerOtai()
{
    SWSS_LOG_ENTER();

    m_apiInitialized = false;

    m_runServerThread = false;
}

ServerOtai::~ServerOtai()
{
    SWSS_LOG_ENTER();

    if (m_apiInitialized)
    {
        apiUninitialize();
    }
}

// INITIALIZE UNINITIALIZE

otai_status_t ServerOtai::apiInitialize(
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

    m_otai = std::make_shared<Otai>(); // actual OTAI to talk to syncd

    auto status = m_otai->apiInitialize(flags, service_method_table);

    SWSS_LOG_NOTICE("init client/server otai: %s", otai_serialize_status(status).c_str());

    if (status == OTAI_STATUS_SUCCESS)
    {
        auto serverConfig = service_method_table->profile_get_value(0, OTAI_REDIS_KEY_SERVER_CONFIG);

        auto cc = ServerConfig::loadFromFile(serverConfig);

        m_selectableChannel = std::make_shared<ZeroMQSelectableChannel>(cc->m_zmqEndpoint);

        SWSS_LOG_NOTICE("starting server thread");

        m_runServerThread = true;

        m_serverThread = std::make_shared<std::thread>(&ServerOtai::serverThreadFunction, this);

        m_apiInitialized = true;
    }

    return status;
}

otai_status_t ServerOtai::apiUninitialize(void)
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

    m_otai = nullptr;

    SWSS_LOG_NOTICE("end");

    return OTAI_STATUS_SUCCESS;
}

// QUAD OID

otai_status_t ServerOtai::create(
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

otai_status_t ServerOtai::remove(
        _In_ otai_object_type_t objectType,
        _In_ otai_object_id_t objectId)
{
    MUTEX();
    SWSS_LOG_ENTER();
    REDIS_CHECK_API_INITIALIZED();

    return m_otai->remove(objectType, objectId);
}

otai_status_t ServerOtai::set(
        _In_ otai_object_type_t objectType,
        _In_ otai_object_id_t objectId,
        _In_ const otai_attribute_t *attr)
{
    MUTEX();
    SWSS_LOG_ENTER();
    REDIS_CHECK_API_INITIALIZED();

    return m_otai->set(objectType, objectId, attr);
}

otai_status_t ServerOtai::get(
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
otai_status_t ServerOtai::create(                                     \
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
otai_status_t ServerOtai::remove(                             \
        _In_ const otai_ ## ot ## _t* entry)                 \
{                                                           \
    MUTEX();                                                \
    SWSS_LOG_ENTER();                                       \
    REDIS_CHECK_API_INITIALIZED();                          \
    return m_otai->remove(entry);                            \
}

OTAIREDIS_DECLARE_EVERY_ENTRY(DECLARE_REMOVE_ENTRY);

#define DECLARE_SET_ENTRY(OT,ot)                            \
otai_status_t ServerOtai::set(                                \
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
otai_status_t ServerOtai::get(                                    \
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

otai_status_t ServerOtai::getStats(
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


otai_status_t ServerOtai::getStatsExt(
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

otai_status_t ServerOtai::clearStats(
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

otai_status_t ServerOtai::bulkGetStats(
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

otai_status_t ServerOtai::bulkClearStats(
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

otai_status_t ServerOtai::bulkCreate(
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

otai_status_t ServerOtai::bulkRemove(
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

otai_status_t ServerOtai::bulkSet(
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

otai_status_t ServerOtai::bulkGet(
        _In_ otai_object_type_t object_type,
        _In_ uint32_t object_count,
        _In_ const otai_object_id_t *object_id,
        _In_ const uint32_t *attr_count,
        _Inout_ otai_attribute_t **attr_list,
        _In_ otai_bulk_op_error_mode_t mode,
        _Out_ otai_status_t *object_statuses)
{
    MUTEX();
    SWSS_LOG_ENTER();
    REDIS_CHECK_API_INITIALIZED();

    SWSS_LOG_ERROR("not implemented, FIXME");

    return OTAI_STATUS_NOT_IMPLEMENTED;
}

// BULK QUAD ENTRY

#define DECLARE_BULK_CREATE_ENTRY(OT,ot)                    \
otai_status_t ServerOtai::bulkCreate(                         \
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
otai_status_t ServerOtai::bulkRemove(                         \
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
otai_status_t ServerOtai::bulkSet(                            \
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
otai_status_t ServerOtai::bulkGet(                            \
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

otai_status_t ServerOtai::flushFdbEntries(
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

otai_status_t ServerOtai::objectTypeGetAvailability(
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


otai_status_t ServerOtai::queryAttributeEnumValuesCapability(
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

otai_object_type_t ServerOtai::objectTypeQuery(
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

otai_object_id_t ServerOtai::switchIdQuery(
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

otai_status_t ServerOtai::logSet(
        _In_ otai_api_t api,
        _In_ otai_log_level_t log_level)
{
    MUTEX();
    SWSS_LOG_ENTER();
    REDIS_CHECK_API_INITIALIZED();

    return m_otai->logSet(api, log_level);
}


void ServerOtai::serverThreadFunction()
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

void ServerOtai::processEvent(
        _In_ SelectableChannel& consumer)
{
    MUTEX();
    SWSS_LOG_ENTER();

    if (!m_apiInitialized)
    {
        SWSS_LOG_ERROR("%s: OTAI API not initialized", __PRETTY_FUNCTION__);

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

otai_status_t ServerOtai::processSingleEvent(
        _In_ const swss::KeyOpFieldsValuesTuple &kco)
{
    SWSS_LOG_ENTER();

    auto& key = kfvKey(kco);
    auto& op = kfvOp(kco);

    SWSS_LOG_INFO("key: %s op: %s", key.c_str(), op.c_str());

    if (key.length() == 0)
    {
        SWSS_LOG_DEBUG("no elements in m_buffer");

        return OTAI_STATUS_SUCCESS;
    }

    if (op == REDIS_ASIC_STATE_COMMAND_CREATE)
        return processQuadEvent(OTAI_COMMON_API_CREATE, kco);

    if (op == REDIS_ASIC_STATE_COMMAND_REMOVE)
        return processQuadEvent(OTAI_COMMON_API_REMOVE, kco);

    if (op == REDIS_ASIC_STATE_COMMAND_SET)
        return processQuadEvent(OTAI_COMMON_API_SET, kco);

    if (op == REDIS_ASIC_STATE_COMMAND_GET)
        return processQuadEvent(OTAI_COMMON_API_GET, kco);

    if (op == REDIS_ASIC_STATE_COMMAND_BULK_CREATE)
        return processBulkQuadEvent(OTAI_COMMON_API_BULK_CREATE, kco);

    if (op == REDIS_ASIC_STATE_COMMAND_BULK_REMOVE)
        return processBulkQuadEvent(OTAI_COMMON_API_BULK_REMOVE, kco);

    if (op == REDIS_ASIC_STATE_COMMAND_BULK_SET)
        return processBulkQuadEvent(OTAI_COMMON_API_BULK_SET, kco);

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

    SWSS_LOG_THROW("event op '%s' is not implemented, FIXME", op.c_str());
}

otai_status_t ServerOtai::processQuadEvent(
        _In_ otai_common_api_t api,
        _In_ const swss::KeyOpFieldsValuesTuple &kco)
{
    SWSS_LOG_ENTER();

    const std::string& key = kfvKey(kco);
    const std::string& op = kfvOp(kco);

    const std::string& strObjectId = key.substr(key.find(":") + 1);

    otai_object_meta_key_t metaKey;
    otai_deserialize_object_meta_key(key, metaKey);

    if (!otai_metadata_is_object_type_valid(metaKey.objecttype))
    {
        SWSS_LOG_THROW("invalid object type %s", key.c_str());
    }

    SWSS_LOG_NOTICE("got server request: %s : %s", key.c_str(), op.c_str());

    auto& values = kfvFieldsValues(kco);

    for (auto& v: values)
    {
        SWSS_LOG_NOTICE(" - attr: %s: %s", fvField(v).c_str(), fvValue(v).c_str());
    }

    OtaiAttributeList list(metaKey.objecttype, values, false);

    otai_attribute_t *attr_list = list.get_attr_list();
    uint32_t attr_count = list.get_attr_count();

    /*
     * NOTE: Need to translate client notification pointers to otairedis
     * pointers since they reside in different memory space.
     */

    if (metaKey.objecttype == OTAI_OBJECT_TYPE_LINECARD && (api == OTAI_COMMON_API_CREATE || api == OTAI_COMMON_API_SET))
    {
        /*
         * Since we will be operating on existing switch, it may happen that
         * client will set notification pointer, which is not actually set in
         * otairedis server.  Then we would need to take extra steps in server
         * to forward those notifications. We could also drop notification
         * request on clients side, since it would be possible only to set only
         * existing pointers.
         *
         * TODO: must be done per switch, and switch may not exists yet
         */

        // TODO m_handler->updateNotificationsPointers(attr_count, attr_list);
        SWSS_LOG_ERROR("TODO, update notification pointers, FIXME!");
    }

    auto info = otai_metadata_get_object_type_info(metaKey.objecttype);

    otai_status_t status;

    // NOTE: for create api (even switch) passed oid will be OTAI_NULL_OBJECT_ID

    otai_object_id_t oid = OTAI_NULL_OBJECT_ID;

    if (info->isnonobjectid)
    {
        status = processEntry(metaKey, api, attr_count, attr_list);
    }
    else
    {
        otai_deserialize_object_id(strObjectId, oid);

        // In case of create oid object, client don't know what will be created
        // object id before it receive it, so instead of that it will transfer
        // switch id in that place since switch id is needed to create other
        // object in the first place. If create method is for switch, this
        // field will be OTAI_NULL_OBJECT_ID.

        otai_object_id_t switchOid = oid;

        status = processOid(metaKey.objecttype, oid, switchOid, api, attr_count, attr_list);
    }

    if (api == OTAI_COMMON_API_GET)
    {
        sendGetResponse(metaKey.objecttype, strObjectId, status, attr_count, attr_list);
    }
    else if (status != OTAI_STATUS_SUCCESS)
    {
        sendApiResponse(api, status, oid);

        SWSS_LOG_ERROR("api failed: %s (%s): %s",
                key.c_str(),
                op.c_str(),
                otai_serialize_status(status).c_str());

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

void ServerOtai::sendApiResponse(
        _In_ otai_common_api_t api,
        _In_ otai_status_t status,
        _In_ otai_object_id_t oid)
{
    SWSS_LOG_ENTER();

    switch (api)
    {
        case OTAI_COMMON_API_CREATE:
        case OTAI_COMMON_API_REMOVE:
        case OTAI_COMMON_API_SET:
            break;

        default:
            SWSS_LOG_THROW("api %s not supported by this function",
                    otai_serialize_common_api(api).c_str());
    }

    std::vector<swss::FieldValueTuple> entry;

    if (api == OTAI_COMMON_API_CREATE)
    {
        // in case of create api, we need to return OID value that was created
        // to the client

        entry.emplace_back("oid", otai_serialize_object_id(oid));
    }

    std::string strStatus = otai_serialize_status(status);

    SWSS_LOG_INFO("sending response for %s api with status: %s",
            otai_serialize_common_api(api).c_str(),
            strStatus.c_str());

    m_selectableChannel->set(strStatus, entry, REDIS_ASIC_STATE_COMMAND_GETRESPONSE);
}

void ServerOtai::sendGetResponse(
        _In_ otai_object_type_t objectType,
        _In_ const std::string& strObjectId,
        _In_ otai_status_t status,
        _In_ uint32_t attr_count,
        _In_ otai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    std::vector<swss::FieldValueTuple> entry;

    if (status == OTAI_STATUS_SUCCESS)
    {
        entry = OtaiAttributeList::serialize_attr_list(
                objectType,
                attr_count,
                attr_list,
                false);
    }
    else if (status == OTAI_STATUS_BUFFER_OVERFLOW)
    {
        /*
         * In this case we got correct values for list, but list was too small
         * so serialize only count without list itself, otairedis will need to
         * take this into account when deserialize.
         *
         * If there was a list somewhere, count will be changed to actual value
         * different attributes can have different lists, many of them may
         * serialize only count, and will need to support that on the receiver.
         */

        entry = OtaiAttributeList::serialize_attr_list(
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

    std::string strStatus = otai_serialize_status(status);

    SWSS_LOG_INFO("sending response for GET api with status: %s", strStatus.c_str());

    /*
     * Since we have only one get at a time, we don't have to serialize object
     * type and object id, only get status is required to be returned.  Get
     * response will not put any data to table, only queue is used.
     */

    m_selectableChannel->set(strStatus, entry, REDIS_ASIC_STATE_COMMAND_GETRESPONSE);

    SWSS_LOG_INFO("response for GET api was send");
}

otai_status_t ServerOtai::processEntry(
        _In_ otai_object_meta_key_t metaKey,
        _In_ otai_common_api_t api,
        _In_ uint32_t attr_count,
        _In_ otai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    switch (api)
    {
        case OTAI_COMMON_API_CREATE:
            return m_otai->create(metaKey, OTAI_NULL_OBJECT_ID, attr_count, attr_list);

        case OTAI_COMMON_API_REMOVE:
            return m_otai->remove(metaKey);

        case OTAI_COMMON_API_SET:
            return m_otai->set(metaKey, attr_list);

        case OTAI_COMMON_API_GET:
            return m_otai->get(metaKey, attr_count, attr_list);

        default:

            SWSS_LOG_THROW("api %s not supported", otai_serialize_common_api(api).c_str());
    }
}

otai_status_t ServerOtai::processOid(
        _In_ otai_object_type_t objectType,
        _Inout_ otai_object_id_t& oid,
        _In_ otai_object_id_t switchOid,
        _In_ otai_common_api_t api,
        _In_ uint32_t attr_count,
        _In_ otai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    SWSS_LOG_DEBUG("calling %s for %s",
            otai_serialize_common_api(api).c_str(),
            otai_serialize_object_type(objectType).c_str());

    auto info = otai_metadata_get_object_type_info(objectType);

    if (info->isnonobjectid)
    {
        SWSS_LOG_THROW("passing non object id %s as generic object", info->objecttypename);
    }

    switch (api)
    {
        case OTAI_COMMON_API_CREATE:
            return m_otai->create(objectType, &oid, switchOid, attr_count, attr_list);

        case OTAI_COMMON_API_REMOVE:
            return m_otai->remove(objectType, oid);

        case OTAI_COMMON_API_SET:
            return m_otai->set(objectType, oid, attr_list);

        case OTAI_COMMON_API_GET:
            return m_otai->get(objectType, oid, attr_count, attr_list);

        default:

            SWSS_LOG_THROW("common api (%s) is not implemented", otai_serialize_common_api(api).c_str());
    }
}

otai_status_t ServerOtai::processBulkQuadEvent(
        _In_ otai_common_api_t api,
        _In_ const swss::KeyOpFieldsValuesTuple &kco)
{
    SWSS_LOG_ENTER();

    const std::string& key = kfvKey(kco); // objectType:count

    std::string strObjectType = key.substr(0, key.find(":"));

    otai_object_type_t objectType;
    otai_deserialize_object_type(strObjectType, objectType);

    const std::vector<swss::FieldValueTuple> &values = kfvFieldsValues(kco);

    std::vector<std::vector<swss::FieldValueTuple>> strAttributes;

    // field = objectId
    // value = attrid=attrvalue|...

    std::vector<std::string> objectIds;

    std::vector<std::shared_ptr<OtaiAttributeList>> attributes;

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

        auto list = std::make_shared<OtaiAttributeList>(objectType, entries, false);

        attributes.push_back(list);
    }

    SWSS_LOG_INFO("bulk %s executing with %zu items",
            strObjectType.c_str(),
            objectIds.size());

    auto info = otai_metadata_get_object_type_info(objectType);

    if (info->isobjectid)
    {
        return processBulkOid(objectType, objectIds, api, attributes, strAttributes);
    }
    else
    {
        return processBulkEntry(objectType, objectIds, api, attributes, strAttributes);
    }
}

otai_status_t ServerOtai::processBulkOid(
        _In_ otai_object_type_t objectType,
        _In_ const std::vector<std::string>& strObjectIds,
        _In_ otai_common_api_t api,
        _In_ const std::vector<std::shared_ptr<OtaiAttributeList>>& attributes,
        _In_ const std::vector<std::vector<swss::FieldValueTuple>>& strAttributes)
{
    SWSS_LOG_ENTER();

    auto info = otai_metadata_get_object_type_info(objectType);

    if (info->isnonobjectid)
    {
        SWSS_LOG_THROW("passing non object id to bulk oid object operation");
    }

    std::vector<otai_status_t> statuses(strObjectIds.size(), OTAI_STATUS_FAILURE);

    otai_status_t status = OTAI_STATUS_FAILURE;

    otai_bulk_op_error_mode_t mode = OTAI_BULK_OP_ERROR_MODE_IGNORE_ERROR;

    std::vector<otai_object_id_t> objectIds(strObjectIds.size());

    for (size_t idx = 0; idx < objectIds.size(); idx++)
    {
        otai_deserialize_object_id(strObjectIds[idx], objectIds[idx]);
    }

    size_t object_count = objectIds.size();

    switch (api)
    {
        case OTAI_COMMON_API_BULK_CREATE:

            {
                std::vector<uint32_t> attr_counts(object_count);
                std::vector<const otai_attribute_t*> attr_lists(object_count);

                for (size_t idx = 0; idx < object_count; idx++)
                {
                    attr_counts[idx] = attributes[idx]->get_attr_count();
                    attr_lists[idx] = attributes[idx]->get_attr_list();
                }

                // in case of client/server, all passed oids are switch OID since
                // client don't know what oid will be assigned to created object
                otai_object_id_t switchId = objectIds.at(0);

                status = m_otai->bulkCreate(
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

        case OTAI_COMMON_API_BULK_REMOVE:

            status = m_otai->bulkRemove(
                    objectType,
                    (uint32_t)objectIds.size(),
                    objectIds.data(),
                    mode,
                    statuses.data());
            break;

        default:
            status = OTAI_STATUS_NOT_SUPPORTED;
            SWSS_LOG_ERROR("api %s is not supported in bulk mode", otai_serialize_common_api(api).c_str());
            break;
    }

    sendBulkApiResponse(api, status, (uint32_t)objectIds.size(), objectIds.data(), statuses.data());

    return status;
}

otai_status_t ServerOtai::processBulkEntry(
        _In_ otai_object_type_t objectType,
        _In_ const std::vector<std::string>& objectIds,
        _In_ otai_common_api_t api,
        _In_ const std::vector<std::shared_ptr<OtaiAttributeList>>& attributes,
        _In_ const std::vector<std::vector<swss::FieldValueTuple>>& strAttributes)
{
    SWSS_LOG_ENTER();

    auto info = otai_metadata_get_object_type_info(objectType);

    if (info->isobjectid)
    {
        SWSS_LOG_THROW("passing oid object to bulk non object id operation");
    }

    std::vector<otai_status_t> statuses(objectIds.size());

    otai_status_t status = OTAI_STATUS_SUCCESS;

    switch (api)
    {
        case OTAI_COMMON_API_BULK_CREATE:
            status = processBulkCreateEntry(objectType, objectIds, attributes, statuses);
            break;

        case OTAI_COMMON_API_BULK_REMOVE:
            status = processBulkRemoveEntry(objectType, objectIds, statuses);
            break;

        case OTAI_COMMON_API_BULK_SET:
            status = processBulkSetEntry(objectType, objectIds, attributes, statuses);
            break;

        default:
            SWSS_LOG_ERROR("api %s is not supported in bulk", otai_serialize_common_api(api).c_str());
            status = OTAI_STATUS_NOT_SUPPORTED;
    }

    std::vector<otai_object_id_t> oids(objectIds.size());

    sendBulkApiResponse(api, status, (uint32_t)objectIds.size(), oids.data(), statuses.data());

    return status;
}

otai_status_t ServerOtai::processBulkCreateEntry(
        _In_ otai_object_type_t objectType,
        _In_ const std::vector<std::string>& objectIds,
        _In_ const std::vector<std::shared_ptr<OtaiAttributeList>>& attributes,
        _Out_ std::vector<otai_status_t>& statuses)
{
    SWSS_LOG_ENTER();

    otai_status_t status = OTAI_STATUS_SUCCESS;

    switch ((int)objectType)
    {

        default:
            return OTAI_STATUS_NOT_SUPPORTED;
    }

    return status;
}

otai_status_t ServerOtai::processBulkRemoveEntry(
        _In_ otai_object_type_t objectType,
        _In_ const std::vector<std::string>& objectIds,
        _Out_ std::vector<otai_status_t>& statuses)
{
    SWSS_LOG_ENTER();

    otai_status_t status = OTAI_STATUS_SUCCESS;

    switch ((int)objectType)
    {
        default:
            return OTAI_STATUS_NOT_SUPPORTED;
    }

    return status;
}

otai_status_t ServerOtai::processBulkSetEntry(
        _In_ otai_object_type_t objectType,
        _In_ const std::vector<std::string>& objectIds,
        _In_ const std::vector<std::shared_ptr<OtaiAttributeList>>& attributes,
        _Out_ std::vector<otai_status_t>& statuses)
{
    SWSS_LOG_ENTER();

    otai_status_t status = OTAI_STATUS_SUCCESS;

    std::vector<otai_attribute_t> attr_lists;

    uint32_t object_count = (uint32_t) objectIds.size();

    for (uint32_t it = 0; it < object_count; it++)
    {
        attr_lists.push_back(attributes[it]->get_attr_list()[0]);
    }

    switch (objectType)
    {

        default:
            return OTAI_STATUS_NOT_SUPPORTED;
    }

    return status;
}

void ServerOtai::sendBulkApiResponse(
        _In_ otai_common_api_t api,
        _In_ otai_status_t status,
        _In_ uint32_t object_count,
        _In_ const otai_object_id_t* object_ids,
        _In_ const otai_status_t* statuses)
{
    SWSS_LOG_ENTER();

    switch (api)
    {
        case OTAI_COMMON_API_BULK_CREATE:
        case OTAI_COMMON_API_BULK_REMOVE:
        case OTAI_COMMON_API_BULK_SET:
            break;

        default:
            SWSS_LOG_THROW("api %s not supported by this function",
                    otai_serialize_common_api(api).c_str());
    }

    std::vector<swss::FieldValueTuple> entry;

    for (uint32_t idx = 0; idx < object_count; idx++)
    {
        entry.emplace_back(otai_serialize_status(statuses[idx]), "");
    }

    if (api == OTAI_COMMON_API_BULK_CREATE)
    {
        // in case of bulk create api, we need to return oids values that was
        // created to the client

        for (uint32_t idx = 0; idx < object_count; idx++)
        {
            entry.emplace_back("oid", otai_serialize_object_id(object_ids[idx]));
        }
    }

    std::string strStatus = otai_serialize_status(status);

    SWSS_LOG_INFO("sending response for %s api with status: %s",
            otai_serialize_common_api(api).c_str(),
            strStatus.c_str());

    m_selectableChannel->set(strStatus, entry, REDIS_ASIC_STATE_COMMAND_GETRESPONSE);
}

otai_status_t ServerOtai::processAttrCapabilityQuery(
        _In_ const swss::KeyOpFieldsValuesTuple &kco)
{
    SWSS_LOG_ENTER();

        return OTAI_STATUS_INVALID_PARAMETER;
}

otai_status_t ServerOtai::processAttrEnumValuesCapabilityQuery(
        _In_ const swss::KeyOpFieldsValuesTuple &kco)
{
    SWSS_LOG_ENTER();

    auto& strSwitchOid = kfvKey(kco);

    otai_object_id_t switchOid;
    otai_deserialize_object_id(strSwitchOid, switchOid);

    auto& values = kfvFieldsValues(kco);

    if (values.size() != 3)
    {
        SWSS_LOG_ERROR("Invalid input: expected 3 arguments, received %zu", values.size());

        m_selectableChannel->set(otai_serialize_status(OTAI_STATUS_INVALID_PARAMETER), {}, REDIS_ASIC_STATE_COMMAND_ATTR_ENUM_VALUES_CAPABILITY_RESPONSE);

        return OTAI_STATUS_INVALID_PARAMETER;
    }

    otai_object_type_t objectType;
    otai_deserialize_object_type(fvValue(values[0]), objectType);

    otai_attr_id_t attrId;
    otai_deserialize_attr_id(fvValue(values[1]), attrId);

    uint32_t list_size = std::stoi(fvValue(values[2]));

    std::vector<int32_t> enum_capabilities_list(list_size);

    otai_s32_list_t enumCapList;

    enumCapList.count = list_size;
    enumCapList.list = enum_capabilities_list.data();

    otai_status_t status = m_otai->queryAttributeEnumValuesCapability(switchOid, objectType, attrId, &enumCapList);

    std::vector<swss::FieldValueTuple> entry;

    if (status == OTAI_STATUS_SUCCESS)
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

    m_selectableChannel->set(otai_serialize_status(status), entry, REDIS_ASIC_STATE_COMMAND_ATTR_ENUM_VALUES_CAPABILITY_RESPONSE);

    return status;
}

otai_status_t ServerOtai::processObjectTypeGetAvailabilityQuery(
    _In_ const swss::KeyOpFieldsValuesTuple &kco)
{
    SWSS_LOG_ENTER();

    auto& strSwitchOid = kfvKey(kco);

    otai_object_id_t switchOid;
    otai_deserialize_object_id(strSwitchOid, switchOid);

    std::vector<swss::FieldValueTuple> values = kfvFieldsValues(kco);

    // needs to pop the object type off the end of the list in order to
    // retrieve the attribute list

    otai_object_type_t objectType;
    otai_deserialize_object_type(fvValue(values.back()), objectType);

    values.pop_back();

    OtaiAttributeList list(objectType, values, false);

    otai_attribute_t *attr_list = list.get_attr_list();

    uint32_t attr_count = list.get_attr_count();

    uint64_t count;

    otai_status_t status = m_otai->objectTypeGetAvailability(
            switchOid,
            objectType,
            attr_count,
            attr_list,
            &count);

    std::vector<swss::FieldValueTuple> entry;

    if (status == OTAI_STATUS_SUCCESS)
    {
        entry.push_back(swss::FieldValueTuple("OBJECT_COUNT", std::to_string(count)));

        SWSS_LOG_DEBUG("Sending response: count = %lu", count);
    }

    m_selectableChannel->set(otai_serialize_status(status), entry, REDIS_ASIC_STATE_COMMAND_OBJECT_TYPE_GET_AVAILABILITY_RESPONSE);

    return status;
}

otai_status_t ServerOtai::processFdbFlush(
        _In_ const swss::KeyOpFieldsValuesTuple &kco)
{
    SWSS_LOG_ENTER();

    return OTAI_STATUS_FAILURE;
}

otai_status_t ServerOtai::processClearStatsEvent(
        _In_ const swss::KeyOpFieldsValuesTuple &kco)
{
    SWSS_LOG_ENTER();

    const std::string &key = kfvKey(kco);

    otai_object_meta_key_t metaKey;
    otai_deserialize_object_meta_key(key, metaKey);

    auto info = otai_metadata_get_object_type_info(metaKey.objecttype);

    if (info->isnonobjectid)
    {
        SWSS_LOG_THROW("non object id not supported on clear stats: %s, FIXME", key.c_str());
    }

    std::vector<otai_stat_id_t> counter_ids;

    for (auto&v: kfvFieldsValues(kco))
    {
        int32_t val;
        otai_deserialize_enum(fvField(v), info->statenum, val);

        counter_ids.push_back(val);
    }

    auto status = m_otai->clearStats(
            metaKey.objecttype,
            metaKey.objectkey.key.object_id,
            (uint32_t)counter_ids.size(),
            counter_ids.data());

    m_selectableChannel->set(otai_serialize_status(status), {}, REDIS_ASIC_STATE_COMMAND_GETRESPONSE);

    return status;
}

otai_status_t ServerOtai::processGetStatsEvent(
        _In_ const swss::KeyOpFieldsValuesTuple &kco)
{
    SWSS_LOG_ENTER();

    const std::string &key = kfvKey(kco);

    otai_object_meta_key_t metaKey;
    otai_deserialize_object_meta_key(key, metaKey);

    // TODO get stats on created object in init view mode could fail

    auto info = otai_metadata_get_object_type_info(metaKey.objecttype);

    if (info->isnonobjectid)
    {
        SWSS_LOG_THROW("non object id not supported on clear stats: %s, FIXME", key.c_str());
    }

    std::vector<otai_stat_id_t> counter_ids;

    for (auto&v: kfvFieldsValues(kco))
    {
        int32_t val;
        otai_deserialize_enum(fvField(v), info->statenum, val);

        counter_ids.push_back(val);
    }

    std::vector<uint64_t> result(counter_ids.size());

    auto status = m_otai->getStats(
            metaKey.objecttype,
            metaKey.objectkey.key.object_id,
            (uint32_t)counter_ids.size(),
            counter_ids.data(),
            result.data());

    std::vector<swss::FieldValueTuple> entry;

    if (status != OTAI_STATUS_SUCCESS)
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

    m_selectableChannel->set(otai_serialize_status(status), entry, REDIS_ASIC_STATE_COMMAND_GETRESPONSE);

    return status;
}
