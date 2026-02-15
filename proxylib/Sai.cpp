#include "Sai.h"
#include "Utils.h"
#include "SaiInternal.h"
#include "ZeroMQChannel.h"
#include "SaiAttributeList.h"
#include "NotificationFactory.h"

#include "meta/Meta.h"
#include "meta/sai_serialize.h"

using namespace saiproxy;
using namespace std::placeholders;

#define PROXY_CHECK_API_INITIALIZED()                                       \
    if (!m_apiInitialized) {                                                \
        SWSS_LOG_ERROR("%s: api not initialized", __PRETTY_FUNCTION__);     \
        return SAI_STATUS_FAILURE; }

#define PROXY_CHECK_POINTER(pointer)                                        \
    if ((pointer) == nullptr) {                                             \
        SWSS_LOG_ERROR("entry pointer " # pointer " is null");              \
        return SAI_STATUS_INVALID_PARAMETER; }

Sai::Sai()
{
    SWSS_LOG_ENTER();

    m_apiInitialized = false;
}

Sai::~Sai()
{
    SWSS_LOG_ENTER();

    if (m_apiInitialized)
    {
        apiUninitialize();
    }
}

// INITIALIZE UNINITIALIZE

sai_status_t Sai::apiInitialize(
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

    memset(&m_sn, 0, sizeof(m_sn));

    m_options = std::make_shared<Options>(); // load default options

    // TODO options should be obtained from service method table

    m_communicationChannel = std::make_shared<sairedis::ZeroMQChannel>(
            m_options->m_zmqChannel,
            m_options->m_zmqNtfChannel,
            std::bind(&Sai::handleNotification, this, _1, _2, _3));

    m_apiInitialized = true;

    return SAI_STATUS_SUCCESS;
}

sai_status_t Sai::apiUninitialize(void)
{
    SWSS_LOG_ENTER();
    PROXY_CHECK_API_INITIALIZED();

    SWSS_LOG_NOTICE("begin");

    m_communicationChannel = nullptr; // will stop the thread

    m_apiInitialized = false;

    SWSS_LOG_NOTICE("end");

    return SAI_STATUS_SUCCESS;
}

// QUAD OID

sai_status_t Sai::create(
        _In_ sai_object_type_t objectType,
        _Out_ sai_object_id_t* objectId,
        _In_ sai_object_id_t switchId,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    MUTEX();
    SWSS_LOG_ENTER();
    PROXY_CHECK_API_INITIALIZED();

    auto entry = saimeta::SaiAttributeList::serialize_attr_list(objectType, attr_count, attr_list, false);

    entry.emplace_back("SWITCH_ID", sai_serialize_object_id(switchId)); // last entry is switch_id

    auto serializedObjectType = sai_serialize_object_type(objectType);

    auto serializedObjectId = sai_serialize_object_id(SAI_NULL_OBJECT_ID);

    std::string key = serializedObjectType + ":" + serializedObjectId;

    m_communicationChannel->set(key, entry, "create");

    swss::KeyOpFieldsValuesTuple kco;

    auto status = m_communicationChannel->wait("create_response", kco);

    if (status == SAI_STATUS_SUCCESS)
    {
        if (objectType == SAI_OBJECT_TYPE_SWITCH)
        {
            updateNotifications(attr_count, attr_list); // TODO should be per switch
        }

        auto& values = kfvFieldsValues(kco);

        if (values.size() == 0)
        {
            SWSS_LOG_THROW("logic error, api returned 0 values!");
        }

        SWSS_LOG_NOTICE("deserialize new object id: %s", fvValue(values[0]).c_str());

        sai_deserialize_object_id(fvValue(values[0]), *objectId);
    }

    return status;
}

sai_status_t Sai::remove(
        _In_ sai_object_type_t objectType,
        _In_ sai_object_id_t objectId)
{
    MUTEX();
    SWSS_LOG_ENTER();
    PROXY_CHECK_API_INITIALIZED();

    return remove(objectType, sai_serialize_object_id(objectId));
}

sai_status_t Sai::set(
        _In_ sai_object_type_t objectType,
        _In_ sai_object_id_t objectId,
        _In_ const sai_attribute_t *attr)
{
    MUTEX();
    SWSS_LOG_ENTER();
    PROXY_CHECK_API_INITIALIZED();

    return set(objectType, sai_serialize_object_id(objectId), attr);
}

sai_status_t Sai::get(
        _In_ sai_object_type_t objectType,
        _In_ sai_object_id_t objectId,
        _In_ uint32_t attr_count,
        _Inout_ sai_attribute_t *attr_list)
{
    MUTEX();
    SWSS_LOG_ENTER();
    PROXY_CHECK_API_INITIALIZED();

    return get(objectType, sai_serialize_object_id(objectId), attr_count, attr_list);
}

// QUAD ENTRY

#define DECLARE_CREATE_ENTRY(OT,ot)                         \
sai_status_t Sai::create(                                   \
        _In_ const sai_ ## ot ## _t* entry,                 \
        _In_ uint32_t attr_count,                           \
        _In_ const sai_attribute_t *attr_list)              \
{                                                           \
    MUTEX();                                                \
    SWSS_LOG_ENTER();                                       \
    PROXY_CHECK_API_INITIALIZED();                          \
    PROXY_CHECK_POINTER(entry)                              \
    return create(                                          \
            (sai_object_type_t)SAI_OBJECT_TYPE_ ## OT,      \
            sai_serialize_ ## ot(*entry),                   \
            attr_count,                                     \
            attr_list);                                     \
}

SAIREDIS_DECLARE_EVERY_ENTRY(DECLARE_CREATE_ENTRY);

#define DECLARE_REMOVE_ENTRY(OT,ot)                         \
sai_status_t Sai::remove(                                   \
        _In_ const sai_ ## ot ## _t* entry)                 \
{                                                           \
    MUTEX();                                                \
    SWSS_LOG_ENTER();                                       \
    PROXY_CHECK_API_INITIALIZED();                          \
    PROXY_CHECK_POINTER(entry)                              \
    return remove(                                          \
            (sai_object_type_t)SAI_OBJECT_TYPE_ ## OT,      \
            sai_serialize_ ## ot(*entry));                  \
}

SAIREDIS_DECLARE_EVERY_ENTRY(DECLARE_REMOVE_ENTRY);

#define DECLARE_SET_ENTRY(OT,ot)                            \
sai_status_t Sai::set(                                      \
        _In_ const sai_ ## ot ## _t* entry,                 \
        _In_ const sai_attribute_t *attr)                   \
{                                                           \
    MUTEX();                                                \
    SWSS_LOG_ENTER();                                       \
    PROXY_CHECK_API_INITIALIZED();                          \
    PROXY_CHECK_POINTER(entry)                              \
    return set(                                             \
            (sai_object_type_t)SAI_OBJECT_TYPE_ ## OT,      \
            sai_serialize_ ## ot(*entry),                   \
            attr);                                          \
}

SAIREDIS_DECLARE_EVERY_ENTRY(DECLARE_SET_ENTRY);

#define DECLARE_GET_ENTRY(OT,ot)                            \
sai_status_t Sai::get(                                      \
        _In_ const sai_ ## ot ## _t* entry,                 \
        _In_ uint32_t attr_count,                           \
        _Inout_ sai_attribute_t *attr_list)                 \
{                                                           \
    MUTEX();                                                \
    SWSS_LOG_ENTER();                                       \
    PROXY_CHECK_API_INITIALIZED();                          \
    PROXY_CHECK_POINTER(entry)                              \
    return get(                                             \
            (sai_object_type_t)SAI_OBJECT_TYPE_ ## OT,      \
            sai_serialize_ ## ot(*entry),                   \
            attr_count,                                     \
            attr_list);                                     \
}

SAIREDIS_DECLARE_EVERY_ENTRY(DECLARE_GET_ENTRY);

sai_status_t Sai::create(
        _In_ sai_object_type_t objectType,
        _In_ const std::string& entry,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    auto vals = saimeta::SaiAttributeList::serialize_attr_list(objectType, attr_count, attr_list, false);

    auto serializedObjectType = sai_serialize_object_type(objectType);

    std::string key = serializedObjectType + ":" + entry;

    m_communicationChannel->set(key, vals, "create_entry");

    swss::KeyOpFieldsValuesTuple kco;

    return m_communicationChannel->wait("create_entry_response", kco);
}

sai_status_t Sai::remove(
        _In_ sai_object_type_t objectType,
        _In_ const std::string& entry)
{
    SWSS_LOG_ENTER();

    auto serializedObjectType = sai_serialize_object_type(objectType);

    std::string key = serializedObjectType + ":" + entry;

    m_communicationChannel->set(key, {}, "remove");

    swss::KeyOpFieldsValuesTuple kco;

    return m_communicationChannel->wait("remove_response", kco);
}

sai_status_t Sai::set(
        _In_ sai_object_type_t objectType,
        _In_ const std::string& entry,
        _In_ const sai_attribute_t *attr)
{
    SWSS_LOG_ENTER();

    auto val = saimeta::SaiAttributeList::serialize_attr_list(objectType, 1, attr, false);

    auto serializedObjectType = sai_serialize_object_type(objectType);

    std::string key = serializedObjectType + ":" + entry;

    m_communicationChannel->set(key, val, "set");

    swss::KeyOpFieldsValuesTuple kco;

    auto status = m_communicationChannel->wait("set_response", kco);

    if (objectType == SAI_OBJECT_TYPE_SWITCH && status == SAI_STATUS_SUCCESS)
    {
        updateNotifications(1, attr);
    }

    return status;
}

sai_status_t Sai::get(
        _In_ sai_object_type_t objectType,
        _In_ const std::string& entry,
        _In_ uint32_t attr_count,
        _Inout_ sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    /*
     * Since user may reuse buffers, then oid list buffers maybe not cleared
     * and contain some garbage, let's clean them so we send all oids as null to
     * syncd.
     */

    sairedis::Utils::clearOidValues(objectType, attr_count, attr_list);

    auto vals = saimeta::SaiAttributeList::serialize_attr_list(objectType, attr_count, attr_list, false);

    std::string serializedObjectType = sai_serialize_object_type(objectType);

    std::string key = serializedObjectType + ":" + entry;

    m_communicationChannel->set(key, vals, "get");

    swss::KeyOpFieldsValuesTuple kco;

    auto status = m_communicationChannel->wait("get_response", kco);

    auto &values = kfvFieldsValues(kco);

    if (status == SAI_STATUS_SUCCESS)
    {
        if (values.size() == 0)
        {
            SWSS_LOG_THROW("logic error, api returned 0 values!");
        }

        saimeta::SaiAttributeList list(objectType, values, false);

        transfer_attributes(objectType, attr_count, list.get_attr_list(), attr_list, false);
    }
    else if (status == SAI_STATUS_BUFFER_OVERFLOW)
    {
        if (values.size() == 0)
        {
            SWSS_LOG_THROW("logic error, api returned 0 values!");
        }

        saimeta::SaiAttributeList list(objectType, values, true);

        transfer_attributes(objectType, attr_count, list.get_attr_list(), attr_list, true);
    }

    return status;
}

// STATS

sai_status_t Sai::getStats(
        _In_ sai_object_type_t object_type,
        _In_ sai_object_id_t object_id,
        _In_ uint32_t number_of_counters,
        _In_ const sai_stat_id_t *counter_ids,
        _Out_ uint64_t *counters)
{
    MUTEX();
    SWSS_LOG_ENTER();
    PROXY_CHECK_API_INITIALIZED();

    auto oi = sai_metadata_get_object_type_info(object_type);

    if (oi == NULL)
    {
        SWSS_LOG_ERROR("invalid object type: %s", sai_serialize_object_type(object_type).c_str());

        return SAI_STATUS_INVALID_PARAMETER;
    }

    auto strObjectType = sai_serialize_object_type(object_type);
    auto strObjectId = sai_serialize_object_id(object_id);

    std::string key = strObjectType + ":" + strObjectId;

    std::vector<swss::FieldValueTuple> entry;

    for (uint32_t i = 0; i < number_of_counters; i++)
    {
        entry.emplace_back(sai_serialize_enum(counter_ids[i], oi->statenum), "");
    }

    m_communicationChannel->set(key, entry, "get_stats");

    swss::KeyOpFieldsValuesTuple kco;

    auto status = m_communicationChannel->wait("get_stats_response", kco);

    if (status == SAI_STATUS_SUCCESS)
    {
        auto &values = kfvFieldsValues(kco);

        if (values.size () != number_of_counters)
        {
            SWSS_LOG_THROW("logic error, wrong number of counters, got %zu, expected %u", values.size(), number_of_counters);
        }

        for (uint32_t idx = 0; idx < number_of_counters; idx++)
        {
            counters[idx] = stoull(fvValue(values[idx]));
        }
    }

    return status;
}

sai_status_t Sai::getStatsExt(
        _In_ sai_object_type_t object_type,
        _In_ sai_object_id_t object_id,
        _In_ uint32_t number_of_counters,
        _In_ const sai_stat_id_t *counter_ids,
        _In_ sai_stats_mode_t mode,
        _Out_ uint64_t *counters)
{
    MUTEX();
    SWSS_LOG_ENTER();
    PROXY_CHECK_API_INITIALIZED();

    auto oi = sai_metadata_get_object_type_info(object_type);

    if (oi == NULL)
    {
        SWSS_LOG_ERROR("invalid object type: %s", sai_serialize_object_type(object_type).c_str());

        return SAI_STATUS_INVALID_PARAMETER;
    }

    auto strObjectType = sai_serialize_object_type(object_type);
    auto strObjectId = sai_serialize_object_id(object_id);

    std::string key = strObjectType + ":" + strObjectId;

    std::vector<swss::FieldValueTuple> entry;

    for (uint32_t i = 0; i < number_of_counters; i++)
    {
        entry.emplace_back(sai_serialize_enum(counter_ids[i], oi->statenum), "");
    }

    entry.emplace_back("STATS_MODE", std::to_string(mode)); // TODO add serialize

    m_communicationChannel->set(key, entry, "get_stats_ext");

    swss::KeyOpFieldsValuesTuple kco;

    auto status = m_communicationChannel->wait("get_stats_ext_response", kco);

    if (status == SAI_STATUS_SUCCESS)
    {
        auto &values = kfvFieldsValues(kco);

        if (values.size () != number_of_counters)
        {
            SWSS_LOG_THROW("logic error, wrong number of counters, got %zu, expected %u", values.size(), number_of_counters);
        }

        for (uint32_t idx = 0; idx < number_of_counters; idx++)
        {
            counters[idx] = stoull(fvValue(values[idx]));
        }
    }

    return status;
}

sai_status_t Sai::clearStats(
        _In_ sai_object_type_t object_type,
        _In_ sai_object_id_t object_id,
        _In_ uint32_t number_of_counters,
        _In_ const sai_stat_id_t *counter_ids)
{
    MUTEX();
    SWSS_LOG_ENTER();
    PROXY_CHECK_API_INITIALIZED();

    auto oi = sai_metadata_get_object_type_info(object_type);

    if (oi == NULL)
    {
        SWSS_LOG_ERROR("invalid object type: %s", sai_serialize_object_type(object_type).c_str());

        return SAI_STATUS_INVALID_PARAMETER;
    }

    auto strObjectType = sai_serialize_object_type(object_type);
    auto strObjectId = sai_serialize_object_id(object_id);

    std::string key = strObjectType + ":" + strObjectId;

    std::vector<swss::FieldValueTuple> entry;

    for (uint32_t i = 0; i < number_of_counters; i++)
    {
        entry.emplace_back(sai_serialize_enum(counter_ids[i], oi->statenum), "");
    }

    m_communicationChannel->set(key, entry, "clear_stats");

    swss::KeyOpFieldsValuesTuple kco;

    return m_communicationChannel->wait("clear_stats_response", kco);
}

sai_status_t Sai::queryStatsCapability(
        _In_ sai_object_id_t switchId,
        _In_ sai_object_type_t objectType,
        _Inout_ sai_stat_capability_list_t *stats_capability)
{
    SWSS_LOG_ENTER();

    SWSS_LOG_ERROR("not implemented, FIXME");

    return SAI_STATUS_NOT_IMPLEMENTED;
}

sai_status_t Sai::queryStatsStCapability(
    _In_ sai_object_id_t switchId,
    _In_ sai_object_type_t objectType,
    _Inout_ sai_stat_st_capability_list_t *stats_capability)
{
    SWSS_LOG_ENTER();

    SWSS_LOG_ERROR("not implemented, FIXME");

    return SAI_STATUS_NOT_IMPLEMENTED;
}

// BULK STATS

sai_status_t Sai::bulkGetStats(
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

    SWSS_LOG_ERROR("not implemented, FIXME");

    return SAI_STATUS_NOT_IMPLEMENTED;
}

sai_status_t Sai::bulkClearStats(
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

    SWSS_LOG_ERROR("not implemented, FIXME");

    return SAI_STATUS_NOT_IMPLEMENTED;
}

// BULK QUAD OID

sai_status_t Sai::bulkCreate(
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
    PROXY_CHECK_API_INITIALIZED();
    PROXY_CHECK_POINTER(object_id);
    PROXY_CHECK_POINTER(object_statuses);

    SWSS_LOG_ERROR("not implemented, FIXME");

    return SAI_STATUS_NOT_IMPLEMENTED;
}

sai_status_t Sai::bulkRemove(
        _In_ sai_object_type_t object_type,
        _In_ uint32_t object_count,
        _In_ const sai_object_id_t *object_id,
        _In_ sai_bulk_op_error_mode_t mode,
        _Out_ sai_status_t *object_statuses)
{
    MUTEX();
    SWSS_LOG_ENTER();
    PROXY_CHECK_API_INITIALIZED();
    PROXY_CHECK_POINTER(object_id);
    PROXY_CHECK_POINTER(object_statuses);

    SWSS_LOG_ERROR("not implemented, FIXME");

    return SAI_STATUS_NOT_IMPLEMENTED;
}

sai_status_t Sai::bulkSet(
        _In_ sai_object_type_t object_type,
        _In_ uint32_t object_count,
        _In_ const sai_object_id_t *object_id,
        _In_ const sai_attribute_t *attr_list,
        _In_ sai_bulk_op_error_mode_t mode,
        _Out_ sai_status_t *object_statuses)
{
    MUTEX();
    SWSS_LOG_ENTER();
    PROXY_CHECK_API_INITIALIZED();
    PROXY_CHECK_POINTER(object_statuses);

    SWSS_LOG_ERROR("not implemented, FIXME");

    return SAI_STATUS_NOT_IMPLEMENTED;
}

sai_status_t Sai::bulkGet(
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

    SWSS_LOG_ERROR("not implemented, FIXME");

    return SAI_STATUS_NOT_IMPLEMENTED;
}

// BULK QUAD ENTRY

#define DECLARE_BULK_CREATE_ENTRY(OT,ot)                    \
sai_status_t Sai::bulkCreate(                               \
        _In_ uint32_t object_count,                         \
        _In_ const sai_ ## ot ## _t* entries,               \
        _In_ const uint32_t *attr_count,                    \
        _In_ const sai_attribute_t **attr_list,             \
        _In_ sai_bulk_op_error_mode_t mode,                 \
        _Out_ sai_status_t *object_statuses)                \
{                                                           \
    MUTEX();                                                \
    SWSS_LOG_ENTER();                                       \
    PROXY_CHECK_API_INITIALIZED();                          \
    PROXY_CHECK_POINTER(entries)                            \
    SWSS_LOG_ERROR("not implemented, FIXME");               \
    return SAI_STATUS_NOT_IMPLEMENTED;                      \
}

SAIREDIS_DECLARE_EVERY_BULK_ENTRY(DECLARE_BULK_CREATE_ENTRY);

// BULK REMOVE

#define DECLARE_BULK_REMOVE_ENTRY(OT,ot)                    \
sai_status_t Sai::bulkRemove(                               \
        _In_ uint32_t object_count,                         \
        _In_ const sai_ ## ot ## _t *entries,               \
        _In_ sai_bulk_op_error_mode_t mode,                 \
        _Out_ sai_status_t *object_statuses)                \
{                                                           \
    MUTEX();                                                \
    SWSS_LOG_ENTER();                                       \
    PROXY_CHECK_API_INITIALIZED();                          \
    PROXY_CHECK_POINTER(entries)                            \
    SWSS_LOG_ERROR("not implemented, FIXME");               \
    return SAI_STATUS_NOT_IMPLEMENTED;                      \
}

SAIREDIS_DECLARE_EVERY_BULK_ENTRY(DECLARE_BULK_REMOVE_ENTRY);

// BULK SET

#define DECLARE_BULK_SET_ENTRY(OT,ot)                       \
sai_status_t Sai::bulkSet(                                  \
        _In_ uint32_t object_count,                         \
        _In_ const sai_ ## ot ## _t *entries,               \
        _In_ const sai_attribute_t *attr_list,              \
        _In_ sai_bulk_op_error_mode_t mode,                 \
        _Out_ sai_status_t *object_statuses)                \
{                                                           \
    MUTEX();                                                \
    SWSS_LOG_ENTER();                                       \
    PROXY_CHECK_API_INITIALIZED();                          \
    PROXY_CHECK_POINTER(entries)                            \
    SWSS_LOG_ERROR("not implemented, FIXME");               \
    return SAI_STATUS_NOT_IMPLEMENTED;                      \
}

SAIREDIS_DECLARE_EVERY_BULK_ENTRY(DECLARE_BULK_SET_ENTRY);

// BULK GET

#define DECLARE_BULK_GET_ENTRY(OT,ot)                       \
sai_status_t Sai::bulkGet(                                  \
        _In_ uint32_t object_count,                         \
        _In_ const sai_ ## ot ## _t *ot,                    \
        _In_ const uint32_t *attr_count,                    \
        _Inout_ sai_attribute_t **attr_list,                \
        _In_ sai_bulk_op_error_mode_t mode,                 \
        _Out_ sai_status_t *object_statuses)                \
{                                                           \
    MUTEX();                                                \
    SWSS_LOG_ENTER();                                       \
    SWSS_LOG_ERROR("FIXME not implemented");                \
    return SAI_STATUS_NOT_IMPLEMENTED;                      \
}

SAIREDIS_DECLARE_EVERY_BULK_ENTRY(DECLARE_BULK_GET_ENTRY);

// NON QUAD API

sai_status_t Sai::flushFdbEntries(
        _In_ sai_object_id_t switch_id,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    MUTEX();
    SWSS_LOG_ENTER();
    PROXY_CHECK_API_INITIALIZED();

    auto entry = saimeta::SaiAttributeList::serialize_attr_list(
            SAI_OBJECT_TYPE_FDB_FLUSH,
            attr_count,
            attr_list,
            false);

    std::string serializedObjectId = sai_serialize_object_type(SAI_OBJECT_TYPE_FDB_FLUSH);

    // NOTE ! we actually give switch ID since FLUSH is not real object
    std::string key = serializedObjectId + ":" + sai_serialize_object_id(switch_id);

    SWSS_LOG_NOTICE("flush key: %s, fields: %lu", key.c_str(), entry.size());

    m_communicationChannel->set(key, entry, "flush_fdb_entries");

    swss::KeyOpFieldsValuesTuple kco;

    return m_communicationChannel->wait("flush_fdb_entries_response", kco);
}

// SAI API

sai_status_t Sai::objectTypeGetAvailability(
        _In_ sai_object_id_t switchId,
        _In_ sai_object_type_t objectType,
        _In_ uint32_t attrCount,
        _In_ const sai_attribute_t *attrList,
        _Out_ uint64_t *count)
{
    MUTEX();
    SWSS_LOG_ENTER();
    PROXY_CHECK_API_INITIALIZED();

    auto strSwitchId = sai_serialize_object_id(switchId);

    auto entry = saimeta::SaiAttributeList::serialize_attr_list(objectType, attrCount, attrList, false);

    entry.emplace_back("OBJECT_TYPE", sai_serialize_object_type(objectType));

    m_communicationChannel->set(strSwitchId, entry, "object_type_get_availability");

    swss::KeyOpFieldsValuesTuple kco;

    auto status = m_communicationChannel->wait("object_type_get_availability_response", kco);

    if (status == SAI_STATUS_SUCCESS)
    {
        auto& values = kfvFieldsValues(kco);

        if (values.size() == 0)
        {
            SWSS_LOG_THROW("logic error, api returned 0 values!");
        }

        *count = std::stoull(fvValue(values[0]));
    }

    return status;
}

sai_status_t Sai::queryAttributeCapability(
        _In_ sai_object_id_t switchId,
        _In_ sai_object_type_t objectType,
        _In_ sai_attr_id_t attrId,
        _Out_ sai_attr_capability_t *capability)
{
    MUTEX();
    SWSS_LOG_ENTER();
    PROXY_CHECK_API_INITIALIZED();

    auto strSwitchId = sai_serialize_object_id(switchId);
    auto strObjectType = sai_serialize_object_type(objectType);

    auto meta = sai_metadata_get_attr_metadata(objectType, attrId);

    if (meta == NULL)
    {
        SWSS_LOG_ERROR("Failed to find attribute metadata: object type %s, attr id %d", strObjectType.c_str(), attrId);

        return SAI_STATUS_INVALID_PARAMETER;
    }

    const std::string attrIdStr = meta->attridname;

    const std::vector<swss::FieldValueTuple> entry =
    {
        swss::FieldValueTuple("OBJECT_TYPE", strObjectType),
        swss::FieldValueTuple("ATTR_ID", attrIdStr)
    };

    m_communicationChannel->set(strSwitchId, entry, "query_attribute_capability");

    swss::KeyOpFieldsValuesTuple kco;

    auto status = m_communicationChannel->wait("query_attribute_capability_response", kco);

    if (status == SAI_STATUS_SUCCESS)
    {
        auto &values = kfvFieldsValues(kco);

        if (values.size() != 3)
        {
            SWSS_LOG_ERROR("logic error, api returned invalin numer in response: expected 3 values, received %zu", values.size());

            return SAI_STATUS_FAILURE;
        }

        capability->create_implemented = (fvValue(values[0]) == "true" ? true : false);
        capability->set_implemented    = (fvValue(values[1]) == "true" ? true : false);
        capability->get_implemented    = (fvValue(values[2]) == "true" ? true : false);
    }

    return status;
}

sai_status_t Sai::queryAttributeEnumValuesCapability(
        _In_ sai_object_id_t switchId,
        _In_ sai_object_type_t objectType,
        _In_ sai_attr_id_t attrId,
        _Inout_ sai_s32_list_t *enumValuesCapability)
{
    MUTEX();
    SWSS_LOG_ENTER();
    PROXY_CHECK_API_INITIALIZED();

    if (enumValuesCapability && enumValuesCapability->list)
    {
        // clear input list, since we use serialize to transfer values
        for (uint32_t idx = 0; idx < enumValuesCapability->count; idx++)
        {
            enumValuesCapability->list[idx] = 0;
        }
    }

    auto strSwitchId = sai_serialize_object_id(switchId);
    auto strObjectType = sai_serialize_object_type(objectType);

    auto meta = sai_metadata_get_attr_metadata(objectType, attrId);

    if (meta == NULL)
    {
        SWSS_LOG_ERROR("Failed to find attribute metadata: object type %s, attr id %d", strObjectType.c_str(), attrId);

        return SAI_STATUS_INVALID_PARAMETER;
    }

    const std::string strAttrId = meta->attridname;
    const std::string listSize = std::to_string(enumValuesCapability->count);

    const std::vector<swss::FieldValueTuple> entry =
    {
        swss::FieldValueTuple("OBJECT_TYPE", strObjectType),
        swss::FieldValueTuple("ATTR_ID", strAttrId),
        swss::FieldValueTuple("LIST_SIZE", listSize)
    };

    m_communicationChannel->set(strSwitchId, entry, "query_attribute_enum_values_capability");

    swss::KeyOpFieldsValuesTuple kco;

    auto status = m_communicationChannel->wait("query_attribute_enum_values_capability_response", kco);

    if (status == SAI_STATUS_SUCCESS)
    {
        const std::vector<swss::FieldValueTuple> &values = kfvFieldsValues(kco);

        if (values.size() != 2)
        {
            SWSS_LOG_ERROR("logic error, expected 2 values, received %zu", values.size());

            return SAI_STATUS_FAILURE;
        }

        const std::string &capability_str = fvValue(values[0]);
        const uint32_t num_capabilities = std::stoi(fvValue(values[1]));

        enumValuesCapability->count = num_capabilities;

        size_t position = 0;

        for (uint32_t i = 0; i < num_capabilities; i++)
        {
            size_t old_position = position;
            position = capability_str.find(",", old_position);
            std::string capability = capability_str.substr(old_position, position - old_position);
            enumValuesCapability->list[i] = std::stoi(capability);

            // We have run out of values to add to our list
            if (position == std::string::npos)
            {
                if (num_capabilities != i + 1)
                {
                    SWSS_LOG_WARN("Query returned less attributes than expected: expected %d, received %d", num_capabilities, i+1);
                }

                break;
            }

            // Skip the commas
            position++;
        }
    }
    else if (status == SAI_STATUS_BUFFER_OVERFLOW)
    {
        const auto &values = kfvFieldsValues(kco);

        if (values.size() != 1)
        {
            SWSS_LOG_ERROR("logic error, expected 1 value, received %zu", values.size());

            return SAI_STATUS_FAILURE;
        }

        enumValuesCapability->count = std::stoi(fvValue(values[0]));
    }

    return status;
}

sai_object_type_t Sai::objectTypeQuery(
        _In_ sai_object_id_t objectId)
{
    MUTEX();
    SWSS_LOG_ENTER();

    if (!m_apiInitialized)
    {
        SWSS_LOG_ERROR("%s: api not initialized", __PRETTY_FUNCTION__);

        return SAI_OBJECT_TYPE_NULL;
    }

    auto key = sai_serialize_object_id(objectId);

    m_communicationChannel->set(key, {}, "object_type_query");

    swss::KeyOpFieldsValuesTuple kco;

    auto status = m_communicationChannel->wait("object_type_query_response", kco);

    if (status == SAI_STATUS_SUCCESS)
    {
        auto& values = kfvFieldsValues(kco);

        if (values.size() == 0)
        {
            SWSS_LOG_THROW("logic error, api returned 0 values!");
        }

        sai_object_type_t objectType;
        sai_deserialize_object_type(fvValue(values[0]), objectType);

        return objectType;
    }
    else
    {
        SWSS_LOG_ERROR("switchIdQuery failed: %s", sai_serialize_status(status).c_str());
    }

    return SAI_OBJECT_TYPE_NULL;
}

sai_object_id_t Sai::switchIdQuery(
        _In_ sai_object_id_t objectId)
{
    MUTEX();
    SWSS_LOG_ENTER();

    if (!m_apiInitialized)
    {
        SWSS_LOG_ERROR("%s: api not initialized", __PRETTY_FUNCTION__);

        return SAI_NULL_OBJECT_ID;
    }

    auto key = sai_serialize_object_id(objectId);

    m_communicationChannel->set(key, {}, "switch_id_query");

    swss::KeyOpFieldsValuesTuple kco;

    auto status = m_communicationChannel->wait("switch_id_query_response", kco);

    if (status == SAI_STATUS_SUCCESS)
    {
        auto& values = kfvFieldsValues(kco);

        if (values.size() == 0)
        {
            SWSS_LOG_THROW("logic error, api returned 0 values!");
        }

        sai_object_id_t switchId;
        sai_deserialize_object_id(fvValue(values[0]), switchId);

        return switchId;
    }
    else
    {
        SWSS_LOG_ERROR("switchIdQuery failed: %s", sai_serialize_status(status).c_str());
    }

    return SAI_NULL_OBJECT_ID;
}

sai_status_t Sai::logSet(
        _In_ sai_api_t api,
        _In_ sai_log_level_t log_level)
{
    MUTEX();
    SWSS_LOG_ENTER();
    PROXY_CHECK_API_INITIALIZED();

    auto key = sai_serialize_api(api) + ":" + sai_serialize_log_level(log_level);

    m_communicationChannel->set(key, {}, "log_set");

    swss::KeyOpFieldsValuesTuple kco;

    return m_communicationChannel->wait("log_set_response", kco);
}

sai_status_t Sai::queryApiVersion(
        _Out_ sai_api_version_t *version)
{
    MUTEX();
    SWSS_LOG_ENTER();
    PROXY_CHECK_API_INITIALIZED();

    SWSS_LOG_NOTICE("compiled proxy headers SAI API version: %d", SAI_API_VERSION);

    if (version == NULL)
    {
        SWSS_LOG_ERROR("version parameter is NULL");

        return SAI_STATUS_INVALID_PARAMETER;
    }

    m_communicationChannel->set("api", {}, "query_api_version");

    swss::KeyOpFieldsValuesTuple kco;

    auto status = m_communicationChannel->wait("query_api_version_response", kco);

    if (status == SAI_STATUS_SUCCESS)
    {
        auto& values = kfvFieldsValues(kco);

        if (values.size() == 0)
        {
            SWSS_LOG_THROW("logic error, api returned 0 values!");
        }

        SWSS_LOG_NOTICE("returned sai api version: %s", fvValue(values[0]).c_str());

        *version = std::stoull(fvValue(values[0]));
    }

    return status;
}

// TODO use function from SAI metadata to populate those

void Sai::updateNotifications(
        _In_ uint32_t attrCount,
        _In_ const sai_attribute_t *attrList)
{
    SWSS_LOG_ENTER();

    /*
     * This function should only be called on CREATE/SET
     * api when object is SWITCH.
     */

    sai_metadata_update_switch_notification_pointers(&m_sn, attrCount, attrList);
}

void Sai::handleNotification(
        _In_ const std::string &name,
        _In_ const std::string &serializedNotification,
        _In_ const std::vector<swss::FieldValueTuple> &values)
{
    MUTEX();
    SWSS_LOG_ENTER();

    if (!m_apiInitialized)
    {
        SWSS_LOG_ERROR("%s: api not initialized", __PRETTY_FUNCTION__);

        return;
    }

    // TODO should be per switch, and we should know on which switch call notification

    auto notification = sairedis::NotificationFactory::deserialize(name, serializedNotification);

    if (notification)
    {
        SWSS_LOG_INFO("got notification: %s, executing callback!", serializedNotification.c_str());

        // execute callback from notification thread

        notification->executeCallback(m_sn);
    }
}
