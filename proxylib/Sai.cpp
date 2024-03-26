#include "Sai.h"
#include "SaiInternal.h"

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
        uninitialize();
    }
}

// INITIALIZE UNINITIALIZE

sai_status_t Sai::initialize(
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

    // TODO move to service method table and
    // TODO enable for channel to work!
//    m_communicationChannel = std::make_shared<_ZeroMQChannel>(
//            "/tmp/saiproxy",
//            "/tmp/saiproxy_ntf",
//            std::bind(&Sai::handleNotification, this, _1, _2, _3));

    m_apiInitialized = true;

    return SAI_STATUS_SUCCESS;
}

sai_status_t Sai::uninitialize(void)
{
    SWSS_LOG_ENTER();
    PROXY_CHECK_API_INITIALIZED();

    SWSS_LOG_NOTICE("begin");

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

    SWSS_LOG_ERROR("not implemented, FIXME");

    return SAI_STATUS_NOT_IMPLEMENTED;
}

sai_status_t Sai::remove(
        _In_ sai_object_type_t objectType,
        _In_ sai_object_id_t objectId)
{
    MUTEX();
    SWSS_LOG_ENTER();
    PROXY_CHECK_API_INITIALIZED();

    SWSS_LOG_ERROR("not implemented, FIXME");

    return SAI_STATUS_NOT_IMPLEMENTED;
}

sai_status_t Sai::set(
        _In_ sai_object_type_t objectType,
        _In_ sai_object_id_t objectId,
        _In_ const sai_attribute_t *attr)
{
    MUTEX();
    SWSS_LOG_ENTER();
    PROXY_CHECK_API_INITIALIZED();

    SWSS_LOG_ERROR("not implemented, FIXME");

    return SAI_STATUS_NOT_IMPLEMENTED;
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

    SWSS_LOG_ERROR("not implemented, FIXME");

    return SAI_STATUS_NOT_IMPLEMENTED;
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
    SWSS_LOG_ERROR("not implemented, FIXME");               \
    return SAI_STATUS_NOT_IMPLEMENTED;                      \
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
    SWSS_LOG_ERROR("not implemented, FIXME");               \
    return SAI_STATUS_NOT_IMPLEMENTED;                      \
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
    SWSS_LOG_ERROR("not implemented, FIXME");               \
    return SAI_STATUS_NOT_IMPLEMENTED;                      \
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
    SWSS_LOG_ERROR("not implemented, FIXME");               \
    return SAI_STATUS_NOT_IMPLEMENTED;                      \
}

SAIREDIS_DECLARE_EVERY_ENTRY(DECLARE_GET_ENTRY);

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

    SWSS_LOG_ERROR("not implemented, FIXME");

    return SAI_STATUS_NOT_IMPLEMENTED;
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

    SWSS_LOG_ERROR("not implemented, FIXME");

    return SAI_STATUS_NOT_IMPLEMENTED;
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

    SWSS_LOG_ERROR("not implemented, FIXME");

    return SAI_STATUS_NOT_IMPLEMENTED;
}

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

// NON QUAD API

sai_status_t Sai::flushFdbEntries(
        _In_ sai_object_id_t switch_id,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    MUTEX();
    SWSS_LOG_ENTER();
    PROXY_CHECK_API_INITIALIZED();

    SWSS_LOG_ERROR("not implemented, FIXME");

    return SAI_STATUS_NOT_IMPLEMENTED;
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

    SWSS_LOG_ERROR("not implemented, FIXME");

    return SAI_STATUS_NOT_IMPLEMENTED;
}

sai_status_t Sai::queryAttributeCapability(
        _In_ sai_object_id_t switch_id,
        _In_ sai_object_type_t object_type,
        _In_ sai_attr_id_t attr_id,
        _Out_ sai_attr_capability_t *capability)
{
    MUTEX();
    SWSS_LOG_ENTER();
    PROXY_CHECK_API_INITIALIZED();

    SWSS_LOG_ERROR("not implemented, FIXME");

    return SAI_STATUS_NOT_IMPLEMENTED;
}

sai_status_t Sai::queryAattributeEnumValuesCapability(
        _In_ sai_object_id_t switch_id,
        _In_ sai_object_type_t object_type,
        _In_ sai_attr_id_t attr_id,
        _Inout_ sai_s32_list_t *enum_values_capability)
{
    MUTEX();
    SWSS_LOG_ENTER();
    PROXY_CHECK_API_INITIALIZED();

    SWSS_LOG_ERROR("not implemented, FIXME");

    return SAI_STATUS_NOT_IMPLEMENTED;
}

sai_object_type_t Sai::objectTypeQuery(
        _In_ sai_object_id_t objectId)
{
    MUTEX();
    SWSS_LOG_ENTER();

    if (!m_apiInitialized)
    {
        SWSS_LOG_ERROR("%s: SAI API not initialized", __PRETTY_FUNCTION__);

        return SAI_OBJECT_TYPE_NULL;
    }

    //json j;

    //j["switch_id"] = sai_serialize_object_id(switch_id);

    //return j.dump();

    // TODO implement !

    SWSS_LOG_ERROR("not implemented, FIXME");

    return SAI_OBJECT_TYPE_NULL;
}

sai_object_id_t Sai::switchIdQuery(
        _In_ sai_object_id_t objectId)
{
    MUTEX();
    SWSS_LOG_ENTER();

    if (!m_apiInitialized)
    {
        SWSS_LOG_ERROR("%s: SAI API not initialized", __PRETTY_FUNCTION__);

        return SAI_NULL_OBJECT_ID;
    }

    // TODO implement !

    SWSS_LOG_ERROR("not implemented, FIXME");

    return SAI_NULL_OBJECT_ID;
}

sai_status_t Sai::logSet(
        _In_ sai_api_t api,
        _In_ sai_log_level_t log_level)
{
    MUTEX();
    SWSS_LOG_ENTER();
    PROXY_CHECK_API_INITIALIZED();

    SWSS_LOG_WARN("log set not implemented for %s:%s",
            sai_serialize_api(api).c_str(),
            sai_serialize_log_level(log_level).c_str());

    return SAI_STATUS_SUCCESS;
}

//sai_switch_notifications_t Sai::handle_notification(
//        _In_ std::shared_ptr<Notification> notification)
//{
//    MUTEX();
//    SWSS_LOG_ENTER();
//
//    if (!m_apiInitialized)
//    {
//        SWSS_LOG_ERROR("%s: api not initialized", __PRETTY_FUNCTION__);
//
//        return { };
//    }
//
//    return context->m_redisSai->syncProcessNotification(notification);
//}
//
//void Sai::handleNotification(
//        _In_ const std::string &name,
//        _In_ const std::string &serializedNotification,
//        _In_ const std::vector<swss::FieldValueTuple> &values)
//{
//    SWSS_LOG_ENTER();
//
//    auto notification = NotificationFactory::deserialize(name, serializedNotification);
//
//    if (notification)
//    {
//        auto _sn = m_notificationCallback(notification); // will be synchronized to api mutex
//
//        // execute callback from notification thread
//
//        notification->executeCallback(_sn);
//    }
//}
