#include "RedisRemoteSaiInterface.h"
#include "Utils.h"
#include "Recorder.h"
#include "VirtualObjectIdManager.h"
#include "SkipRecordAttrContainer.h"
#include "SwitchContainer.h"
#include "ZeroMQChannel.h"

#include "sairediscommon.h"

#include "meta/NotificationFactory.h"
#include "meta/sai_serialize.h"
#include "meta/SaiAttributeList.h"
#include "meta/PerformanceIntervalTimer.h"
#include "meta/Globals.h"

#include "swss/tokenize.h"

#include "config.h"

#include <inttypes.h>

using namespace sairedis;
using namespace saimeta;
using namespace sairediscommon;
using namespace std::placeholders;

std::vector<swss::FieldValueTuple> serialize_counter_id_list(
        _In_ const sai_enum_metadata_t *stats_enum,
        _In_ uint32_t count,
        _In_ const sai_stat_id_t *counter_id_list);

RedisRemoteSaiInterface::RedisRemoteSaiInterface(
        _In_ std::shared_ptr<ContextConfig> contextConfig,
        _In_ std::function<sai_switch_notifications_t(std::shared_ptr<Notification>)> notificationCallback,
        _In_ std::shared_ptr<Recorder> recorder):
    m_contextConfig(contextConfig),
    m_redisCommunicationMode(SAI_REDIS_COMMUNICATION_MODE_REDIS_ASYNC),
    m_recorder(recorder),
    m_notificationCallback(notificationCallback)
{
    SWSS_LOG_ENTER();

    SWSS_LOG_NOTICE("sairedis git revision %s, SAI git revision: %s", SAIREDIS_GIT_REVISION, SAI_GIT_REVISION);

    m_initialized = false;

    apiInitialize(0, nullptr);
}

RedisRemoteSaiInterface::~RedisRemoteSaiInterface()
{
    SWSS_LOG_ENTER();

    if (m_initialized)
    {
        apiUninitialize();
    }
}

sai_status_t RedisRemoteSaiInterface::apiInitialize(
        _In_ uint64_t flags,
        _In_ const sai_service_method_table_t *service_method_table)
{
    SWSS_LOG_ENTER();

    if (m_initialized)
    {
        SWSS_LOG_ERROR("already initialized");

        return SAI_STATUS_FAILURE;
    }

    m_skipRecordAttrContainer = std::make_shared<SkipRecordAttrContainer>();

    m_asicInitViewMode = false; // default mode is apply mode
    m_useTempView = false;
    m_syncMode = false;
    m_redisCommunicationMode = SAI_REDIS_COMMUNICATION_MODE_REDIS_ASYNC;

    if (m_contextConfig->m_zmqEnable)
    {
        m_communicationChannel = std::make_shared<ZeroMQChannel>(
                m_contextConfig->m_zmqEndpoint,
                m_contextConfig->m_zmqNtfEndpoint,
                std::bind(&RedisRemoteSaiInterface::handleNotification, this, _1, _2, _3));

        SWSS_LOG_NOTICE("zmq enabled, forcing sync mode");

        m_syncMode = true;
    }
    else
    {
        m_communicationChannel = std::make_shared<RedisChannel>(
                m_contextConfig->m_dbAsic,
                std::bind(&RedisRemoteSaiInterface::handleNotification, this, _1, _2, _3));
    }

    m_responseTimeoutMs = m_communicationChannel->getResponseTimeout();

    m_db = std::make_shared<swss::DBConnector>(m_contextConfig->m_dbAsic, 0);

    m_redisVidIndexGenerator = std::make_shared<RedisVidIndexGenerator>(m_db, REDIS_KEY_VIDCOUNTER);

    clear_local_state();

    // TODO what will happen when we receive notification in init view mode ?

    m_initialized = true;

    return SAI_STATUS_SUCCESS;
}

sai_status_t RedisRemoteSaiInterface::apiUninitialize(void)
{
    SWSS_LOG_ENTER();

    SWSS_LOG_NOTICE("begin");

    if (!m_initialized)
    {
        SWSS_LOG_ERROR("not initialized");

        return SAI_STATUS_FAILURE;
    }

    m_communicationChannel = nullptr; // will stop thread

    // clear local state after stopping threads

    clear_local_state();

    m_initialized = false;

    SWSS_LOG_NOTICE("end");

    return SAI_STATUS_SUCCESS;
}

sai_status_t RedisRemoteSaiInterface::create(
        _In_ sai_object_type_t objectType,
        _Out_ sai_object_id_t* objectId,
        _In_ sai_object_id_t switchId,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    *objectId = SAI_NULL_OBJECT_ID;

    if (objectType == SAI_OBJECT_TYPE_SWITCH)
    {
        // for given hardware info we always return same switch id,
        // this is required since we could be performing warm boot here

        auto hwinfo = Globals::getHardwareInfo(attr_count, attr_list);

        if (hwinfo.size())
        {
            m_recorder->recordComment("SAI_SWITCH_ATTR_SWITCH_HARDWARE_INFO=" + hwinfo);
        }

        switchId = m_virtualObjectIdManager->allocateNewSwitchObjectId(hwinfo);

        *objectId = switchId;

        if (switchId == SAI_NULL_OBJECT_ID)
        {
            SWSS_LOG_ERROR("switch ID allocation failed");

            return SAI_STATUS_FAILURE;
        }

        auto *attr = sai_metadata_get_attr_by_id(
                SAI_SWITCH_ATTR_INIT_SWITCH,
                attr_count,
                attr_list);

        if (attr && attr->value.booldata == false)
        {
            if (m_switchContainer->contains(*objectId))
            {
                SWSS_LOG_NOTICE("switch container already contains switch %s",
                        sai_serialize_object_id(*objectId).c_str());

                return SAI_STATUS_SUCCESS;
            }

            refreshTableDump();

            if (m_tableDump.find(switchId) == m_tableDump.end())
            {
                SWSS_LOG_ERROR("failed to find switch %s to connect (init=false)",
                        sai_serialize_object_id(switchId).c_str());

                m_virtualObjectIdManager->releaseObjectId(switchId);

                return SAI_STATUS_FAILURE;
            }

            // when init is false, don't send query to syncd, just return success
            // that we found switch and we connected to it

            auto sw = std::make_shared<Switch>(*objectId, attr_count, attr_list);

            m_switchContainer->insert(sw);

            return SAI_STATUS_SUCCESS;
        }
    }
    else
    {
        *objectId = m_virtualObjectIdManager->allocateNewObjectId(objectType, switchId);
    }

    if (*objectId == SAI_NULL_OBJECT_ID)
    {
        SWSS_LOG_ERROR("failed to create %s, with switch id: %s",
                sai_serialize_object_type(objectType).c_str(),
                sai_serialize_object_id(switchId).c_str());

        return SAI_STATUS_INSUFFICIENT_RESOURCES;
    }

    // NOTE: objectId was allocated by the caller

    auto status = create(
            objectType,
            sai_serialize_object_id(*objectId),
            attr_count,
            attr_list);

    if (objectType == SAI_OBJECT_TYPE_SWITCH && status == SAI_STATUS_SUCCESS)
    {
        /*
         * When doing CREATE operation user may want to update notification
         * pointers, since notifications can be defined per switch we need to
         * update them.
         *
         * TODO: should be moved inside to redis_generic_create
         */

        auto sw = std::make_shared<Switch>(*objectId, attr_count, attr_list);

        m_switchContainer->insert(sw);
    }
    else if (status != SAI_STATUS_SUCCESS)
    {
        // if create failed, then release allocated object
        m_virtualObjectIdManager->releaseObjectId(*objectId);
    }

    return status;
}

sai_status_t RedisRemoteSaiInterface::remove(
        _In_ sai_object_type_t objectType,
        _In_ sai_object_id_t objectId)
{
    SWSS_LOG_ENTER();

    auto status = remove(
            objectType,
            sai_serialize_object_id(objectId));

    if (objectType == SAI_OBJECT_TYPE_SWITCH && status == SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_NOTICE("removing switch id %s", sai_serialize_object_id(objectId).c_str());

        m_virtualObjectIdManager->releaseObjectId(objectId);

        // remove switch from container
        m_switchContainer->removeSwitch(objectId);
    }

    return status;
}

