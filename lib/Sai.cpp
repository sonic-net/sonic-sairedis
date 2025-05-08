#include "Sai.h"
#include "SaiInternal.h"
#include "SwitchConfigContainer.h"
#include "ContextConfigContainer.h"

#include "meta/Meta.h"
#include "meta/sai_serialize.h"

// TODO - simplify recorder

using namespace sairedis;
using namespace std::placeholders;

#define REDIS_CHECK_API_INITIALIZED()                                       \
    if (!m_apiInitialized) {                                                \
        SWSS_LOG_ERROR("%s: api not initialized", __PRETTY_FUNCTION__);     \
        return SAI_STATUS_FAILURE; }

#define REDIS_CHECK_CONTEXT(oid)                                            \
    auto _globalContext = VirtualObjectIdManager::getGlobalContext(oid);    \
    auto context = getContext(_globalContext);                              \
    if (context == nullptr) {                                               \
        SWSS_LOG_ERROR("no context at index %u for oid %s",                 \
                _globalContext,                                             \
                sai_serialize_object_id(oid).c_str());                      \
        return SAI_STATUS_FAILURE; }

#define REDIS_CHECK_POINTER(pointer)                                        \
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

    m_recorder = std::make_shared<Recorder>();

    const char* contextConfig = service_method_table->profile_get_value(0, SAI_REDIS_KEY_CONTEXT_CONFIG);

    auto ccc = ContextConfigContainer::loadFromFile(contextConfig);

    for (auto&cc: ccc->getAllContextConfigs())
    {
        auto context = std::make_shared<Context>(cc, m_recorder, std::bind(&Sai::handle_notification, this, _1, _2));

        m_contextMap[cc->m_guid] = context;
    }

    m_apiInitialized = true;

    return SAI_STATUS_SUCCESS;
}

sai_status_t Sai::apiUninitialize(void)
{
    SWSS_LOG_ENTER();
    REDIS_CHECK_API_INITIALIZED();

    SWSS_LOG_NOTICE("begin");

    m_contextMap.clear();

    m_recorder = nullptr;

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
    REDIS_CHECK_API_INITIALIZED();

    REDIS_CHECK_CONTEXT(switchId);

    if (objectType == SAI_OBJECT_TYPE_SWITCH && attr_count > 0 && attr_list)
    {
        uint32_t globalContext = 0; // default

        if (attr_list[attr_count - 1].id == SAI_REDIS_SWITCH_ATTR_CONTEXT)
        {
            globalContext = attr_list[--attr_count].value.u32;
        }

        SWSS_LOG_NOTICE("request switch create with context %u", globalContext);

        context = getContext(globalContext);

        if (context == nullptr)
        {
            SWSS_LOG_ERROR("no global context defined at index %u", globalContext);

            return SAI_STATUS_FAILURE;
        }
    }

    auto status = context->m_meta->create(
            objectType,
            objectId,
            switchId,
            attr_count,
            attr_list);

    if (objectType == SAI_OBJECT_TYPE_SWITCH && status == SAI_STATUS_SUCCESS)
    {
        auto *attr = sai_metadata_get_attr_by_id(
                SAI_SWITCH_ATTR_INIT_SWITCH,
                attr_count,
                attr_list);

        if (attr && attr->value.booldata == false)
        {
            // request to connect to existing switch

            SWSS_LOG_NOTICE("Sai::create call context populateMetadata");

            context->populateMetadata(*objectId);
        }
    }

    return status;
}

sai_status_t Sai::remove(
        _In_ sai_object_type_t objectType,
        _In_ sai_object_id_t objectId)
{
    MUTEX();
    SWSS_LOG_ENTER();
    REDIS_CHECK_API_INITIALIZED();
    REDIS_CHECK_CONTEXT(objectId);

    return context->m_meta->remove(objectType, objectId);
}

sai_status_t Sai::set(
        _In_ sai_object_type_t objectType,
        _In_ sai_object_id_t objectId,
        _In_ const sai_attribute_t *attr)
{
    MUTEX();
    SWSS_LOG_ENTER();
    REDIS_CHECK_API_INITIALIZED();

    if (RedisRemoteSaiInterface::isRedisAttribute(objectType, attr))
    {
        if (attr->id == SAI_REDIS_SWITCH_ATTR_REDIS_COMMUNICATION_MODE)
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

        // Setting on all contexts if objectType != SAI_OBJECT_TYPE_SWITCH or objectId == NULL
        for (auto& kvp: m_contextMap)
        {
            if (objectType == SAI_OBJECT_TYPE_SWITCH && objectId != SAI_NULL_OBJECT_ID)
            {
                if (!kvp.second->m_redisSai->containsSwitch(objectId))
                {
                    continue;
                }
            }

            sai_status_t status = kvp.second->m_redisSai->set(objectType, objectId, attr);

            success &= (status == SAI_STATUS_SUCCESS);

            SWSS_LOG_INFO("setting attribute 0x%x status: %s",
                    attr->id,
                    sai_serialize_status(status).c_str());
        }

        return success ? SAI_STATUS_SUCCESS : SAI_STATUS_FAILURE;
    }

    REDIS_CHECK_CONTEXT(objectId);

    return context->m_meta->set(objectType, objectId, attr);
}

