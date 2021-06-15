#include "ServerSai.h"
#include "SaiInternal.h"
#include "Sai.h"
#include "sairediscommon.h"

#include "syncd/ZeroMQSelectableChannel.h"

#include "meta/sai_serialize.h"
#include "meta/SaiAttributeList.h"

#include "swss/logger.h"
#include "swss/select.h"

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
        uninitialize();
    }
}

// INITIALIZE UNINITIALIZE

sai_status_t ServerSai::initialize(
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

    auto status = m_sai->initialize(flags, service_method_table);

    SWSS_LOG_NOTICE("init client/server sai: %s", sai_serialize_status(status).c_str());

    if (status == SAI_STATUS_SUCCESS)
    {
        // TODO from config
        m_selectableChannel = std::make_shared<syncd::ZeroMQSelectableChannel>("ipc:///tmp/saiServer");

        SWSS_LOG_NOTICE("starting server thread");

        m_runServerThread = true;

        m_serverThread = std::make_shared<std::thread>(&ServerSai::serverThreadFunction, this);

        m_apiInitialized = true;
    }

    return status;
}

sai_status_t ServerSai::uninitialize(void)
{
    SWSS_LOG_ENTER();
    REDIS_CHECK_API_INITIALIZED();

    SWSS_LOG_NOTICE("begin");

    m_apiInitialized = false;

    if (m_serverThread)
    {
        SWSS_LOG_NOTICE("end server thread begin");

        m_runServerThread = true;

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

DECLARE_CREATE_ENTRY(FDB_ENTRY,fdb_entry);
DECLARE_CREATE_ENTRY(INSEG_ENTRY,inseg_entry);
DECLARE_CREATE_ENTRY(IPMC_ENTRY,ipmc_entry);
DECLARE_CREATE_ENTRY(L2MC_ENTRY,l2mc_entry);
DECLARE_CREATE_ENTRY(MCAST_FDB_ENTRY,mcast_fdb_entry);
DECLARE_CREATE_ENTRY(NEIGHBOR_ENTRY,neighbor_entry);
DECLARE_CREATE_ENTRY(ROUTE_ENTRY,route_entry);
DECLARE_CREATE_ENTRY(NAT_ENTRY,nat_entry);


#define DECLARE_REMOVE_ENTRY(OT,ot)                         \
sai_status_t ServerSai::remove(                             \
        _In_ const sai_ ## ot ## _t* entry)                 \
{                                                           \
    MUTEX();                                                \
    SWSS_LOG_ENTER();                                       \
    REDIS_CHECK_API_INITIALIZED();                          \
    return m_sai->remove(entry);                            \
}

DECLARE_REMOVE_ENTRY(FDB_ENTRY,fdb_entry);
DECLARE_REMOVE_ENTRY(INSEG_ENTRY,inseg_entry);
DECLARE_REMOVE_ENTRY(IPMC_ENTRY,ipmc_entry);
DECLARE_REMOVE_ENTRY(L2MC_ENTRY,l2mc_entry);
DECLARE_REMOVE_ENTRY(MCAST_FDB_ENTRY,mcast_fdb_entry);
DECLARE_REMOVE_ENTRY(NEIGHBOR_ENTRY,neighbor_entry);
DECLARE_REMOVE_ENTRY(ROUTE_ENTRY,route_entry);
DECLARE_REMOVE_ENTRY(NAT_ENTRY,nat_entry);

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

DECLARE_SET_ENTRY(FDB_ENTRY,fdb_entry);
DECLARE_SET_ENTRY(INSEG_ENTRY,inseg_entry);
DECLARE_SET_ENTRY(IPMC_ENTRY,ipmc_entry);
DECLARE_SET_ENTRY(L2MC_ENTRY,l2mc_entry);
DECLARE_SET_ENTRY(MCAST_FDB_ENTRY,mcast_fdb_entry);
DECLARE_SET_ENTRY(NEIGHBOR_ENTRY,neighbor_entry);
DECLARE_SET_ENTRY(ROUTE_ENTRY,route_entry);
DECLARE_SET_ENTRY(NAT_ENTRY,nat_entry);

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

DECLARE_GET_ENTRY(FDB_ENTRY,fdb_entry);
DECLARE_GET_ENTRY(INSEG_ENTRY,inseg_entry);
DECLARE_GET_ENTRY(IPMC_ENTRY,ipmc_entry);
DECLARE_GET_ENTRY(L2MC_ENTRY,l2mc_entry);
DECLARE_GET_ENTRY(MCAST_FDB_ENTRY,mcast_fdb_entry);
DECLARE_GET_ENTRY(NEIGHBOR_ENTRY,neighbor_entry);
DECLARE_GET_ENTRY(ROUTE_ENTRY,route_entry);
DECLARE_GET_ENTRY(NAT_ENTRY,nat_entry);

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

DECLARE_BULK_CREATE_ENTRY(ROUTE_ENTRY,route_entry)
DECLARE_BULK_CREATE_ENTRY(FDB_ENTRY,fdb_entry);
DECLARE_BULK_CREATE_ENTRY(INSEG_ENTRY,inseg_entry);
DECLARE_BULK_CREATE_ENTRY(NAT_ENTRY,nat_entry)


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

DECLARE_BULK_REMOVE_ENTRY(ROUTE_ENTRY,route_entry)
DECLARE_BULK_REMOVE_ENTRY(FDB_ENTRY,fdb_entry);
DECLARE_BULK_REMOVE_ENTRY(INSEG_ENTRY,inseg_entry);
DECLARE_BULK_REMOVE_ENTRY(NAT_ENTRY,nat_entry)

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

DECLARE_BULK_SET_ENTRY(ROUTE_ENTRY,route_entry);
DECLARE_BULK_SET_ENTRY(FDB_ENTRY,fdb_entry);
DECLARE_BULK_SET_ENTRY(INSEG_ENTRY,inseg_entry);
DECLARE_BULK_SET_ENTRY(NAT_ENTRY,nat_entry);

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

sai_status_t ServerSai::queryAattributeEnumValuesCapability(
        _In_ sai_object_id_t switch_id,
        _In_ sai_object_type_t object_type,
        _In_ sai_attr_id_t attr_id,
        _Inout_ sai_s32_list_t *enum_values_capability)
{
    MUTEX();
    SWSS_LOG_ENTER();
    REDIS_CHECK_API_INITIALIZED();

    return m_sai->queryAattributeEnumValuesCapability(
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
        _In_ syncd::SelectableChannel& consumer)
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

// TODO implement
//    if (op == REDIS_ASIC_STATE_COMMAND_BULK_CREATE)
//        return processBulkQuadEvent(SAI_COMMON_API_BULK_CREATE, kco);
//
//    if (op == REDIS_ASIC_STATE_COMMAND_BULK_REMOVE)
//        return processBulkQuadEvent(SAI_COMMON_API_BULK_REMOVE, kco);
//
//    if (op == REDIS_ASIC_STATE_COMMAND_BULK_SET)
//        return processBulkQuadEvent(SAI_COMMON_API_BULK_SET, kco);
//
//    if (op == REDIS_ASIC_STATE_COMMAND_NOTIFY)
//        return processNotifySyncd(kco);
//
//    if (op == REDIS_ASIC_STATE_COMMAND_GET_STATS)
//        return processGetStatsEvent(kco);
//
//    if (op == REDIS_ASIC_STATE_COMMAND_CLEAR_STATS)
//        return processClearStatsEvent(kco);
//
//    if (op == REDIS_ASIC_STATE_COMMAND_FLUSH)
//        return processFdbFlush(kco);
//
//    if (op == REDIS_ASIC_STATE_COMMAND_ATTR_CAPABILITY_QUERY)
//        return processAttrCapabilityQuery(kco);
//
//    if (op == REDIS_ASIC_STATE_COMMAND_ATTR_ENUM_VALUES_CAPABILITY_QUERY)
//        return processAttrEnumValuesCapabilityQuery(kco);
//
//    if (op == REDIS_ASIC_STATE_COMMAND_OBJECT_TYPE_GET_AVAILABILITY_QUERY)
//        return processObjectTypeGetAvailabilityQuery(kco);

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