sai_status_t RedisRemoteSaiInterface::setRedisExtensionAttribute(
        _In_ sai_object_type_t objectType,
        _In_ sai_object_id_t objectId,
        _In_ const sai_attribute_t *attr)
{
    SWSS_LOG_ENTER();

    if (attr == nullptr)
    {
        SWSS_LOG_ERROR("attr pointer is null");

        return SAI_STATUS_FAILURE;
    }

    /*
     * NOTE: that this will work without
     * switch being created.
     */

    switch (attr->id)
    {
        case SAI_REDIS_SWITCH_ATTR_PERFORM_LOG_ROTATE:

            if (m_recorder)
            {
                m_recorder->requestLogRotate();
            }

            return SAI_STATUS_SUCCESS;

        case SAI_REDIS_SWITCH_ATTR_RECORD:

            if (m_recorder)
            {
                m_recorder->enableRecording(attr->value.booldata);
            }

            return SAI_STATUS_SUCCESS;

        case SAI_REDIS_SWITCH_ATTR_NOTIFY_SYNCD:

            return sai_redis_notify_syncd(objectId, attr);

        case SAI_REDIS_SWITCH_ATTR_USE_TEMP_VIEW:

            m_useTempView = attr->value.booldata;

            return SAI_STATUS_SUCCESS;

        case SAI_REDIS_SWITCH_ATTR_RECORD_STATS:

            m_recorder->recordStats(attr->value.booldata);

            return SAI_STATUS_SUCCESS;

        case SAI_REDIS_SWITCH_ATTR_SYNC_OPERATION_RESPONSE_TIMEOUT:

            m_responseTimeoutMs = attr->value.u64;

            m_communicationChannel->setResponseTimeout(m_responseTimeoutMs);

            SWSS_LOG_NOTICE("set response timeout to %lu ms", m_responseTimeoutMs);

            return SAI_STATUS_SUCCESS;

        case SAI_REDIS_SWITCH_ATTR_SYNC_MODE:

            SWSS_LOG_WARN("sync mode is depreacated, use communication mode");

            m_syncMode = attr->value.booldata;

            if (m_contextConfig->m_zmqEnable)
            {
                SWSS_LOG_NOTICE("zmq enabled, forcing sync mode");

                m_syncMode = true;
            }

            if (m_syncMode)
            {
                SWSS_LOG_NOTICE("disabling buffered pipeline in sync mode");

                m_communicationChannel->setBuffered(false);
            }

            return SAI_STATUS_SUCCESS;

        case SAI_REDIS_SWITCH_ATTR_REDIS_COMMUNICATION_MODE:

            m_redisCommunicationMode = (sai_redis_communication_mode_t)attr->value.s32;

            if (m_contextConfig->m_zmqEnable)
            {
                SWSS_LOG_NOTICE("zmq enabled via context config");

                m_redisCommunicationMode = SAI_REDIS_COMMUNICATION_MODE_ZMQ_SYNC;
            }

            m_communicationChannel = nullptr;

            switch (m_redisCommunicationMode)
            {
                case SAI_REDIS_COMMUNICATION_MODE_REDIS_ASYNC:

                    SWSS_LOG_NOTICE("enabling redis async mode");

                    m_syncMode = false;

                    m_communicationChannel = std::make_shared<RedisChannel>(
                            m_contextConfig->m_dbAsic,
                            std::bind(&RedisRemoteSaiInterface::handleNotification, this, _1, _2, _3));

                    m_communicationChannel->setResponseTimeout(m_responseTimeoutMs);

                    m_communicationChannel->setBuffered(true);

                    return SAI_STATUS_SUCCESS;

                case SAI_REDIS_COMMUNICATION_MODE_REDIS_SYNC:

                    SWSS_LOG_NOTICE("enabling redis sync mode");

                    m_syncMode = true;

                    m_communicationChannel = std::make_shared<RedisChannel>(
                            m_contextConfig->m_dbAsic,
                            std::bind(&RedisRemoteSaiInterface::handleNotification, this, _1, _2, _3));

                    m_communicationChannel->setResponseTimeout(m_responseTimeoutMs);

                    m_communicationChannel->setBuffered(false);

                    return SAI_STATUS_SUCCESS;

                case SAI_REDIS_COMMUNICATION_MODE_ZMQ_SYNC:

                    m_contextConfig->m_zmqEnable = true;

                    // main communication channel was created at initialize method
                    // so this command will replace it with zmq channel

                    m_communicationChannel = std::make_shared<ZeroMQChannel>(
                            m_contextConfig->m_zmqEndpoint,
                            m_contextConfig->m_zmqNtfEndpoint,
                            std::bind(&RedisRemoteSaiInterface::handleNotification, this, _1, _2, _3));

                    m_communicationChannel->setResponseTimeout(m_responseTimeoutMs);

                    SWSS_LOG_NOTICE("zmq enabled, forcing sync mode");

                    m_syncMode = true;

                    SWSS_LOG_NOTICE("disabling buffered pipeline in sync mode");

                    m_communicationChannel->setBuffered(false);

                    return SAI_STATUS_SUCCESS;

                default:

                    SWSS_LOG_ERROR("invalid communication mode value: %d", m_redisCommunicationMode);

                    return SAI_STATUS_NOT_SUPPORTED;
            }

        case SAI_REDIS_SWITCH_ATTR_USE_PIPELINE:

            if (m_syncMode)
            {
                SWSS_LOG_WARN("use pipeline is not supported in sync mode");

                return SAI_STATUS_NOT_SUPPORTED;
            }

            m_communicationChannel->setBuffered(attr->value.booldata);

            return SAI_STATUS_SUCCESS;

        case SAI_REDIS_SWITCH_ATTR_FLUSH:

            m_communicationChannel->flush();

            return SAI_STATUS_SUCCESS;

        case SAI_REDIS_SWITCH_ATTR_RECORDING_OUTPUT_DIR:

            if (m_recorder)
            {
                m_recorder->setRecordingOutputDirectory(*attr);
            }

            return SAI_STATUS_SUCCESS;

        case SAI_REDIS_SWITCH_ATTR_RECORDING_FILENAME:

            if (m_recorder)
            {
                m_recorder->setRecordingFilename(*attr);
            }

            return SAI_STATUS_SUCCESS;

        case SAI_REDIS_SWITCH_ATTR_FLEX_COUNTER_GROUP:
            return notifyCounterGroupOperations(objectId,
                                                reinterpret_cast<sai_redis_flex_counter_group_parameter_t*>(attr->value.ptr));

        case SAI_REDIS_SWITCH_ATTR_FLEX_COUNTER:
            return notifyCounterOperations(objectId,
                                           reinterpret_cast<sai_redis_flex_counter_parameter_t*>(attr->value.ptr));

        default:
            break;
    }

    SWSS_LOG_ERROR("unknown redis extension attribute: %d", attr->id);

    return SAI_STATUS_FAILURE;
}

bool RedisRemoteSaiInterface::isSaiS8ListValidString(
        _In_ const sai_s8_list_t &s8list)
{
    SWSS_LOG_ENTER();

    if (s8list.list != nullptr && s8list.count > 0)
    {
        size_t len = strnlen((const char *)s8list.list, s8list.count);

        if (len == (size_t)s8list.count)
        {
            return true;
        }
        else
        {
            SWSS_LOG_ERROR("Count (%u) is different than strnlen (%zu)", s8list.count, len);
        }
    }

    return false;
}

bool RedisRemoteSaiInterface::emplaceStrings(
        _In_ const sai_s8_list_t &field,
        _In_ const sai_s8_list_t &value,
        _Out_ std::vector<swss::FieldValueTuple> &entries)
{
    SWSS_LOG_ENTER();

    bool result = false;

    if (isSaiS8ListValidString(field) && isSaiS8ListValidString(value))
    {
        entries.emplace_back(std::string((const char*)field.list, field.count), std::string((const char*)value.list, value.count));
        result = true;
    }

    return result;
}

bool RedisRemoteSaiInterface::emplaceStrings(
        _In_ const char *field,
        _In_ const sai_s8_list_t &value,
        _Out_ std::vector<swss::FieldValueTuple> &entries)
{
    SWSS_LOG_ENTER();

    bool result = false;

    if (isSaiS8ListValidString(value))
    {
        entries.emplace_back(field, std::string((const char*)value.list, value.count));
        result = true;
    }

    return result;
}

sai_status_t RedisRemoteSaiInterface::notifyCounterGroupOperations(
        _In_ sai_object_id_t objectId,
        _In_ const sai_redis_flex_counter_group_parameter_t *flexCounterGroupParam)
{
    SWSS_LOG_ENTER();

    std::vector<swss::FieldValueTuple> entries;

    if (flexCounterGroupParam == nullptr || !isSaiS8ListValidString(flexCounterGroupParam->counter_group_name))
    {
        SWSS_LOG_ERROR("Invalid parameters when handling counter group operation");
        return SAI_STATUS_FAILURE;
    }

    std::string key((const char*)flexCounterGroupParam->counter_group_name.list, flexCounterGroupParam->counter_group_name.count);

    emplaceStrings(POLL_INTERVAL_FIELD, flexCounterGroupParam->poll_interval, entries);
    emplaceStrings(BULK_CHUNK_SIZE_FIELD, flexCounterGroupParam->bulk_chunk_size, entries);
    emplaceStrings(BULK_CHUNK_SIZE_PER_PREFIX_FIELD, flexCounterGroupParam->bulk_chunk_size_per_prefix, entries);
    emplaceStrings(STATS_MODE_FIELD, flexCounterGroupParam->stats_mode, entries);
    emplaceStrings(flexCounterGroupParam->plugin_name, flexCounterGroupParam->plugins, entries);
    emplaceStrings(FLEX_COUNTER_STATUS_FIELD, flexCounterGroupParam->operation, entries);

    m_recorder->recordGenericCounterPolling(key, entries);

    m_communicationChannel->set(key,
                                entries,
                                (entries.size() != 0) ? REDIS_FLEX_COUNTER_COMMAND_SET_GROUP : REDIS_FLEX_COUNTER_COMMAND_DEL_GROUP);

    return waitForResponse(SAI_COMMON_API_SET);
}