sai_status_t Sai::get(
        _In_ sai_object_type_t objectType,
        _In_ sai_object_id_t objectId,
        _In_ uint32_t attr_count,
        _Inout_ sai_attribute_t *attr_list)
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
sai_status_t Sai::create(                                           \
        _In_ const sai_ ## ot ## _t* entry,                         \
        _In_ uint32_t attr_count,                                   \
        _In_ const sai_attribute_t *attr_list)                      \
{                                                                   \
    MUTEX();                                                        \
    SWSS_LOG_ENTER();                                               \
    REDIS_CHECK_API_INITIALIZED();                                  \
    REDIS_CHECK_POINTER(entry)                                      \
    REDIS_CHECK_CONTEXT(entry->switch_id);                          \
    return context->m_meta->create(entry, attr_count, attr_list);   \
}

SAIREDIS_DECLARE_EVERY_ENTRY(DECLARE_CREATE_ENTRY);

#define DECLARE_REMOVE_ENTRY(OT,ot)                         \
sai_status_t Sai::remove(                                   \
        _In_ const sai_ ## ot ## _t* entry)                 \
{                                                           \
    MUTEX();                                                \
    SWSS_LOG_ENTER();                                       \
    REDIS_CHECK_API_INITIALIZED();                          \
    REDIS_CHECK_POINTER(entry)                              \
    REDIS_CHECK_CONTEXT(entry->switch_id);                  \
    return context->m_meta->remove(entry);                  \
}

SAIREDIS_DECLARE_EVERY_ENTRY(DECLARE_REMOVE_ENTRY);

#define DECLARE_SET_ENTRY(OT,ot)                            \
sai_status_t Sai::set(                                      \
        _In_ const sai_ ## ot ## _t* entry,                 \
        _In_ const sai_attribute_t *attr)                   \
{                                                           \
    MUTEX();                                                \
    SWSS_LOG_ENTER();                                       \
    REDIS_CHECK_API_INITIALIZED();                          \
    REDIS_CHECK_POINTER(entry)                              \
    REDIS_CHECK_CONTEXT(entry->switch_id);                  \
    return context->m_meta->set(entry, attr);               \
}

SAIREDIS_DECLARE_EVERY_ENTRY(DECLARE_SET_ENTRY);

#define DECLARE_GET_ENTRY(OT,ot)                                \
sai_status_t Sai::get(                                          \
        _In_ const sai_ ## ot ## _t* entry,                     \
        _In_ uint32_t attr_count,                               \
        _Inout_ sai_attribute_t *attr_list)                     \
{                                                               \
    MUTEX();                                                    \
    SWSS_LOG_ENTER();                                           \
    REDIS_CHECK_API_INITIALIZED();                              \
    REDIS_CHECK_POINTER(entry)                                  \
    REDIS_CHECK_CONTEXT(entry->switch_id);                      \
    return context->m_meta->get(entry, attr_count, attr_list);  \
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
    REDIS_CHECK_API_INITIALIZED();
    REDIS_CHECK_CONTEXT(object_id);

    return context->m_meta->getStats(
            object_type,
            object_id,
            number_of_counters,
            counter_ids,
            counters);
}

sai_status_t Sai::queryStatsCapability(
        _In_ sai_object_id_t switchId,
        _In_ sai_object_type_t objectType,
        _Inout_ sai_stat_capability_list_t *stats_capability)
{
    MUTEX();
    SWSS_LOG_ENTER();
    REDIS_CHECK_API_INITIALIZED();
    REDIS_CHECK_CONTEXT(switchId);

    return context->m_meta->queryStatsCapability(
            switchId,
            objectType,
            stats_capability);
}

sai_status_t Sai::queryStatsStCapability(
    _In_ sai_object_id_t switchId,
    _In_ sai_object_type_t objectType,
    _Inout_ sai_stat_st_capability_list_t *stats_capability)
{
    MUTEX();
    SWSS_LOG_ENTER();
    REDIS_CHECK_API_INITIALIZED();
    REDIS_CHECK_CONTEXT(switchId);

    return context->m_meta->queryStatsStCapability(
        switchId,
        objectType,
        stats_capability);
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

sai_status_t Sai::clearStats(
        _In_ sai_object_type_t object_type,
        _In_ sai_object_id_t object_id,
        _In_ uint32_t number_of_counters,
        _In_ const sai_stat_id_t *counter_ids)
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

sai_status_t Sai::bulkRemove(
        _In_ sai_object_type_t object_type,
        _In_ uint32_t object_count,
        _In_ const sai_object_id_t *object_id,
        _In_ sai_bulk_op_error_mode_t mode,
        _Out_ sai_status_t *object_statuses)
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
    REDIS_CHECK_API_INITIALIZED();
    REDIS_CHECK_CONTEXT(*object_id);

    return context->m_meta->bulkGet(
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
    REDIS_CHECK_API_INITIALIZED();                          \
    REDIS_CHECK_POINTER(entries)                            \
    REDIS_CHECK_CONTEXT(entries->switch_id);                \
    return context->m_meta->bulkRemove(                     \
            object_count,                                   \
            entries,                                        \
            mode,                                           \
            object_statuses);                               \
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
    REDIS_CHECK_API_INITIALIZED();                          \
    REDIS_CHECK_POINTER(ot);                                \
    REDIS_CHECK_POINTER(attr_count);                        \
    REDIS_CHECK_POINTER(attr_list);                         \
    REDIS_CHECK_POINTER(object_statuses);                   \
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
    REDIS_CHECK_API_INITIALIZED();
    REDIS_CHECK_CONTEXT(switch_id);

    return context->m_meta->flushFdbEntries(
            switch_id,
            attr_count,
            attr_list);
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
    REDIS_CHECK_API_INITIALIZED();
    REDIS_CHECK_CONTEXT(switchId);

    return context->m_meta->objectTypeGetAvailability(
            switchId,
            objectType,
            attrCount,
            attrList,
            count);
}

sai_status_t Sai::queryAttributeCapability(
        _In_ sai_object_id_t switch_id,
        _In_ sai_object_type_t object_type,
        _In_ sai_attr_id_t attr_id,
        _Out_ sai_attr_capability_t *capability)
{
    MUTEX();
    SWSS_LOG_ENTER();
    REDIS_CHECK_API_INITIALIZED();
    REDIS_CHECK_CONTEXT(switch_id);

    return context->m_meta->queryAttributeCapability(
            switch_id,
            object_type,
            attr_id,
            capability);
}

sai_status_t Sai::queryAttributeEnumValuesCapability(
        _In_ sai_object_id_t switch_id,
        _In_ sai_object_type_t object_type,
        _In_ sai_attr_id_t attr_id,
        _Inout_ sai_s32_list_t *enum_values_capability)
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

sai_object_type_t Sai::objectTypeQuery(
        _In_ sai_object_id_t objectId)
{
    SWSS_LOG_ENTER();

    if (!m_apiInitialized)
    {
        SWSS_LOG_ERROR("%s: SAI API not initialized", __PRETTY_FUNCTION__);

        return SAI_OBJECT_TYPE_NULL;
    }

    return VirtualObjectIdManager::objectTypeQuery(objectId);
}

sai_object_id_t Sai::switchIdQuery(
        _In_ sai_object_id_t objectId)
{
    SWSS_LOG_ENTER();

    if (!m_apiInitialized)
    {
        SWSS_LOG_ERROR("%s: SAI API not initialized", __PRETTY_FUNCTION__);

        return SAI_NULL_OBJECT_ID;
    }

    return VirtualObjectIdManager::switchIdQuery(objectId);
}

sai_status_t Sai::logSet(
        _In_ sai_api_t api,
        _In_ sai_log_level_t log_level)
{
    MUTEX();
    SWSS_LOG_ENTER();
    REDIS_CHECK_API_INITIALIZED();

    for (auto&kvp: m_contextMap)
    {
        kvp.second->m_meta->logSet(api, log_level);
    }

    return SAI_STATUS_SUCCESS;
}

sai_status_t Sai::queryApiVersion(
        _Out_ sai_api_version_t *version)
{
    MUTEX();
    SWSS_LOG_ENTER();
    REDIS_CHECK_API_INITIALIZED();

    // TODO we should use specific context, but we don't know which one since
    // there is no object ID parameter, we can use default context or cast
    // version as context id same as passed in SAI_REDIS_SWITCH_ATTR_CONTEXT
    // currently we will return just first context on context map, since
    // user maybe not aware of trick with casting context

    for (auto&kvp: m_contextMap)
    {
        SWSS_LOG_WARN("using first context");

        return kvp.second->m_meta->queryApiVersion(version);
    }

    SWSS_LOG_ERROR("context map is empty");

    return SAI_STATUS_FAILURE;
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

sai_switch_notifications_t Sai::handle_notification(
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

    return context->m_redisSai->syncProcessNotification(notification);
}

std::shared_ptr<Context> Sai::getContext(
        _In_ uint32_t globalContext)
{
    SWSS_LOG_ENTER();

    auto it = m_contextMap.find(globalContext);

    if (it == m_contextMap.end())
        return nullptr;

    return it->second;
}

std::vector<swss::FieldValueTuple> serialize_counter_id_list(
        _In_ const sai_enum_metadata_t *stats_enum,
        _In_ uint32_t count,
        _In_ const sai_stat_id_t*counter_id_list)
{
    SWSS_LOG_ENTER();

    std::vector<swss::FieldValueTuple> values;

    for (uint32_t i = 0; i < count; i++)
    {
        const char *name = sai_metadata_get_enum_value_name(stats_enum, counter_id_list[i]);

        if (name == NULL)
        {
            SWSS_LOG_THROW("failed to find enum %d in %s", counter_id_list[i], stats_enum->name);
        }

        values.emplace_back(name, "");
    }

    return values;
}
