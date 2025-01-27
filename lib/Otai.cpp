#include "Otai.h"
#include "OtaiInterface.h"
#include "SwitchConfigContainer.h"
#include "ContextConfigContainer.h"

#include "meta/Meta.h"
#include "meta/otai_serialize.h"

// TODO - simplify recorder

using namespace otairedis;
using namespace std::placeholders;

#define REDIS_CHECK_API_INITIALIZED()                                       \
    if (!m_apiInitialized) {                                                \
        SWSS_LOG_ERROR("%s: api not initialized", __PRETTY_FUNCTION__);     \
        return OTAI_STATUS_FAILURE; }

#define REDIS_CHECK_CONTEXT(oid)                                            \
    auto _globalContext = VirtualObjectIdManager::getGlobalContext(oid);    \
    auto context = getContext(_globalContext);                              \
    if (context == nullptr) {                                               \
        SWSS_LOG_ERROR("no context at index %u for oid %s",                 \
                _globalContext,                                             \
                otai_serialize_object_id(oid).c_str());                      \
        return OTAI_STATUS_FAILURE; }

#define REDIS_CHECK_POINTER(pointer)                                        \
    if ((pointer) == nullptr) {                                             \
        SWSS_LOG_ERROR("entry pointer " # pointer " is null");              \
        return OTAI_STATUS_INVALID_PARAMETER; }

Otai::Otai()
{
    SWSS_LOG_ENTER();

    m_apiInitialized = false;
}

Otai::~Otai()
{
    SWSS_LOG_ENTER();

    if (m_apiInitialized)
    {
        apiUninitialize();
    }
}

// INITIALIZE UNINITIALIZE

otai_status_t Otai::apiInitialize(
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

    m_recorder = std::make_shared<Recorder>();

    const char* contextConfig = service_method_table->profile_get_value(0, OTAI_REDIS_KEY_CONTEXT_CONFIG);

    auto ccc = ContextConfigContainer::loadFromFile(contextConfig);

    for (auto&cc: ccc->getAllContextConfigs())
    {
        auto context = std::make_shared<Context>(cc, m_recorder, std::bind(&Otai::handle_notification, this, _1, _2));

        m_contextMap[cc->m_guid] = context;
    }

    m_apiInitialized = true;

    return OTAI_STATUS_SUCCESS;
}

otai_status_t Otai::apiUninitialize(void)
{
    SWSS_LOG_ENTER();
    REDIS_CHECK_API_INITIALIZED();

    SWSS_LOG_NOTICE("begin");

    m_contextMap.clear();

    m_recorder = nullptr;

    m_apiInitialized = false;

    SWSS_LOG_NOTICE("end");

    return OTAI_STATUS_SUCCESS;
}

// QUAD OID

otai_status_t Otai::create(
        _In_ otai_object_type_t objectType,
        _Out_ otai_object_id_t* objectId,
        _In_ otai_object_id_t switchId,
        _In_ uint32_t attr_count,
        _In_ const otai_attribute_t *attr_list)
{
    MUTEX();
    SWSS_LOG_ENTER();
    REDIS_CHECK_API_INITIALIZED();

    REDIS_CHECK_CONTEXT(switchId);

    if (objectType == OTAI_OBJECT_TYPE_LINECARD && attr_count > 0 && attr_list)
    {
        uint32_t globalContext = 0; // default

        if (attr_list[attr_count - 1].id == OTAI_REDIS_LINECARD_ATTR_CONTEXT)
        {
            globalContext = attr_list[--attr_count].value.u32;
        }

        SWSS_LOG_NOTICE("request switch create with context %u", globalContext);

        context = getContext(globalContext);

        if (context == nullptr)
        {
            SWSS_LOG_ERROR("no global context defined at index %u", globalContext);

            return OTAI_STATUS_FAILURE;
        }
    }

    auto status = context->m_meta->create(
            objectType,
            objectId,
            switchId,
            attr_count,
            attr_list);

    if (objectType == OTAI_OBJECT_TYPE_LINECARD && status == OTAI_STATUS_SUCCESS)
    {
        auto *attr = otai_metadata_get_attr_by_id(
                OTAI_LINECARD_ATTR_INIT_LINECARD,
                attr_count,
                attr_list);

        if (attr && attr->value.booldata == false)
        {
            // request to connect to existing switch

            SWSS_LOG_NOTICE("Otai::create call context populateMetadata");

            context->populateMetadata(*objectId);
        }
    }

    return status;
}

otai_status_t Otai::remove(
        _In_ otai_object_type_t objectType,
        _In_ otai_object_id_t objectId)
{
    MUTEX();
    SWSS_LOG_ENTER();
    REDIS_CHECK_API_INITIALIZED();
    REDIS_CHECK_CONTEXT(objectId);

    return context->m_meta->remove(objectType, objectId);
}

otai_status_t Otai::set(
        _In_ otai_object_type_t objectType,
        _In_ otai_object_id_t objectId,
        _In_ const otai_attribute_t *attr)
{
    MUTEX();
    SWSS_LOG_ENTER();
    REDIS_CHECK_API_INITIALIZED();

    if (RedisRemoteOtaiInterface::isRedisAttribute(objectType, attr))
    {
        if (attr->id == OTAI_REDIS_LINECARD_ATTR_REDIS_COMMUNICATION_MODE)
        {
            // Since communication mode destroys current channel and creates
            // new one, it may happen, that during this SET api execution when
            // api mutex is acquired, channel destructor will be blocking on
            // thread->join() and channel thread will start processing
            // incoming notification. That notification will be synchronized
            // with api mutex and will cause deadlock, so to mitigate this
            // scenario we will unlock api mutex here.
            //
            // This is not the perfect, but assuming that communication mode is
            // changed in single thread and before switch create then we should
            // not hit race condition.

            SWSS_LOG_NOTICE("unlocking api mutex for communication mode");

            MUTEX_UNLOCK();
        }

        // skip metadata if attribute is redis extension attribute

        bool success = true;

        // Setting on all contexts if objectType != OTAI_OBJECT_TYPE_LINECARD or objectId == NULL
        for (auto& kvp: m_contextMap)
        {
            if (objectType == OTAI_OBJECT_TYPE_LINECARD && objectId != OTAI_NULL_OBJECT_ID)
            {
                if (!kvp.second->m_redisOtai->containsSwitch(objectId))
                {
                    continue;
                }
            }

            otai_status_t status = kvp.second->m_redisOtai->set(objectType, objectId, attr);

            success &= (status == OTAI_STATUS_SUCCESS);

            SWSS_LOG_INFO("setting attribute 0x%x status: %s",
                    attr->id,
                    otai_serialize_status(status).c_str());
        }

        return success ? OTAI_STATUS_SUCCESS : OTAI_STATUS_FAILURE;
    }

    REDIS_CHECK_CONTEXT(objectId);

    return context->m_meta->set(objectType, objectId, attr);
}

otai_status_t Otai::get(
        _In_ otai_object_type_t objectType,
        _In_ otai_object_id_t objectId,
        _In_ uint32_t attr_count,
        _Inout_ otai_attribute_t *attr_list)
{
    MUTEX();
    SWSS_LOG_ENTER();
    REDIS_CHECK_API_INITIALIZED();
    REDIS_CHECK_CONTEXT(objectId);

    return context->m_meta->get(
            objectType,
            objectId,
            attr_count,
            attr_list);
}

// QUAD ENTRY

#define DECLARE_CREATE_ENTRY(OT,ot)                                 \
otai_status_t Otai::create(                                           \
        _In_ const otai_ ## ot ## _t* entry,                         \
        _In_ uint32_t attr_count,                                   \
        _In_ const otai_attribute_t *attr_list)                      \
{                                                                   \
    MUTEX();                                                        \
    SWSS_LOG_ENTER();                                               \
    REDIS_CHECK_API_INITIALIZED();                                  \
    REDIS_CHECK_POINTER(entry)                                      \
    REDIS_CHECK_CONTEXT(entry->switch_id);                          \
    return context->m_meta->create(entry, attr_count, attr_list);   \
}

OTAIREDIS_DECLARE_EVERY_ENTRY(DECLARE_CREATE_ENTRY);

#define DECLARE_REMOVE_ENTRY(OT,ot)                         \
otai_status_t Otai::remove(                                   \
        _In_ const otai_ ## ot ## _t* entry)                 \
{                                                           \
    MUTEX();                                                \
    SWSS_LOG_ENTER();                                       \
    REDIS_CHECK_API_INITIALIZED();                          \
    REDIS_CHECK_POINTER(entry)                              \
    REDIS_CHECK_CONTEXT(entry->switch_id);                  \
    return context->m_meta->remove(entry);                  \
}

OTAIREDIS_DECLARE_EVERY_ENTRY(DECLARE_REMOVE_ENTRY);

#define DECLARE_SET_ENTRY(OT,ot)                            \
otai_status_t Otai::set(                                      \
        _In_ const otai_ ## ot ## _t* entry,                 \
        _In_ const otai_attribute_t *attr)                   \
{                                                           \
    MUTEX();                                                \
    SWSS_LOG_ENTER();                                       \
    REDIS_CHECK_API_INITIALIZED();                          \
    REDIS_CHECK_POINTER(entry)                              \
    REDIS_CHECK_CONTEXT(entry->switch_id);                  \
    return context->m_meta->set(entry, attr);               \
}

OTAIREDIS_DECLARE_EVERY_ENTRY(DECLARE_SET_ENTRY);

#define DECLARE_GET_ENTRY(OT,ot)                                \
otai_status_t Otai::get(                                          \
        _In_ const otai_ ## ot ## _t* entry,                     \
        _In_ uint32_t attr_count,                               \
        _Inout_ otai_attribute_t *attr_list)                     \
{                                                               \
    MUTEX();                                                    \
    SWSS_LOG_ENTER();                                           \
    REDIS_CHECK_API_INITIALIZED();                              \
    REDIS_CHECK_POINTER(entry)                                  \
    REDIS_CHECK_CONTEXT(entry->switch_id);                      \
    return context->m_meta->get(entry, attr_count, attr_list);  \
}

OTAIREDIS_DECLARE_EVERY_ENTRY(DECLARE_GET_ENTRY);

// STATS

otai_status_t Otai::getStats(
        _In_ otai_object_type_t object_type,
        _In_ otai_object_id_t object_id,
        _In_ uint32_t number_of_counters,
        _In_ const otai_stat_id_t *counter_ids,
        _Out_ uint64_t *counters)
{
    MUTEX();
    SWSS_LOG_ENTER();
    REDIS_CHECK_API_INITIALIZED();
    REDIS_CHECK_CONTEXT(object_id);

    return context->m_meta->getStats(
            object_type,
            object_id,
            number_of_counters,
            counter_ids,
            counters);
}

otai_status_t Otai::getStatsExt(
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
    REDIS_CHECK_CONTEXT(object_id);

    return context->m_meta->getStatsExt(
            object_type,
            object_id,
            number_of_counters,
            counter_ids,
            mode,
            counters);
}

otai_status_t Otai::clearStats(
        _In_ otai_object_type_t object_type,
        _In_ otai_object_id_t object_id,
        _In_ uint32_t number_of_counters,
        _In_ const otai_stat_id_t *counter_ids)
{
    MUTEX();
    SWSS_LOG_ENTER();
    REDIS_CHECK_API_INITIALIZED();
    REDIS_CHECK_CONTEXT(object_id);

    return context->m_meta->clearStats(
            object_type,
            object_id,
            number_of_counters,
            counter_ids);
}

otai_status_t Otai::bulkGetStats(
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
    SWSS_LOG_ENTER();

    return OTAI_STATUS_NOT_IMPLEMENTED;
}

otai_status_t Otai::bulkClearStats(
        _In_ otai_object_id_t switchId,
        _In_ otai_object_type_t object_type,
        _In_ uint32_t object_count,
        _In_ const otai_object_key_t *object_key,
        _In_ uint32_t number_of_counters,
        _In_ const otai_stat_id_t *counter_ids,
        _In_ otai_stats_mode_t mode,
        _Inout_ otai_status_t *object_statuses)
{
    SWSS_LOG_ENTER();

    return OTAI_STATUS_NOT_IMPLEMENTED;
}

// BULK QUAD OID

otai_status_t Otai::bulkCreate(
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
    REDIS_CHECK_CONTEXT(switch_id);

    return context->m_meta->bulkCreate(
            object_type,
            switch_id,
            object_count,
            attr_count,
            attr_list,
            mode,
            object_id,
            object_statuses);
}

otai_status_t Otai::bulkRemove(
        _In_ otai_object_type_t object_type,
        _In_ uint32_t object_count,
        _In_ const otai_object_id_t *object_id,
        _In_ otai_bulk_op_error_mode_t mode,
        _Out_ otai_status_t *object_statuses)
{
    MUTEX();
    SWSS_LOG_ENTER();
    REDIS_CHECK_API_INITIALIZED();
    REDIS_CHECK_POINTER(object_id);
    REDIS_CHECK_CONTEXT(*object_id);

    return context->m_meta->bulkRemove(
            object_type,
            object_count,
            object_id,
            mode,
            object_statuses);
}

otai_status_t Otai::bulkSet(
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
    REDIS_CHECK_CONTEXT(*object_id);

    return context->m_meta->bulkSet(
            object_type,
            object_count,
            object_id,
            attr_list,
            mode,
            object_statuses);
}

otai_status_t Otai::bulkGet(
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
otai_status_t Otai::bulkCreate(                               \
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
    REDIS_CHECK_POINTER(entries)                            \
    REDIS_CHECK_CONTEXT(entries->switch_id);                \
    return context->m_meta->bulkCreate(                     \
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
otai_status_t Otai::bulkRemove(                               \
        _In_ uint32_t object_count,                         \
        _In_ const otai_ ## ot ## _t *entries,               \
        _In_ otai_bulk_op_error_mode_t mode,                 \
        _Out_ otai_status_t *object_statuses)                \
{                                                           \
    MUTEX();                                                \
    SWSS_LOG_ENTER();                                       \
    REDIS_CHECK_API_INITIALIZED();                          \
    REDIS_CHECK_POINTER(entries)                            \
    REDIS_CHECK_CONTEXT(entries->switch_id);                \
    return context->m_meta->bulkRemove(                     \
            object_count,                                   \
            entries,                                        \
            mode,                                           \
            object_statuses);                               \
}

OTAIREDIS_DECLARE_EVERY_BULK_ENTRY(DECLARE_BULK_REMOVE_ENTRY);

// BULK SET

#define DECLARE_BULK_SET_ENTRY(OT,ot)                       \
otai_status_t Otai::bulkSet(                                  \
        _In_ uint32_t object_count,                         \
        _In_ const otai_ ## ot ## _t *entries,               \
        _In_ const otai_attribute_t *attr_list,              \
        _In_ otai_bulk_op_error_mode_t mode,                 \
        _Out_ otai_status_t *object_statuses)                \
{                                                           \
    MUTEX();                                                \
    SWSS_LOG_ENTER();                                       \
    REDIS_CHECK_API_INITIALIZED();                          \
    REDIS_CHECK_POINTER(entries)                            \
    REDIS_CHECK_CONTEXT(entries->switch_id);                \
    return context->m_meta->bulkSet(                        \
            object_count,                                   \
            entries,                                        \
            attr_list,                                      \
            mode,                                           \
            object_statuses);                               \
}

OTAIREDIS_DECLARE_EVERY_BULK_ENTRY(DECLARE_BULK_SET_ENTRY);

// BULK GET

#define DECLARE_BULK_GET_ENTRY(OT,ot)                       \
otai_status_t Otai::bulkGet(                                  \
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
    REDIS_CHECK_POINTER(ot);                                \
    REDIS_CHECK_POINTER(attr_count);                        \
    REDIS_CHECK_POINTER(attr_list);                         \
    REDIS_CHECK_POINTER(object_statuses);                   \
    SWSS_LOG_ERROR("FIXME not implemented");                \
    return OTAI_STATUS_NOT_IMPLEMENTED;                      \
}

OTAIREDIS_DECLARE_EVERY_BULK_ENTRY(DECLARE_BULK_GET_ENTRY);

// NON QUAD API

otai_status_t Otai::flushFdbEntries(
        _In_ otai_object_id_t switch_id,
        _In_ uint32_t attr_count,
        _In_ const otai_attribute_t *attr_list)
{
    MUTEX();
    SWSS_LOG_ENTER();
    REDIS_CHECK_API_INITIALIZED();
    REDIS_CHECK_CONTEXT(switch_id);

    return context->m_meta->flushFdbEntries(
            switch_id,
            attr_count,
            attr_list);
}

// OTAI API

otai_status_t Otai::objectTypeGetAvailability(
        _In_ otai_object_id_t switchId,
        _In_ otai_object_type_t objectType,
        _In_ uint32_t attrCount,
        _In_ const otai_attribute_t *attrList,
        _Out_ uint64_t *count)
{
    MUTEX();
    SWSS_LOG_ENTER();
    REDIS_CHECK_API_INITIALIZED();
    REDIS_CHECK_CONTEXT(switchId);

    return context->m_meta->objectTypeGetAvailability(
            switchId,
            objectType,
            attrCount,
            attrList,
            count);
}

otai_status_t Otai::queryAttributeEnumValuesCapability(
        _In_ otai_object_id_t switch_id,
        _In_ otai_object_type_t object_type,
        _In_ otai_attr_id_t attr_id,
        _Inout_ otai_s32_list_t *enum_values_capability)
{
    MUTEX();
    SWSS_LOG_ENTER();
    REDIS_CHECK_API_INITIALIZED();
    REDIS_CHECK_CONTEXT(switch_id);

    return context->m_meta->queryAttributeEnumValuesCapability(
            switch_id,
            object_type,
            attr_id,
            enum_values_capability);
}

otai_object_type_t Otai::objectTypeQuery(
        _In_ otai_object_id_t objectId)
{
    SWSS_LOG_ENTER();

    if (!m_apiInitialized)
    {
        SWSS_LOG_ERROR("%s: OTAI API not initialized", __PRETTY_FUNCTION__);

        return OTAI_OBJECT_TYPE_NULL;
    }

    return VirtualObjectIdManager::objectTypeQuery(objectId);
}

otai_object_id_t Otai::switchIdQuery(
        _In_ otai_object_id_t objectId)
{
    SWSS_LOG_ENTER();

    if (!m_apiInitialized)
    {
        SWSS_LOG_ERROR("%s: OTAI API not initialized", __PRETTY_FUNCTION__);

        return OTAI_NULL_OBJECT_ID;
    }

    return VirtualObjectIdManager::switchIdQuery(objectId);
}

otai_status_t Otai::logSet(
        _In_ otai_api_t api,
        _In_ otai_log_level_t log_level)
{
    MUTEX();
    SWSS_LOG_ENTER();
    REDIS_CHECK_API_INITIALIZED();

    for (auto&kvp: m_contextMap)
    {
        kvp.second->m_meta->logSet(api, log_level);
    }

    return OTAI_STATUS_SUCCESS;
}


/*
 * NOTE: Notifications during switch create and switch remove.
 *
 * It is possible that when we create switch we will immediately start getting
 * notifications from it, and it may happen that this switch will not be yet
 * put to switch container and notification won't find it. But before
 * notification will be processed it will first try to acquire mutex, so create
 * switch function will end and switch will be put inside container.
 *
 * Similar it can happen that we receive notification when we are removing
 * switch, then switch will be removed from switch container and notification
 * will not find existing switch, but that's ok since switch was removed, and
 * notification can be ignored.
 */

otai_linecard_notifications_t Otai::handle_notification(
        _In_ std::shared_ptr<Notification> notification,
        _In_ Context* context)
{
    MUTEX();
    SWSS_LOG_ENTER();

    if (!m_apiInitialized)
    {
        SWSS_LOG_ERROR("%s: api not initialized", __PRETTY_FUNCTION__);

        return { };
    }

    return context->m_redisOtai->syncProcessNotification(notification);
}

std::shared_ptr<Context> Otai::getContext(
        _In_ uint32_t globalContext)
{
    SWSS_LOG_ENTER();

    auto it = m_contextMap.find(globalContext);

    if (it == m_contextMap.end())
        return nullptr;

    return it->second;
}

std::vector<swss::FieldValueTuple> serialize_counter_id_list(
        _In_ const otai_enum_metadata_t *stats_enum,
        _In_ uint32_t count,
        _In_ const otai_stat_id_t*counter_id_list)
{
    SWSS_LOG_ENTER();

    std::vector<swss::FieldValueTuple> values;

    for (uint32_t i = 0; i < count; i++)
    {
        const char *name = otai_metadata_get_enum_value_name(stats_enum, counter_id_list[i]);

        if (name == NULL)
        {
            SWSS_LOG_THROW("failed to find enum %d in %s", counter_id_list[i], stats_enum->name);
        }

        values.emplace_back(name, "");
    }

    return values;
}