sai_status_t RedisRemoteSaiInterface::notifyCounterOperations(
        _In_ sai_object_id_t objectId,
        _In_ const sai_redis_flex_counter_parameter_t *flexCounterParam)
{
    SWSS_LOG_ENTER();

    if (flexCounterParam == nullptr || !isSaiS8ListValidString(flexCounterParam->counter_key))
    {
        SWSS_LOG_ERROR("Invalid parameters when handling counter operation");
        return SAI_STATUS_FAILURE;
    }

    std::vector<swss::FieldValueTuple> entries;
    std::string key((const char*)flexCounterParam->counter_key.list, flexCounterParam->counter_key.count);
    std::string command;

    if (emplaceStrings(flexCounterParam->counter_field_name, flexCounterParam->counter_ids, entries))
    {
        command = REDIS_FLEX_COUNTER_COMMAND_START_POLL;
        emplaceStrings(STATS_MODE_FIELD, flexCounterParam->stats_mode, entries);
    }
    else
    {
        command = REDIS_FLEX_COUNTER_COMMAND_STOP_POLL;
    }

    m_recorder->recordGenericCounterPolling(key, entries);
    m_communicationChannel->set(key, entries, command);

    return waitForResponse(SAI_COMMON_API_SET);
}

sai_status_t RedisRemoteSaiInterface::set(
        _In_ sai_object_type_t objectType,
        _In_ sai_object_id_t objectId,
        _In_ const sai_attribute_t *attr)
{
    SWSS_LOG_ENTER();

    if (RedisRemoteSaiInterface::isRedisAttribute(objectType, attr))
    {
        return setRedisExtensionAttribute(objectType, objectId, attr);
    }

    auto status = set(
            objectType,
            sai_serialize_object_id(objectId),
            attr);

    if (objectType == SAI_OBJECT_TYPE_SWITCH && status == SAI_STATUS_SUCCESS)
    {
        auto sw = m_switchContainer->getSwitch(objectId);

        if (!sw)
        {
            SWSS_LOG_THROW("failed to find switch %s in container",
                    sai_serialize_object_id(objectId).c_str());
        }

        /*
         * When doing SET operation user may want to update notification
         * pointers.
         */

        sw->updateNotifications(1, attr);
    }

    return status;
}

sai_status_t RedisRemoteSaiInterface::get(
        _In_ sai_object_type_t objectType,
        _In_ sai_object_id_t objectId,
        _In_ uint32_t attr_count,
        _Inout_ sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    return get(
            objectType,
            sai_serialize_object_id(objectId),
            attr_count,
            attr_list);
}


#define DECLARE_REMOVE_ENTRY(OT,ot)                             \
sai_status_t RedisRemoteSaiInterface::remove(                   \
        _In_ const sai_ ## ot ## _t* ot)                        \
{                                                               \
    SWSS_LOG_ENTER();                                           \
    return remove(                                              \
            (sai_object_type_t)SAI_OBJECT_TYPE_ ## OT,          \
            sai_serialize_ ## ot(*ot));                         \
}

SAIREDIS_DECLARE_EVERY_ENTRY(DECLARE_REMOVE_ENTRY);

#define DECLARE_CREATE_ENTRY(OT,ot)                             \
sai_status_t RedisRemoteSaiInterface::create(                   \
        _In_ const sai_ ## ot ## _t* ot,                        \
        _In_ uint32_t attr_count,                               \
        _In_ const sai_attribute_t *attr_list)                  \
{                                                               \
    SWSS_LOG_ENTER();                                           \
    return create(                                              \
            (sai_object_type_t)SAI_OBJECT_TYPE_ ## OT,          \
            sai_serialize_ ## ot(*ot),                          \
            attr_count,                                         \
            attr_list);                                         \
}

SAIREDIS_DECLARE_EVERY_ENTRY(DECLARE_CREATE_ENTRY);

#define DECLARE_SET_ENTRY(OT,ot)                                \
sai_status_t RedisRemoteSaiInterface::set(                      \
        _In_ const sai_ ## ot ## _t* ot,                        \
        _In_ const sai_attribute_t *attr)                       \
{                                                               \
    SWSS_LOG_ENTER();                                           \
    return set(                                                 \
            (sai_object_type_t)SAI_OBJECT_TYPE_ ## OT,          \
            sai_serialize_ ## ot(*ot),                          \
            attr);                                              \
}

SAIREDIS_DECLARE_EVERY_ENTRY(DECLARE_SET_ENTRY);

#define DECLARE_BULK_CREATE_ENTRY(OT,ot)                                                     \
sai_status_t RedisRemoteSaiInterface::bulkCreate(                                            \
        _In_ uint32_t object_count,                                                          \
        _In_ const sai_ ## ot ## _t *ot,                                                     \
        _In_ const uint32_t *attr_count,                                                     \
        _In_ const sai_attribute_t **attr_list,                                              \
        _In_ sai_bulk_op_error_mode_t mode,                                                  \
        _Out_ sai_status_t *object_statuses)                                                 \
{                                                                                            \
    SWSS_LOG_ENTER();                                                                        \
    static PerformanceIntervalTimer timer("RedisRemoteSaiInterface::bulkCreate(" #ot ")");   \
    timer.start();                                                                           \
    std::vector<std::string> serialized_object_ids;                                          \
    for (uint32_t idx = 0; idx < object_count; idx++)                                        \
    {                                                                                        \
        std::string str_object_id = sai_serialize_ ##ot (ot[idx]);                           \
        serialized_object_ids.push_back(str_object_id);                                      \
    }                                                                                        \
    auto status = bulkCreate(                                                                \
            (sai_object_type_t)SAI_OBJECT_TYPE_ ## OT,                                       \
            serialized_object_ids,                                                           \
            attr_count,                                                                      \
            attr_list,                                                                       \
            mode,                                                                            \
            object_statuses);                                                                \
    timer.stop();                                                                            \
    timer.inc(object_count);                                                                 \
    return status;                                                                           \
}

SAIREDIS_DECLARE_EVERY_BULK_ENTRY(DECLARE_BULK_CREATE_ENTRY);

#define DECLARE_BULK_REMOVE_ENTRY(OT,ot)                                                                        \
sai_status_t RedisRemoteSaiInterface::bulkRemove(                                                               \
        _In_ uint32_t object_count,                                                                             \
        _In_ const sai_ ## ot ## _t *ot,                                                                        \
        _In_ sai_bulk_op_error_mode_t mode,                                                                     \
        _Out_ sai_status_t *object_statuses)                                                                    \
{                                                                                                               \
    SWSS_LOG_ENTER();                                                                                           \
    std::vector<std::string> serializedObjectIds;                                                               \
    for (uint32_t idx = 0; idx < object_count; idx++)                                                           \
    {                                                                                                           \
        serializedObjectIds.emplace_back(sai_serialize_ ##ot (ot[idx]));                                        \
    }                                                                                                           \
    return bulkRemove((sai_object_type_t)SAI_OBJECT_TYPE_ ## OT, serializedObjectIds, mode, object_statuses);   \
}

SAIREDIS_DECLARE_EVERY_BULK_ENTRY(DECLARE_BULK_REMOVE_ENTRY);

#define DECLARE_BULK_SET_ENTRY(OT,ot)                                                                                   \
sai_status_t RedisRemoteSaiInterface::bulkSet(                                                                          \
        _In_ uint32_t object_count,                                                                                     \
        _In_ const sai_ ## ot ## _t *ot,                                                                                \
        _In_ const sai_attribute_t *attr_list,                                                                          \
        _In_ sai_bulk_op_error_mode_t mode,                                                                             \
        _Out_ sai_status_t *object_statuses)                                                                            \
{                                                                                                                       \
    SWSS_LOG_ENTER();                                                                                                   \
    std::vector<std::string> serializedObjectIds;                                                                       \
    for (uint32_t idx = 0; idx < object_count; idx++)                                                                   \
    {                                                                                                                   \
        serializedObjectIds.emplace_back(sai_serialize_ ##ot (ot[idx]));                                                \
    }                                                                                                                   \
    return bulkSet((sai_object_type_t)SAI_OBJECT_TYPE_ ## OT, serializedObjectIds, attr_list, mode, object_statuses);   \
}

SAIREDIS_DECLARE_EVERY_BULK_ENTRY(DECLARE_BULK_SET_ENTRY);

// BULK GET

#define DECLARE_BULK_GET_ENTRY(OT,ot)                       \
sai_status_t RedisRemoteSaiInterface::bulkGet(              \
        _In_ uint32_t object_count,                         \
        _In_ const sai_ ## ot ## _t *ot,                    \
        _In_ const uint32_t *attr_count,                    \
        _Inout_ sai_attribute_t **attr_list,                \
        _In_ sai_bulk_op_error_mode_t mode,                 \
        _Out_ sai_status_t *object_statuses)                \
{                                                           \
    SWSS_LOG_ENTER();                                       \
    SWSS_LOG_ERROR("FIXME not implemented");                \
    return SAI_STATUS_NOT_IMPLEMENTED;                      \
}

SAIREDIS_DECLARE_EVERY_BULK_ENTRY(DECLARE_BULK_GET_ENTRY);


sai_status_t RedisRemoteSaiInterface::create(
        _In_ sai_object_type_t object_type,
        _In_ const std::string& serializedObjectId,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    auto entry = SaiAttributeList::serialize_attr_list(
            object_type,
            attr_count,
            attr_list,
            false);

    if (entry.empty())
    {
        // make sure that we put object into db
        // even if there are no attributes set
        swss::FieldValueTuple null("NULL", "NULL");

        entry.push_back(null);
    }

    auto serializedObjectType = sai_serialize_object_type(object_type);

    const std::string key = serializedObjectType + ":" + serializedObjectId;

    SWSS_LOG_DEBUG("generic create key: %s, fields: %" PRIu64, key.c_str(), entry.size());

    m_recorder->recordGenericCreate(key, entry);

    m_communicationChannel->set(key, entry, REDIS_ASIC_STATE_COMMAND_CREATE);

    auto status = waitForResponse(SAI_COMMON_API_CREATE);

    m_recorder->recordGenericCreateResponse(status);

    return status;
}

sai_status_t RedisRemoteSaiInterface::remove(
        _In_ sai_object_type_t objectType,
        _In_ const std::string& serializedObjectId)
{
    SWSS_LOG_ENTER();

    auto serializedObjectType = sai_serialize_object_type(objectType);

    const std::string key = serializedObjectType + ":" + serializedObjectId;

    SWSS_LOG_DEBUG("generic remove key: %s", key.c_str());

    m_recorder->recordGenericRemove(key);

    m_communicationChannel->del(key, REDIS_ASIC_STATE_COMMAND_REMOVE);

    auto status = waitForResponse(SAI_COMMON_API_REMOVE);

    m_recorder->recordGenericRemoveResponse(status);

    return status;
}

sai_status_t RedisRemoteSaiInterface::set(
        _In_ sai_object_type_t objectType,
        _In_ const std::string &serializedObjectId,
        _In_ const sai_attribute_t *attr)
{
    SWSS_LOG_ENTER();

    auto entry = SaiAttributeList::serialize_attr_list(
            objectType,
            1,
            attr,
            false);

    auto serializedObjectType = sai_serialize_object_type(objectType);

    std::string key = serializedObjectType + ":" + serializedObjectId;

    SWSS_LOG_DEBUG("generic set key: %s, fields: %lu", key.c_str(), entry.size());

    m_recorder->recordGenericSet(key, entry);

    m_communicationChannel->set(key, entry, REDIS_ASIC_STATE_COMMAND_SET);

    auto status = waitForResponse(SAI_COMMON_API_SET);

    m_recorder->recordGenericSetResponse(status);

    return status;
}

sai_status_t RedisRemoteSaiInterface::waitForResponse(
        _In_ sai_common_api_t api)
{
    SWSS_LOG_ENTER();

    if (m_syncMode)
    {
        swss::KeyOpFieldsValuesTuple kco;

        auto status = m_communicationChannel->wait(REDIS_ASIC_STATE_COMMAND_GETRESPONSE, kco);

        m_recorder->recordGenericResponse(status);

        return status;
    }

    /*
     * By default sync mode is disabled and all create/set/remove are
     * considered success operations.
     */

    return SAI_STATUS_SUCCESS;
}

sai_status_t RedisRemoteSaiInterface::waitForGetResponse(
        _In_ sai_object_type_t objectType,
        _In_ uint32_t attr_count,
        _Inout_ sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    swss::KeyOpFieldsValuesTuple kco;

    auto status = m_communicationChannel->wait(REDIS_ASIC_STATE_COMMAND_GETRESPONSE, kco);

    auto &values = kfvFieldsValues(kco);

    if (status == SAI_STATUS_SUCCESS)
    {
        if (values.size() == 0)
        {
            SWSS_LOG_THROW("logic error, get response returned 0 values!, send api response or sync/async issue?");
        }

        SaiAttributeList list(objectType, values, false);

        transfer_attributes(objectType, attr_count, list.get_attr_list(), attr_list, false);
    }
    else if (status == SAI_STATUS_BUFFER_OVERFLOW)
    {
        if (values.size() == 0)
        {
            SWSS_LOG_THROW("logic error, get response returned 0 values!, send api response or sync/async issue?");
        }

        SaiAttributeList list(objectType, values, true);

        // no need for id fix since this is overflow
        transfer_attributes(objectType, attr_count, list.get_attr_list(), attr_list, true);
    }

    return status;
}

sai_status_t RedisRemoteSaiInterface::get(
        _In_ sai_object_type_t objectType,
        _In_ const std::string& serializedObjectId,
        _In_ uint32_t attr_count,
        _Inout_ sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    /*
     * Since user may reuse buffers, then oid list buffers maybe not cleared
     * and contain some garbage, let's clean them so we send all oids as null to
     * syncd.
     */

    Utils::clearOidValues(objectType, attr_count, attr_list);

    auto entry = SaiAttributeList::serialize_attr_list(objectType, attr_count, attr_list, false);

    std::string serializedObjectType = sai_serialize_object_type(objectType);

    std::string key = serializedObjectType + ":" + serializedObjectId;

    SWSS_LOG_DEBUG("generic get key: %s, fields: %lu", key.c_str(), entry.size());

    bool record = !m_skipRecordAttrContainer->canSkipRecording(objectType, attr_count, attr_list);

    if (record)
    {
        m_recorder->recordGenericGet(key, entry);
    }

    // get is special, it will not put data
    // into asic view, only to message queue
    m_communicationChannel->set(key, entry, REDIS_ASIC_STATE_COMMAND_GET);

    auto status = waitForGetResponse(objectType, attr_count, attr_list);

    if (record)
    {
        m_recorder->recordGenericGetResponse(status, objectType, attr_count, attr_list);
    }

    return status;
}

#define DECLARE_GET_ENTRY(OT,ot)                                \
sai_status_t RedisRemoteSaiInterface::get(                      \
        _In_ const sai_ ## ot ## _t* ot,                        \
        _In_ uint32_t attr_count,                               \
        _Inout_ sai_attribute_t *attr_list)                     \
{                                                               \
    SWSS_LOG_ENTER();                                           \
    return get(                                                 \
            (sai_object_type_t)SAI_OBJECT_TYPE_ ## OT,          \
            sai_serialize_ ## ot(*ot),                          \
            attr_count,                                         \
            attr_list);                                         \
}

SAIREDIS_DECLARE_EVERY_ENTRY(DECLARE_GET_ENTRY);

sai_status_t RedisRemoteSaiInterface::waitForFlushFdbEntriesResponse()
{
    SWSS_LOG_ENTER();

    swss::KeyOpFieldsValuesTuple kco;

    auto status = m_communicationChannel->wait(REDIS_ASIC_STATE_COMMAND_FLUSHRESPONSE, kco);

    return status;
}

sai_status_t RedisRemoteSaiInterface::flushFdbEntries(
        _In_ sai_object_id_t switchId,
        _In_ uint32_t attrCount,
        _In_ const sai_attribute_t *attrList)
{
    SWSS_LOG_ENTER();

    auto entry = SaiAttributeList::serialize_attr_list(
            SAI_OBJECT_TYPE_FDB_FLUSH,
            attrCount,
            attrList,
            false);

    std::string serializedObjectId = sai_serialize_object_type(SAI_OBJECT_TYPE_FDB_FLUSH);

    // NOTE ! we actually give switch ID since FLUSH is not real object
    std::string key = serializedObjectId + ":" + sai_serialize_object_id(switchId);

    SWSS_LOG_NOTICE("flush key: %s, fields: %lu", key.c_str(), entry.size());

    m_recorder->recordFlushFdbEntries(switchId, attrCount, attrList);
   // TODO m_recorder->recordFlushFdbEntries(key, entry)

    m_communicationChannel->set(key, entry, REDIS_ASIC_STATE_COMMAND_FLUSH);

    auto status = waitForFlushFdbEntriesResponse();

    m_recorder->recordFlushFdbEntriesResponse(status);

    return status;
}

sai_status_t RedisRemoteSaiInterface::objectTypeGetAvailability(
        _In_ sai_object_id_t switchId,
        _In_ sai_object_type_t objectType,
        _In_ uint32_t attrCount,
        _In_ const sai_attribute_t *attrList,
        _Out_ uint64_t *count)
{
    SWSS_LOG_ENTER();

    auto strSwitchId = sai_serialize_object_id(switchId);

    auto entry = SaiAttributeList::serialize_attr_list(objectType, attrCount, attrList, false);

    entry.push_back(swss::FieldValueTuple("OBJECT_TYPE", sai_serialize_object_type(objectType)));

    SWSS_LOG_DEBUG(
            "Query arguments: switch: %s, attributes: %s",
            strSwitchId.c_str(),
            Globals::joinFieldValues(entry).c_str());

    // Syncd will pop this argument off before trying to deserialize the attribute list

    m_recorder->recordObjectTypeGetAvailability(switchId, objectType, attrCount, attrList);
    // recordObjectTypeGetAvailability(strSwitchId, entry);

    // This query will not put any data into the ASIC view, just into the
    // message queue
    m_communicationChannel->set(strSwitchId, entry, REDIS_ASIC_STATE_COMMAND_OBJECT_TYPE_GET_AVAILABILITY_QUERY);

    auto status = waitForObjectTypeGetAvailabilityResponse(count);

    m_recorder->recordObjectTypeGetAvailabilityResponse(status, count);

    return status;
}

sai_status_t RedisRemoteSaiInterface::waitForObjectTypeGetAvailabilityResponse(
        _Inout_ uint64_t *count)
{
    SWSS_LOG_ENTER();

    swss::KeyOpFieldsValuesTuple kco;

    auto status = m_communicationChannel->wait(REDIS_ASIC_STATE_COMMAND_OBJECT_TYPE_GET_AVAILABILITY_RESPONSE, kco);

    if (status == SAI_STATUS_SUCCESS)
    {
        auto &values = kfvFieldsValues(kco);

        if (values.size() != 1)
        {
            SWSS_LOG_THROW("Invalid response from syncd: expected 1 value, received %zu", values.size());
        }

        const std::string &availability_str = fvValue(values[0]);

        *count = std::stoull(availability_str);

        SWSS_LOG_DEBUG("Received payload: count = %lu", *count);
    }

    return status;
}

sai_status_t RedisRemoteSaiInterface::queryAttributeCapability(
        _In_ sai_object_id_t switchId,
        _In_ sai_object_type_t objectType,
        _In_ sai_attr_id_t attrId,
        _Out_ sai_attr_capability_t *capability)
{
    SWSS_LOG_ENTER();

    auto switchIdStr = sai_serialize_object_id(switchId);
    auto objectTypeStr = sai_serialize_object_type(objectType);

    auto meta = sai_metadata_get_attr_metadata(objectType, attrId);

    if (meta == NULL)
    {
        SWSS_LOG_ERROR("Failed to find attribute metadata: object type %s, attr id %d", objectTypeStr.c_str(), attrId);
        return SAI_STATUS_INVALID_PARAMETER;
    }

    const std::string attrIdStr = meta->attridname;

    const std::vector<swss::FieldValueTuple> entry =
    {
        swss::FieldValueTuple("OBJECT_TYPE", objectTypeStr),
        swss::FieldValueTuple("ATTR_ID", attrIdStr)
    };

    SWSS_LOG_DEBUG(
            "Query arguments: switch %s, object type: %s, attribute: %s",
            switchIdStr.c_str(),
            objectTypeStr.c_str(),
            attrIdStr.c_str()
    );

    // This query will not put any data into the ASIC view, just into the
    // message queue

    m_recorder->recordQueryAttributeCapability(switchId, objectType, attrId, capability);

    m_communicationChannel->set(switchIdStr, entry, REDIS_ASIC_STATE_COMMAND_ATTR_CAPABILITY_QUERY);

    auto status = waitForQueryAttributeCapabilityResponse(capability);

    m_recorder->recordQueryAttributeCapabilityResponse(status, objectType, attrId, capability);

    return status;
}

sai_status_t RedisRemoteSaiInterface::waitForQueryAttributeCapabilityResponse(
        _Out_ sai_attr_capability_t* capability)
{
    SWSS_LOG_ENTER();

    swss::KeyOpFieldsValuesTuple kco;

    auto status = m_communicationChannel->wait(REDIS_ASIC_STATE_COMMAND_ATTR_CAPABILITY_RESPONSE, kco);

    if (status == SAI_STATUS_SUCCESS)
    {
        const std::vector<swss::FieldValueTuple> &values = kfvFieldsValues(kco);

        if (values.size() != 3)
        {
            SWSS_LOG_ERROR("Invalid response from syncd: expected 3 values, received %zu", values.size());

            return SAI_STATUS_FAILURE;
        }

        capability->create_implemented = (fvValue(values[0]) == "true" ? true : false);
        capability->set_implemented    = (fvValue(values[1]) == "true" ? true : false);
        capability->get_implemented    = (fvValue(values[2]) == "true" ? true : false);

        SWSS_LOG_DEBUG("Received payload: create_implemented:%s, set_implemented:%s, get_implemented:%s",
            (capability->create_implemented? "true":"false"), (capability->set_implemented? "true":"false"), (capability->get_implemented? "true":"false"));
    }

    return status;
}

sai_status_t RedisRemoteSaiInterface::queryAttributeEnumValuesCapability(
        _In_ sai_object_id_t switchId,
        _In_ sai_object_type_t objectType,
        _In_ sai_attr_id_t attrId,
        _Inout_ sai_s32_list_t *enumValuesCapability)
{
    SWSS_LOG_ENTER();

    if (enumValuesCapability && enumValuesCapability->list)
    {
        // clear input list, since we use serialize to transfer values
        for (uint32_t idx = 0; idx < enumValuesCapability->count; idx++)
            enumValuesCapability->list[idx] = 0;
    }

    auto switch_id_str = sai_serialize_object_id(switchId);
    auto object_type_str = sai_serialize_object_type(objectType);

    auto meta = sai_metadata_get_attr_metadata(objectType, attrId);

    if (meta == NULL)
    {
        SWSS_LOG_ERROR("Failed to find attribute metadata: object type %s, attr id %d", object_type_str.c_str(), attrId);
        return SAI_STATUS_INVALID_PARAMETER;
    }

    const std::string attr_id_str = meta->attridname;
    const std::string list_size = std::to_string(enumValuesCapability->count);

    const std::vector<swss::FieldValueTuple> entry =
    {
        swss::FieldValueTuple("OBJECT_TYPE", object_type_str),
        swss::FieldValueTuple("ATTR_ID", attr_id_str),
        swss::FieldValueTuple("LIST_SIZE", list_size)
    };

    SWSS_LOG_DEBUG(
            "Query arguments: switch %s, object type: %s, attribute: %s, count: %s",
            switch_id_str.c_str(),
            object_type_str.c_str(),
            attr_id_str.c_str(),
            list_size.c_str()
    );

    // This query will not put any data into the ASIC view, just into the
    // message queue

    m_recorder->recordQueryAttributeEnumValuesCapability(switchId, objectType, attrId, enumValuesCapability);

    m_communicationChannel->set(switch_id_str, entry, REDIS_ASIC_STATE_COMMAND_ATTR_ENUM_VALUES_CAPABILITY_QUERY);

    auto status = waitForQueryAttributeEnumValuesCapabilityResponse(enumValuesCapability);

    m_recorder->recordQueryAttributeEnumValuesCapabilityResponse(status, objectType, attrId, enumValuesCapability);

    return status;
}

sai_status_t RedisRemoteSaiInterface::waitForQueryAttributeEnumValuesCapabilityResponse(
        _Inout_ sai_s32_list_t* enumValuesCapability)
{
    SWSS_LOG_ENTER();

    swss::KeyOpFieldsValuesTuple kco;

    auto status = m_communicationChannel->wait(REDIS_ASIC_STATE_COMMAND_ATTR_ENUM_VALUES_CAPABILITY_RESPONSE, kco);

    if (status == SAI_STATUS_SUCCESS)
    {
        const std::vector<swss::FieldValueTuple> &values = kfvFieldsValues(kco);

        if (values.size() != 2)
        {
            SWSS_LOG_ERROR("Invalid response from syncd: expected 2 values, received %zu", values.size());

            return SAI_STATUS_FAILURE;
        }

        const std::string &capability_str = fvValue(values[0]);
        const uint32_t num_capabilities = std::stoi(fvValue(values[1]));

        SWSS_LOG_DEBUG("Received payload: capabilities = '%s', count = %d", capability_str.c_str(), num_capabilities);

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
        const std::vector<swss::FieldValueTuple> &values = kfvFieldsValues(kco);

        if (values.size() != 1)
        {
            SWSS_LOG_ERROR("Invalid response from syncd: expected 1 value, received %zu", values.size());

            return SAI_STATUS_FAILURE;
        }

        const uint32_t num_capabilities = std::stoi(fvValue(values[0]));

        SWSS_LOG_DEBUG("Received payload: count = %u", num_capabilities);

        enumValuesCapability->count = num_capabilities;
    }

    return status;
}

sai_status_t RedisRemoteSaiInterface::getStats(
        _In_ sai_object_type_t object_type,
        _In_ sai_object_id_t object_id,
        _In_ uint32_t number_of_counters,
        _In_ const sai_stat_id_t *counter_ids,
        _Out_ uint64_t *counters)
{
    SWSS_LOG_ENTER();

    auto stats_enum = sai_metadata_get_object_type_info(object_type)->statenum;

    auto entry = serialize_counter_id_list(stats_enum, number_of_counters, counter_ids);

    std::string str_object_type = sai_serialize_object_type(object_type);

    std::string key = str_object_type + ":" + sai_serialize_object_id(object_id);

    SWSS_LOG_DEBUG("generic get stats key: %s, fields: %lu", key.c_str(), entry.size());

    // get_stats will not put data to asic view, only to message queue

    m_communicationChannel->set(key, entry, REDIS_ASIC_STATE_COMMAND_GET_STATS);

    return waitForGetStatsResponse(number_of_counters, counters);
}

sai_status_t RedisRemoteSaiInterface::waitForGetStatsResponse(
        _In_ uint32_t number_of_counters,
        _Out_ uint64_t *counters)
{
    SWSS_LOG_ENTER();

    swss::KeyOpFieldsValuesTuple kco;

    auto status = m_communicationChannel->wait(REDIS_ASIC_STATE_COMMAND_GETRESPONSE, kco);

    if (status == SAI_STATUS_SUCCESS)
    {
        auto &values = kfvFieldsValues(kco);

        if (values.size () != number_of_counters)
        {
            SWSS_LOG_THROW("wrong number of counters, got %zu, expected %u", values.size(), number_of_counters);
        }

        for (uint32_t idx = 0; idx < number_of_counters; idx++)
        {
            counters[idx] = stoull(fvValue(values[idx]));
        }
    }

    return status;
}

sai_status_t RedisRemoteSaiInterface::queryStatsCapability(
        _In_ sai_object_id_t switchId,
        _In_ sai_object_type_t objectType,
        _Inout_ sai_stat_capability_list_t *stats_capability)
{
    SWSS_LOG_ENTER();

    auto switchIdStr = sai_serialize_object_id(switchId);
    auto objectTypeStr = sai_serialize_object_type(objectType);

    if (stats_capability == NULL)
    {
        SWSS_LOG_ERROR("Failed to find stats-capability: switch %s, object type %s", switchIdStr.c_str(), objectTypeStr.c_str());
        return SAI_STATUS_INVALID_PARAMETER;
    }

    if (stats_capability && stats_capability->list && (stats_capability->count))
    {
        // clear input list, since we use serialize to transfer the values
        for (uint32_t idx = 0; idx < stats_capability->count; idx++)
	{
            stats_capability->list[idx].stat_enum = 0;
            stats_capability->list[idx].stat_modes = 0;
	}
    }

    const std::string listSize = std::to_string(stats_capability->count);

    const std::vector<swss::FieldValueTuple> entry =
    {
        swss::FieldValueTuple("OBJECT_TYPE", objectTypeStr),
	swss::FieldValueTuple("LIST_SIZE", listSize)
    };

    SWSS_LOG_DEBUG(
            "Query arguments: switch %s, object type: %s, count: %s",
            switchIdStr.c_str(),
            objectTypeStr.c_str(),
            listSize.c_str()
    );

    // This query will not put any data into the ASIC view, just into the
    // message queue

    m_recorder->recordQueryStatsCapability(switchId, objectType, stats_capability);

    m_communicationChannel->set(switchIdStr, entry, REDIS_ASIC_STATE_COMMAND_STATS_CAPABILITY_QUERY);

    auto status = waitForQueryStatsCapabilityResponse(stats_capability);

    m_recorder->recordQueryStatsCapabilityResponse(status, objectType, stats_capability);

    return status;
}


sai_status_t RedisRemoteSaiInterface::queryStatsStCapability(
    _In_ sai_object_id_t switchId,
    _In_ sai_object_type_t objectType,
    _Inout_ sai_stat_st_capability_list_t *stats_capability)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_NOT_IMPLEMENTED;
}

sai_status_t RedisRemoteSaiInterface::waitForQueryStatsCapabilityResponse(
        _Inout_ sai_stat_capability_list_t* stats_capability)
{
    SWSS_LOG_ENTER();

    swss::KeyOpFieldsValuesTuple kco;

    auto status = m_communicationChannel->wait(REDIS_ASIC_STATE_COMMAND_STATS_CAPABILITY_RESPONSE, kco);

    if (status == SAI_STATUS_SUCCESS)
    {
        const std::vector<swss::FieldValueTuple> &values = kfvFieldsValues(kco);

        if (values.size() != 3)
        {
            SWSS_LOG_ERROR("Invalid response from syncd: expected 3 value, received %zu", values.size());

            return SAI_STATUS_FAILURE;
        }

        const std::string &stat_enum_str = fvValue(values[0]);
        const std::string &stat_modes_str = fvValue(values[1]);
        const uint32_t num_capabilities = std::stoi(fvValue(values[2]));

        SWSS_LOG_DEBUG("Received payload: stat_enums = '%s', stat_modes = '%s', count = %d",
                       stat_enum_str.c_str(), stat_modes_str.c_str(), num_capabilities);

        stats_capability->count = num_capabilities;

        sai_deserialize_stats_capability_list(stats_capability, stat_enum_str, stat_modes_str);
    }
    else if (status ==  SAI_STATUS_BUFFER_OVERFLOW)
    {
        const std::vector<swss::FieldValueTuple> &values = kfvFieldsValues(kco);

        if (values.size() != 1)
        {
            SWSS_LOG_ERROR("Invalid response from syncd: expected 1 value, received %zu", values.size());

            return SAI_STATUS_FAILURE;
        }

        const uint32_t num_capabilities = std::stoi(fvValue(values[0]));

        SWSS_LOG_DEBUG("Received payload: count = %u", num_capabilities);

        stats_capability->count = num_capabilities;
    }

    return status;
}

sai_status_t RedisRemoteSaiInterface::getStatsExt(
        _In_ sai_object_type_t object_type,
        _In_ sai_object_id_t object_id,
        _In_ uint32_t number_of_counters,
        _In_ const sai_stat_id_t *counter_ids,
        _In_ sai_stats_mode_t mode,
        _Out_ uint64_t *counters)
{
    SWSS_LOG_ENTER();

    SWSS_LOG_ERROR("not implemented");

    // TODO could be the same as getStats but put mode at first argument

    return SAI_STATUS_NOT_IMPLEMENTED;
}

sai_status_t RedisRemoteSaiInterface::clearStats(
        _In_ sai_object_type_t object_type,
        _In_ sai_object_id_t object_id,
        _In_ uint32_t number_of_counters,
        _In_ const sai_stat_id_t *counter_ids)
{
    SWSS_LOG_ENTER();

    auto stats_enum = sai_metadata_get_object_type_info(object_type)->statenum;

    auto values = serialize_counter_id_list(stats_enum, number_of_counters, counter_ids);

    auto str_object_type = sai_serialize_object_type(object_type);

    auto key = str_object_type + ":" + sai_serialize_object_id(object_id);

    SWSS_LOG_DEBUG("generic clear stats key: %s, fields: %lu", key.c_str(), values.size());

    // clear_stats will not put data into asic view, only to message queue

    m_recorder->recordGenericClearStats(object_type, object_id, number_of_counters, counter_ids);

    m_communicationChannel->set(key, values, REDIS_ASIC_STATE_COMMAND_CLEAR_STATS);

    auto status = waitForClearStatsResponse();

    m_recorder->recordGenericClearStatsResponse(status);

    return status;
}

sai_status_t RedisRemoteSaiInterface::bulkGetStats(
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

    SWSS_LOG_ERROR("not implemented");

    return SAI_STATUS_NOT_IMPLEMENTED;
}

sai_status_t RedisRemoteSaiInterface::bulkClearStats(
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

    SWSS_LOG_ERROR("not implemented");

    return SAI_STATUS_NOT_IMPLEMENTED;
}

sai_status_t RedisRemoteSaiInterface::waitForClearStatsResponse()
{
    SWSS_LOG_ENTER();

    swss::KeyOpFieldsValuesTuple kco;

    auto status = m_communicationChannel->wait(REDIS_ASIC_STATE_COMMAND_GETRESPONSE, kco);

    return status;
}

sai_status_t RedisRemoteSaiInterface::bulkRemove(
        _In_ sai_object_type_t object_type,
        _In_ const std::vector<std::string> &serialized_object_ids,
        _In_ sai_bulk_op_error_mode_t mode,
        _Out_ sai_status_t *object_statuses)
{
    SWSS_LOG_ENTER();

    // TODO support mode, this will need to go as extra parameter and needs to
    // be supported by LUA script passed as first or last entry in values,
    // currently mode is ignored

    std::string serializedObjectType = sai_serialize_object_type(object_type);

    std::vector<swss::FieldValueTuple> entries;

    for (size_t idx = 0; idx < serialized_object_ids.size(); ++idx)
    {
        std::string str_attr = "";

        swss::FieldValueTuple fvtNoStatus(serialized_object_ids[idx], str_attr);

        entries.push_back(fvtNoStatus);
    }

    /*
     * We are adding number of entries to actually add ':' to be compatible
     * with previous
     */

    // key:         object_type:count
    // field:       object_id
    // value:       object_attrs
    std::string key = serializedObjectType + ":" + std::to_string(entries.size());

    m_recorder->recordBulkGenericRemove(serializedObjectType, entries);

    m_communicationChannel->set(key, entries, REDIS_ASIC_STATE_COMMAND_BULK_REMOVE);

    return waitForBulkResponse(SAI_COMMON_API_BULK_REMOVE, (uint32_t)serialized_object_ids.size(), object_statuses);
}

sai_status_t RedisRemoteSaiInterface::waitForBulkResponse(
        _In_ sai_common_api_t api,
        _In_ uint32_t object_count,
        _Out_ sai_status_t *object_statuses)
{
    SWSS_LOG_ENTER();

    if (m_syncMode)
    {
        swss::KeyOpFieldsValuesTuple kco;

        auto status = m_communicationChannel->wait(REDIS_ASIC_STATE_COMMAND_GETRESPONSE, kco);

        auto &values = kfvFieldsValues(kco);

        if (values.size () != object_count)
        {
            SWSS_LOG_THROW("wrong number of statuses, got %zu, expected %u", values.size(), object_count);
        }

        // deserialize statuses for all objects

        for (uint32_t idx = 0; idx < object_count; idx++)
        {
            sai_deserialize_status(fvField(values[idx]), object_statuses[idx]);
        }

        m_recorder->recordBulkGenericResponse(status, object_count, object_statuses);

        return status;
    }

    /*
     * By default sync mode is disabled and all bulk create/set/remove are
     * considered success operations.
     */

    for (uint32_t idx = 0; idx < object_count; idx++)
    {
        object_statuses[idx] = SAI_STATUS_SUCCESS;
    }

    return SAI_STATUS_SUCCESS;
}

sai_status_t RedisRemoteSaiInterface::waitForBulkGetResponse(
        _In_ sai_object_type_t objectType,
        _In_ uint32_t object_count,
        _In_ const uint32_t *attr_count,
        _Inout_ sai_attribute_t **attr_list,
        _Out_ sai_status_t *object_statuses)
{
    SWSS_LOG_ENTER();

    swss::KeyOpFieldsValuesTuple kco;

    const auto status = m_communicationChannel->wait(REDIS_ASIC_STATE_COMMAND_GETRESPONSE, kco);

    const auto &values = kfvFieldsValues(kco);

    if (values.size() != object_count)
    {
        SWSS_LOG_THROW("wrong number of statuses, got %zu, expected %u", values.size(), object_count);
    }

    for (size_t idx = 0; idx < values.size(); idx++)
    {
        // field = status
        // value = attrid=attrvalue|...

        const auto& statusStr = fvField(values[idx]);
        const auto& joined = fvValue(values[idx]);

        const auto v = swss::tokenize(joined, '|');

        std::vector<swss::FieldValueTuple> entries; // attributes per object id
        entries.reserve(v.size());

        for (size_t i = 0; i < v.size(); i++)
        {
            const std::string item = v.at(i);

            auto start = item.find_first_of("=");

            auto field = item.substr(0, start);
            auto value = item.substr(start + 1);

            entries.emplace_back(field, value);
        }

        // deserialize statuses for all objects
        sai_deserialize_status(statusStr, object_statuses[idx]);

        const auto objectStatus = object_statuses[idx];

        if (objectStatus == SAI_STATUS_SUCCESS || objectStatus == SAI_STATUS_BUFFER_OVERFLOW)
        {
            const auto countOnly = (objectStatus == SAI_STATUS_BUFFER_OVERFLOW);

            if (values.size() == 0)
            {
                SWSS_LOG_THROW("logic error, get response returned 0 values!, send api response or sync/async issue?");
            }

            SaiAttributeList list(objectType, entries, countOnly);

            // no need for id fix since this is overflow
            transfer_attributes(objectType, attr_count[idx], list.get_attr_list(), attr_list[idx], countOnly);
        }
    }

    m_recorder->recordBulkGenericGetResponse(status, values);

    return status;
}

sai_status_t RedisRemoteSaiInterface::bulkRemove(
        _In_ sai_object_type_t object_type,
        _In_ uint32_t object_count,
        _In_ const sai_object_id_t *object_id,
        _In_ sai_bulk_op_error_mode_t mode,
        _Out_ sai_status_t *object_statuses)
{
    SWSS_LOG_ENTER();

    std::vector<std::string> serializedObjectIds;

    for (uint32_t idx = 0; idx < object_count; idx++)
    {
        serializedObjectIds.emplace_back(sai_serialize_object_id(object_id[idx]));
    }

    return bulkRemove(object_type, serializedObjectIds, mode, object_statuses);
}

sai_status_t RedisRemoteSaiInterface::bulkSet(
        _In_ sai_object_type_t object_type,
        _In_ uint32_t object_count,
        _In_ const sai_object_id_t *object_id,
        _In_ const sai_attribute_t *attr_list,
        _In_ sai_bulk_op_error_mode_t mode,
        _Out_ sai_status_t *object_statuses)
{
    SWSS_LOG_ENTER();

    std::vector<std::string> serializedObjectIds;

    for (uint32_t idx = 0; idx < object_count; idx++)
    {
        serializedObjectIds.emplace_back(sai_serialize_object_id(object_id[idx]));
    }

    return bulkSet(object_type, serializedObjectIds, attr_list, mode, object_statuses);
}

sai_status_t RedisRemoteSaiInterface::bulkSet(
        _In_ sai_object_type_t object_type,
        _In_ const std::vector<std::string> &serialized_object_ids,
        _In_ const sai_attribute_t *attr_list,
        _In_ sai_bulk_op_error_mode_t mode,
        _Out_ sai_status_t *object_statuses)
{
    SWSS_LOG_ENTER();

    // TODO support mode

    std::vector<swss::FieldValueTuple> entries;

    for (size_t idx = 0; idx < serialized_object_ids.size(); ++idx)
    {
        auto entry = SaiAttributeList::serialize_attr_list(object_type, 1, &attr_list[idx], false);

        std::string str_attr = Globals::joinFieldValues(entry);

        swss::FieldValueTuple value(serialized_object_ids[idx], str_attr);

        entries.push_back(value);
    }

    /*
     * We are adding number of entries to actually add ':' to be compatible
     * with previous
     */

    auto serializedObjectType = sai_serialize_object_type(object_type);

    std::string key = serializedObjectType + ":" + std::to_string(entries.size());

    m_recorder->recordBulkGenericSet(serializedObjectType, entries);

    m_communicationChannel->set(key, entries, REDIS_ASIC_STATE_COMMAND_BULK_SET);

    return waitForBulkResponse(SAI_COMMON_API_BULK_SET, (uint32_t)serialized_object_ids.size(), object_statuses);
}

sai_status_t RedisRemoteSaiInterface::bulkGet(
        _In_ sai_object_type_t object_type,
        _In_ uint32_t object_count,
        _In_ const sai_object_id_t *object_id,
        _In_ const uint32_t *attr_count,
        _Inout_ sai_attribute_t **attr_list,
        _In_ sai_bulk_op_error_mode_t mode,
        _Out_ sai_status_t *object_statuses)
{
    SWSS_LOG_ENTER();

    std::vector<std::string> serializedObjectIds;
    serializedObjectIds.reserve(object_count);

    for (uint32_t idx = 0; idx < object_count; idx++)
    {
        serializedObjectIds.emplace_back(sai_serialize_object_id(object_id[idx]));
    }

    return bulkGet(object_type, serializedObjectIds, attr_count, attr_list, mode, object_statuses);
}

sai_status_t RedisRemoteSaiInterface::bulkGet(
        _In_ sai_object_type_t object_type,
        _In_ const std::vector<std::string> &serialized_object_ids,
        _In_ const uint32_t *attr_count,
        _Inout_ sai_attribute_t **attr_list,
        _In_ sai_bulk_op_error_mode_t mode,
        _Inout_ sai_status_t *object_statuses)
{
    SWSS_LOG_ENTER();

    const auto serializedObjectType = sai_serialize_object_type(object_type);

    std::vector<swss::FieldValueTuple> entries;
    entries.reserve(serialized_object_ids.size());

    for (size_t idx = 0; idx < serialized_object_ids.size(); idx++)
    {
        /*
        * Since user may reuse buffers, then oid list buffers maybe not cleared
        * and contain some garbage, let's clean them so we send all oids as null to
        * syncd.
        */

        Utils::clearOidValues(object_type, attr_count[idx], attr_list[idx]);

        const auto entry = SaiAttributeList::serialize_attr_list(object_type, attr_count[idx], attr_list[idx], false);

        const auto strAttr = Globals::joinFieldValues(entry);

        swss::FieldValueTuple fvt(serialized_object_ids[idx] , strAttr);

        entries.push_back(fvt);
    }

    /*
     * We are adding number of entries to actually add ':' to be compatible
     * with previous
     */

    const auto key = serializedObjectType + ":" + std::to_string(entries.size());

    m_communicationChannel->set(key, entries, REDIS_ASIC_STATE_COMMAND_BULK_GET);

    m_recorder->recordBulkGenericGet(serializedObjectType, entries);

    const auto object_count = static_cast<uint32_t>(serialized_object_ids.size());

    const auto status = waitForBulkGetResponse(object_type, object_count, attr_count, attr_list, object_statuses);

    return status;
}

sai_status_t RedisRemoteSaiInterface::bulkCreate(
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

    // TODO support mode

    for (uint32_t idx = 0; idx < object_count; idx++)
    {
        object_id[idx] = m_virtualObjectIdManager->allocateNewObjectId(object_type, switch_id);

        if (object_id[idx] == SAI_NULL_OBJECT_ID)
        {
            SWSS_LOG_ERROR("failed to create %s, with switch id: %s",
                    sai_serialize_object_type(object_type).c_str(),
                    sai_serialize_object_id(switch_id).c_str());

            return SAI_STATUS_INSUFFICIENT_RESOURCES;
        }
    }

    std::vector<std::string> serialized_object_ids;

    // on create vid is put in db by syncd
    for (uint32_t idx = 0; idx < object_count; idx++)
    {
        std::string str_object_id = sai_serialize_object_id(object_id[idx]);
        serialized_object_ids.push_back(str_object_id);
    }

    return bulkCreate(
            object_type,
            serialized_object_ids,
            attr_count,
            attr_list,
            mode,
            object_statuses);
}

sai_status_t RedisRemoteSaiInterface::bulkCreate(
        _In_ sai_object_type_t object_type,
        _In_ const std::vector<std::string> &serialized_object_ids,
        _In_ const uint32_t *attr_count,
        _In_ const sai_attribute_t **attr_list,
        _In_ sai_bulk_op_error_mode_t mode,
        _Inout_ sai_status_t *object_statuses)
{
    SWSS_LOG_ENTER();

    // TODO support mode

    std::string str_object_type = sai_serialize_object_type(object_type);

    std::vector<swss::FieldValueTuple> entries;

    for (size_t idx = 0; idx < serialized_object_ids.size(); ++idx)
    {
        auto entry = SaiAttributeList::serialize_attr_list(object_type, attr_count[idx], attr_list[idx], false);

        if (entry.empty())
        {
            // make sure that we put object into db
            // even if there are no attributes set
            swss::FieldValueTuple null("NULL", "NULL");

            entry.push_back(null);
        }

        std::string str_attr = Globals::joinFieldValues(entry);

        swss::FieldValueTuple fvtNoStatus(serialized_object_ids[idx] , str_attr);

        entries.push_back(fvtNoStatus);
    }

    /*
     * We are adding number of entries to actually add ':' to be compatible
     * with previous
     */

    // key:         object_type:count
    // field:       object_id
    // value:       object_attrs
    std::string key = str_object_type + ":" + std::to_string(entries.size());

    m_recorder->recordBulkGenericCreate(str_object_type, entries);

    m_communicationChannel->set(key, entries, REDIS_ASIC_STATE_COMMAND_BULK_CREATE);

    return waitForBulkResponse(SAI_COMMON_API_BULK_CREATE, (uint32_t)serialized_object_ids.size(), object_statuses);
}

sai_status_t RedisRemoteSaiInterface::notifySyncd(
        _In_ sai_object_id_t switchId,
        _In_ sai_redis_notify_syncd_t redisNotifySyncd)
{
    SWSS_LOG_ENTER();

    std::vector<swss::FieldValueTuple> entry;

    auto key = sai_serialize(redisNotifySyncd);

    SWSS_LOG_NOTICE("sending syncd: %s", key.c_str());

    // we need to use "GET" channel to be sure that
    // all previous operations were applied, if we don't
    // use GET channel then we may hit race condition
    // on syncd side where syncd will start compare view
    // when there are still objects in op queue
    //
    // other solution can be to use notify event
    // and then on syncd side read all the asic state queue
    // and apply changes before switching to init/apply mode

    m_recorder->recordNotifySyncd(switchId, redisNotifySyncd);

    m_communicationChannel->set(key, entry, REDIS_ASIC_STATE_COMMAND_NOTIFY);

    auto status = waitForNotifySyncdResponse();

    m_recorder->recordNotifySyncdResponse(status);

    return status;
}

sai_status_t RedisRemoteSaiInterface::waitForNotifySyncdResponse()
{
    SWSS_LOG_ENTER();

    swss::KeyOpFieldsValuesTuple kco;

    auto status = m_communicationChannel->wait(REDIS_ASIC_STATE_COMMAND_NOTIFY, kco);

    return status;
}

bool RedisRemoteSaiInterface::isRedisAttribute(
        _In_ sai_object_id_t objectType,
        _In_ const sai_attribute_t* attr)
{
    SWSS_LOG_ENTER();

    if ((objectType != SAI_OBJECT_TYPE_SWITCH) || (attr == nullptr) || (attr->id < SAI_SWITCH_ATTR_CUSTOM_RANGE_START))
    {
        return false;
    }

    return true;
}

void RedisRemoteSaiInterface::handleNotification(
        _In_ const std::string &name,
        _In_ const std::string &serializedNotification,
        _In_ const std::vector<swss::FieldValueTuple> &values)
{
    SWSS_LOG_ENTER();

    // TODO to pass switch_id for every notification we could add it to values
    // at syncd side
    //
    // Each global context (syncd) will have it's own notification thread
    // handler, so we will know at which context notification arrived, but we
    // also need to know at which switch id generated this notification. For
    // that we will assign separate notification handlers in syncd itself, and
    // each of those notifications will know to which switch id it belongs.
    // Then later we could also check whether oids in notification actually
    // belongs to given switch id.  This way we could find vendor bugs like
    // sending notifications from one switch to another switch handler.
    //
    // But before that we will extract switch id from notification itself.

    // TODO record should also be under api mutex, all other apis are

    m_recorder->recordNotification(name, serializedNotification, values);

    auto notification = NotificationFactory::deserialize(name, serializedNotification);

    if (notification)
    {
        auto sn = m_notificationCallback(notification); // will be synchronized to api mutex

        // execute callback from notification thread

        notification->executeCallback(sn);
    }
}

sai_object_type_t RedisRemoteSaiInterface::objectTypeQuery(
        _In_ sai_object_id_t objectId)
{
    SWSS_LOG_ENTER();

    return m_virtualObjectIdManager->saiObjectTypeQuery(objectId);
}

sai_object_id_t RedisRemoteSaiInterface::switchIdQuery(
        _In_ sai_object_id_t objectId)
{
    SWSS_LOG_ENTER();

    return m_virtualObjectIdManager->saiSwitchIdQuery(objectId);
}

sai_status_t RedisRemoteSaiInterface::logSet(
        _In_ sai_api_t api,
        _In_ sai_log_level_t log_level)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_SUCCESS;
}

sai_status_t RedisRemoteSaiInterface::queryApiVersion(
        _Out_ sai_api_version_t *version)
{
    SWSS_LOG_ENTER();

    if (version)
    {
        *version = SAI_API_VERSION;

        // TODO FIXME implement proper query for syncd, currently this is not an issue since swss is not using this API

        SWSS_LOG_WARN("retruning SAI API version %d with sairedis compiled SAI headers, not actual libsai.so", SAI_API_VERSION);

        return SAI_STATUS_SUCCESS;
    }

    SWSS_LOG_ERROR("version parameter is NULL");

    return SAI_STATUS_INVALID_PARAMETER;
}

sai_status_t RedisRemoteSaiInterface::sai_redis_notify_syncd(
        _In_ sai_object_id_t switchId,
        _In_ const sai_attribute_t *attr)
{
    SWSS_LOG_ENTER();

    auto redisNotifySyncd = (sai_redis_notify_syncd_t)attr->value.s32;

    switch (redisNotifySyncd)
    {
        case SAI_REDIS_NOTIFY_SYNCD_INIT_VIEW:
        case SAI_REDIS_NOTIFY_SYNCD_APPLY_VIEW:
        case SAI_REDIS_NOTIFY_SYNCD_INSPECT_ASIC:
        case SAI_REDIS_NOTIFY_SYNCD_INVOKE_DUMP:
            break;

        default:

            SWSS_LOG_ERROR("invalid notify syncd attr value %s", sai_serialize(redisNotifySyncd).c_str());

            return SAI_STATUS_FAILURE;
    }

    auto status = notifySyncd(switchId, redisNotifySyncd);

    if (status == SAI_STATUS_SUCCESS)
    {
        switch (redisNotifySyncd)
        {
            case SAI_REDIS_NOTIFY_SYNCD_INIT_VIEW:

                SWSS_LOG_NOTICE("switched ASIC to INIT VIEW");

                m_asicInitViewMode = true;

                SWSS_LOG_NOTICE("clearing current local state since init view is called on initialized switch");

                clear_local_state();

                break;

            case SAI_REDIS_NOTIFY_SYNCD_APPLY_VIEW:

                SWSS_LOG_NOTICE("switched ASIC to APPLY VIEW");

                m_asicInitViewMode = false;

                break;

            case SAI_REDIS_NOTIFY_SYNCD_INSPECT_ASIC:

                SWSS_LOG_NOTICE("inspect ASIC SUCCEEDED");

                break;

            case SAI_REDIS_NOTIFY_SYNCD_INVOKE_DUMP:

                SWSS_LOG_NOTICE("invoked DUMP succeeded");

                break;

            default:
                break;
        }
    }

    return status;
}

void RedisRemoteSaiInterface::clear_local_state()
{
    SWSS_LOG_ENTER();

    SWSS_LOG_NOTICE("clearing local state");

    // Will need to be executed after init VIEW

    // will clear switch container
    m_switchContainer = std::make_shared<SwitchContainer>();

    m_virtualObjectIdManager =
        std::make_shared<VirtualObjectIdManager>(
                m_contextConfig->m_guid,
                m_contextConfig->m_scc,
                m_redisVidIndexGenerator);

    auto meta = m_meta.lock();

    if (meta)
    {
        meta->meta_init_db();
    }
}

void RedisRemoteSaiInterface::setMeta(
        _In_ std::weak_ptr<saimeta::Meta> meta)
{
    SWSS_LOG_ENTER();

    m_meta = meta;
}

sai_switch_notifications_t RedisRemoteSaiInterface::syncProcessNotification(
        _In_ std::shared_ptr<Notification> notification)
{
    SWSS_LOG_ENTER();

    // NOTE: process metadata must be executed under sairedis API mutex since
    // it will access meta database and notification comes from different
    // thread, and this method is executed from notifications thread

    auto meta = m_meta.lock();

    if (!meta)
    {
        SWSS_LOG_WARN("meta pointer expired");

        return { };
    }

    notification->processMetadata(meta);

    auto objectId = notification->getAnyObjectId();

    auto switchId = m_virtualObjectIdManager->saiSwitchIdQuery(objectId);

    auto sw = m_switchContainer->getSwitch(switchId);

    if (sw)
    {
        return sw->getSwitchNotifications(); // explicit copy
    }

    SWSS_LOG_WARN("switch %s not present in container, returning empty switch notifications",
            sai_serialize_object_id(switchId).c_str());

    return { };
}

bool RedisRemoteSaiInterface::containsSwitch(
        _In_ sai_object_id_t switchId) const
{
    SWSS_LOG_ENTER();

    if (!m_switchContainer->contains(switchId))
    {
        SWSS_LOG_INFO("context %s failed to find switch %s",
                m_contextConfig->m_name.c_str(), sai_serialize_object_id(switchId).c_str());
        return false;
    }

    return true;
}

const std::map<sai_object_id_t, swss::TableDump>& RedisRemoteSaiInterface::getTableDump() const
{
    SWSS_LOG_ENTER();

    return m_tableDump;
}

void RedisRemoteSaiInterface::refreshTableDump()
{
    SWSS_LOG_ENTER();

    SWSS_LOG_TIMER("get asic view from %s", ASIC_STATE_TABLE);

    swss::Table table(m_db.get(), ASIC_STATE_TABLE);

    swss::TableDump dump;

    table.dump(dump);

    auto& map = m_tableDump;

    map.clear();

    for (auto& key: dump)
    {
        sai_object_meta_key_t mk;
        sai_deserialize_object_meta_key(key.first, mk);

        auto switchVID = switchIdQuery(mk.objectkey.key.object_id);

        map[switchVID][key.first] = key.second;
    }

    SWSS_LOG_NOTICE("%s switch count: %zu:", ASIC_STATE_TABLE, map.size());

    for (auto& kvp: map)
    {
        SWSS_LOG_NOTICE("%s: objects count: %zu",
                sai_serialize_object_id(kvp.first).c_str(),
                kvp.second.size());
    }
}
