#include "ClientSai.h"
#include "SaiInternal.h"
#include "RedisRemoteSaiInterface.h"
#include "ZeroMQChannel.h"
#include "Utils.h"
#include "sairediscommon.h"
#include "ClientConfig.h"

#include "swss/logger.h"

#include "meta/sai_serialize.h"
#include "meta/NotificationFactory.h"
#include "meta/SaiAttributeList.h"
#include "meta/PerformanceIntervalTimer.h"
#include "meta/Globals.h"

#include <inttypes.h>

#define REDIS_CHECK_API_INITIALIZED()                                       \
    if (!m_apiInitialized) {                                                \
        SWSS_LOG_ERROR("%s: api not initialized", __PRETTY_FUNCTION__);     \
        return SAI_STATUS_FAILURE; }

using namespace sairedis;
using namespace sairediscommon;
using namespace saimeta;
using namespace std::placeholders;

// TODO how to tell if current SAI is in init view or apply view ?

std::vector<swss::FieldValueTuple> serialize_counter_id_list(
        _In_ const sai_enum_metadata_t *stats_enum,
        _In_ uint32_t count,
        _In_ const sai_stat_id_t *counter_id_list);

ClientSai::ClientSai()
{
    SWSS_LOG_ENTER();

    m_apiInitialized = false;
}

ClientSai::~ClientSai()
{
    SWSS_LOG_ENTER();

    if (m_apiInitialized)
    {
        apiUninitialize();
    }
}

sai_status_t ClientSai::apiInitialize(
        _In_ uint64_t flags,
        _In_ const sai_service_method_table_t *service_method_table)
{
    MUTEX();
    SWSS_LOG_ENTER();

    if (m_apiInitialized)
    {
        SWSS_LOG_ERROR("already initialized");

        return SAI_STATUS_FAILURE;
    }

    if ((service_method_table == NULL) ||
            (service_method_table->profile_get_next_value == NULL) ||
            (service_method_table->profile_get_value == NULL))
    {
        SWSS_LOG_ERROR("invalid service_method_table handle passed to SAI API initialize");

        return SAI_STATUS_INVALID_PARAMETER;
    }

    // TODO support context config

    m_switchContainer = std::make_shared<SwitchContainer>();

    auto clientConfig = service_method_table->profile_get_value(0, SAI_REDIS_KEY_CLIENT_CONFIG);

    auto cc = ClientConfig::loadFromFile(clientConfig);

    m_communicationChannel = std::make_shared<ZeroMQChannel>(
            cc->m_zmqEndpoint,
            cc->m_zmqNtfEndpoint,
            std::bind(&ClientSai::handleNotification, this, _1, _2, _3));

    m_apiInitialized = true;

    return SAI_STATUS_SUCCESS;
}

sai_status_t ClientSai::apiUninitialize(void)
{
    SWSS_LOG_ENTER();
    REDIS_CHECK_API_INITIALIZED();

    SWSS_LOG_NOTICE("begin");

    m_communicationChannel = nullptr;

    m_apiInitialized = false;

    SWSS_LOG_NOTICE("end");

    return SAI_STATUS_SUCCESS;
}

// QUAD API

sai_status_t ClientSai::create(
        _In_ sai_object_type_t objectType,
        _Out_ sai_object_id_t* objectId,
        _In_ sai_object_id_t switchId,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    MUTEX();
    SWSS_LOG_ENTER();
    REDIS_CHECK_API_INITIALIZED();

    // NOTE: in client mode, each create oid must be retrieved from server since
    // server is actually creating new oid, and it's value must be transferred
    // over communication channel

    *objectId = SAI_NULL_OBJECT_ID;

    if (objectType == SAI_OBJECT_TYPE_SWITCH && attr_count > 0 && attr_list)
    {
        auto initSwitchAttr = sai_metadata_get_attr_by_id(SAI_SWITCH_ATTR_INIT_SWITCH, attr_count, attr_list);

        if (initSwitchAttr && initSwitchAttr->value.booldata == false)
        {
            // TODO for connect, we may not need to actually call server, if we
            // have hwinfo and context and context container information, we
            // could allocate switch object ID locally

            auto hwinfo = Globals::getHardwareInfo(attr_count, attr_list);

            // TODO support context
            SWSS_LOG_NOTICE("request to connect switch (context: 0) and hwinfo='%s'", hwinfo.c_str());

            for (uint32_t i = 0; i < attr_count; ++i)
            {
                auto meta = sai_metadata_get_attr_metadata(SAI_OBJECT_TYPE_SWITCH, attr_list[i].id);

                if (meta == NULL)
                {
                    SWSS_LOG_THROW("failed to find metadata for switch attribute %d", attr_list[i].id);
                }

                if (meta->attrid == SAI_SWITCH_ATTR_INIT_SWITCH)
                    continue;

                if (meta->attrid == SAI_SWITCH_ATTR_SWITCH_HARDWARE_INFO)
                    continue;

                if (meta->attrvaluetype == SAI_ATTR_VALUE_TYPE_POINTER)
                {
                    SWSS_LOG_ERROR("notifications not supported yet, FIXME");

                    return SAI_STATUS_FAILURE;
                }

                SWSS_LOG_ERROR("attribute %s not supported during INIT_SWITCH=false, expected HARDWARE_INFO or notification pointer", meta->attridname);

                return SAI_STATUS_FAILURE;
            }
        }
        else
        {
            SWSS_LOG_ERROR("creating new switch not supported yet, use SAI_SWITCH_ATTR_INIT_SWITCH=false");

            return SAI_STATUS_FAILURE;
        }
    }

    auto status = create(
            objectType,
            sai_serialize_object_id(switchId), // using switch ID instead of oid to transfer to server
            attr_count,
            attr_list);

    if (status == SAI_STATUS_SUCCESS)
    {
        // since user requested create, OID value was created remotely and it
        // was returned in m_lastCreateOids

        *objectId = m_lastCreateOids.at(0);
    }

    if (objectType == SAI_OBJECT_TYPE_SWITCH && status == SAI_STATUS_SUCCESS)
    {
        /*
         * When doing CREATE operation user may want to update notification
         * pointers, since notifications can be defined per switch we need to
         * update them.
         */

        SWSS_LOG_NOTICE("create switch OID = %s", sai_serialize_object_id(*objectId).c_str());

        auto sw = std::make_shared<Switch>(*objectId, attr_count, attr_list);

        m_switchContainer->insert(sw);
    }

    return status;
}

sai_status_t ClientSai::remove(
        _In_ sai_object_type_t objectType,
        _In_ sai_object_id_t objectId)
{
    MUTEX();
    SWSS_LOG_ENTER();
    REDIS_CHECK_API_INITIALIZED();

    auto status = remove(
            objectType,
            sai_serialize_object_id(objectId));

    if (objectType == SAI_OBJECT_TYPE_SWITCH && status == SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_NOTICE("removing switch id %s", sai_serialize_object_id(objectId).c_str());

        // remove switch from container
        m_switchContainer->removeSwitch(objectId);
    }

    return status;
}

sai_status_t ClientSai::set(
        _In_ sai_object_type_t objectType,
        _In_ sai_object_id_t objectId,
        _In_ const sai_attribute_t *attr)
{
    MUTEX();
    SWSS_LOG_ENTER();
    REDIS_CHECK_API_INITIALIZED();

    if (RedisRemoteSaiInterface::isRedisAttribute(objectType, attr))
    {
        SWSS_LOG_ERROR("sairedis extension attributes are not supported in CLIENT mode");

        return SAI_STATUS_FAILURE;
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

sai_status_t ClientSai::get(
        _In_ sai_object_type_t objectType,
        _In_ sai_object_id_t objectId,
        _In_ uint32_t attr_count,
        _Inout_ sai_attribute_t *attr_list)
{
    MUTEX();
    SWSS_LOG_ENTER();
    REDIS_CHECK_API_INITIALIZED();

    return get(
            objectType,
            sai_serialize_object_id(objectId),
            attr_count,
            attr_list);
}

// QUAD ENTRY API

#define DECLARE_CREATE_ENTRY(OT,ot)                             \
sai_status_t ClientSai::create(                                 \
        _In_ const sai_ ## ot ## _t* ot,                        \
        _In_ uint32_t attr_count,                               \
        _In_ const sai_attribute_t *attr_list)                  \
{                                                               \
    MUTEX();                                                    \
    SWSS_LOG_ENTER();                                           \
    REDIS_CHECK_API_INITIALIZED();                              \
    return create(                                              \
            (sai_object_type_t)SAI_OBJECT_TYPE_ ## OT,          \
            sai_serialize_ ## ot(*ot),                          \
            attr_count,                                         \
            attr_list);                                         \
}

SAIREDIS_DECLARE_EVERY_ENTRY(DECLARE_CREATE_ENTRY);

#define DECLARE_REMOVE_ENTRY(OT,ot)                             \
sai_status_t ClientSai::remove(                                 \
        _In_ const sai_ ## ot ## _t* ot)                        \
{                                                               \
    MUTEX();                                                    \
    SWSS_LOG_ENTER();                                           \
    REDIS_CHECK_API_INITIALIZED();                              \
    return remove(                                              \
            (sai_object_type_t)SAI_OBJECT_TYPE_ ## OT,          \
            sai_serialize_ ## ot(*ot));                         \
}

SAIREDIS_DECLARE_EVERY_ENTRY(DECLARE_REMOVE_ENTRY);

#define DECLARE_SET_ENTRY(OT,ot)                                \
sai_status_t ClientSai::set(                                    \
        _In_ const sai_ ## ot ## _t* ot,                        \
        _In_ const sai_attribute_t *attr)                       \
{                                                               \
    MUTEX();                                                    \
    SWSS_LOG_ENTER();                                           \
    REDIS_CHECK_API_INITIALIZED();                              \
    return set(                                                 \
            (sai_object_type_t)SAI_OBJECT_TYPE_ ## OT,          \
            sai_serialize_ ## ot(*ot),                          \
            attr);                                              \
}

SAIREDIS_DECLARE_EVERY_ENTRY(DECLARE_SET_ENTRY);

#define DECLARE_GET_ENTRY(OT,ot)                                \
sai_status_t ClientSai::get(                                    \
        _In_ const sai_ ## ot ## _t* ot,                        \
        _In_ uint32_t attr_count,                               \
        _Inout_ sai_attribute_t *attr_list)                     \
{                                                               \
    MUTEX();                                                    \
    SWSS_LOG_ENTER();                                           \
    REDIS_CHECK_API_INITIALIZED();                              \
    return get(                                                 \
            (sai_object_type_t)SAI_OBJECT_TYPE_ ## OT,          \
            sai_serialize_ ## ot(*ot),                          \
            attr_count,                                         \
            attr_list);                                         \
}

SAIREDIS_DECLARE_EVERY_ENTRY(DECLARE_GET_ENTRY);

#define DECLARE_BULK_CREATE_ENTRY(OT,ot)                                       \
sai_status_t ClientSai::bulkCreate(                                            \
        _In_ uint32_t object_count,                                            \
        _In_ const sai_ ## ot ## _t *ot,                                       \
        _In_ const uint32_t *attr_count,                                       \
        _In_ const sai_attribute_t **attr_list,                                \
        _In_ sai_bulk_op_error_mode_t mode,                                    \
        _Out_ sai_status_t *object_statuses)                                   \
{                                                                              \
    MUTEX();                                                                   \
    SWSS_LOG_ENTER();                                                          \
    REDIS_CHECK_API_INITIALIZED();                                             \
    static PerformanceIntervalTimer timer("ClientSai::bulkCreate(" #ot ")");   \
    timer.start();                                                             \
    std::vector<std::string> serialized_object_ids;                            \
    for (uint32_t idx = 0; idx < object_count; idx++)                          \
    {                                                                          \
        std::string str_object_id = sai_serialize_ ##ot (ot[idx]);             \
        serialized_object_ids.push_back(str_object_id);                        \
    }                                                                          \
    auto status = bulkCreate(                                                  \
            (sai_object_type_t)SAI_OBJECT_TYPE_ ## OT,                         \
            serialized_object_ids,                                             \
            attr_count,                                                        \
            attr_list,                                                         \
            mode,                                                              \
            object_statuses);                                                  \
    timer.stop();                                                              \
    timer.inc(object_count);                                                   \
    return status;                                                             \
}

SAIREDIS_DECLARE_EVERY_BULK_ENTRY(DECLARE_BULK_CREATE_ENTRY)

#define DECLARE_BULK_REMOVE_ENTRY(OT,ot)                                                                        \
sai_status_t ClientSai::bulkRemove(                                                                             \
        _In_ uint32_t object_count,                                                                             \
        _In_ const sai_ ## ot ## _t *ot,                                                                        \
        _In_ sai_bulk_op_error_mode_t mode,                                                                     \
        _Out_ sai_status_t *object_statuses)                                                                    \
{                                                                                                               \
    MUTEX();                                                                                                    \
    SWSS_LOG_ENTER();                                                                                           \
    REDIS_CHECK_API_INITIALIZED();                                                                              \
    std::vector<std::string> serializedObjectIds;                                                               \
    for (uint32_t idx = 0; idx < object_count; idx++)                                                           \
    {                                                                                                           \
        serializedObjectIds.emplace_back(sai_serialize_ ##ot (ot[idx]));                                        \
    }                                                                                                           \
    return bulkRemove((sai_object_type_t)SAI_OBJECT_TYPE_ ## OT, serializedObjectIds, mode, object_statuses);   \
}

SAIREDIS_DECLARE_EVERY_BULK_ENTRY(DECLARE_BULK_REMOVE_ENTRY)

#define DECLARE_BULK_SET_ENTRY(OT,ot)                                                                     \
sai_status_t ClientSai::bulkSet(                                                                          \
        _In_ uint32_t object_count,                                                                       \
        _In_ const sai_ ## ot ## _t *ot,                                                                  \
        _In_ const sai_attribute_t *attr_list,                                                            \
        _In_ sai_bulk_op_error_mode_t mode,                                                               \
        _Out_ sai_status_t *object_statuses)                                                              \
{                                                                                                         \
    MUTEX();                                                                                              \
    SWSS_LOG_ENTER();                                                                                     \
    REDIS_CHECK_API_INITIALIZED();                                                                        \
    std::vector<std::string> serializedObjectIds;                                                         \
    for (uint32_t idx = 0; idx < object_count; idx++)                                                     \
    {                                                                                                     \
        serializedObjectIds.emplace_back(sai_serialize_ ##ot (ot[idx]));                                  \
    }                                                                                                     \
    return bulkSet(SAI_OBJECT_TYPE_ROUTE_ENTRY, serializedObjectIds, attr_list, mode, object_statuses);   \
}

SAIREDIS_DECLARE_EVERY_BULK_ENTRY(DECLARE_BULK_SET_ENTRY)

// BULK GET

#define DECLARE_BULK_GET_ENTRY(OT,ot)                       \
sai_status_t ClientSai::bulkGet(                            \
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

SAIREDIS_DECLARE_EVERY_BULK_ENTRY(DECLARE_BULK_GET_ENTRY)

// QUAD API HELPERS

sai_status_t ClientSai::create(
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

    m_communicationChannel->set(key, entry, REDIS_ASIC_STATE_COMMAND_CREATE);

    auto status = waitForResponse(SAI_COMMON_API_CREATE);

    return status;
}

sai_status_t ClientSai::remove(
        _In_ sai_object_type_t objectType,
        _In_ const std::string& serializedObjectId)
{
    SWSS_LOG_ENTER();

    auto serializedObjectType = sai_serialize_object_type(objectType);

    const std::string key = serializedObjectType + ":" + serializedObjectId;

    SWSS_LOG_DEBUG("generic remove key: %s", key.c_str());

    m_communicationChannel->set(key, {}, REDIS_ASIC_STATE_COMMAND_REMOVE);

    auto status = waitForResponse(SAI_COMMON_API_REMOVE);

    return status;
}

sai_status_t ClientSai::set(
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

    m_communicationChannel->set(key, entry, REDIS_ASIC_STATE_COMMAND_SET);

    auto status = waitForResponse(SAI_COMMON_API_SET);

    return status;
}

sai_status_t ClientSai::get(
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

    // get is special, it will not put data
    // into asic view, only to message queue
    m_communicationChannel->set(key, entry, REDIS_ASIC_STATE_COMMAND_GET);

    auto status = waitForGetResponse(objectType, attr_count, attr_list);

    return status;
}

// QUAD API RESPONSE HELPERS

sai_status_t ClientSai::waitForResponse(
        _In_ sai_common_api_t api)
{
    SWSS_LOG_ENTER();

    swss::KeyOpFieldsValuesTuple kco;

    auto status = m_communicationChannel->wait(REDIS_ASIC_STATE_COMMAND_GETRESPONSE, kco);

    if (api == SAI_COMMON_API_CREATE && status == SAI_STATUS_SUCCESS)
    {
        m_lastCreateOids.clear();

        // since last api was create, we need to extract OID that was created in that api
        // if create was for entry, oid is NULL

        const auto& entry = kfvFieldsValues(kco);

        const auto& field = fvField(entry.at(0));
        const auto& value = fvValue(entry.at(0));

        SWSS_LOG_INFO("parsing response: %s:%s", field.c_str(), value.c_str());

        if (field == "oid")
        {
            sai_object_id_t oid;
            sai_deserialize_object_id(value, oid);

            m_lastCreateOids.push_back(oid);
        }
    }

    return status;
}

sai_status_t ClientSai::waitForGetResponse(
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

// FLUSH FDB ENTRIES

sai_status_t ClientSai::flushFdbEntries(
        _In_ sai_object_id_t switchId,
        _In_ uint32_t attrCount,
        _In_ const sai_attribute_t *attrList)
{
    MUTEX();
    SWSS_LOG_ENTER();
    REDIS_CHECK_API_INITIALIZED();

    auto entry = SaiAttributeList::serialize_attr_list(
            SAI_OBJECT_TYPE_FDB_FLUSH,
            attrCount,
            attrList,
            false);

    std::string serializedObjectId = sai_serialize_object_type(SAI_OBJECT_TYPE_FDB_FLUSH);

    // NOTE ! we actually give switch ID since FLUSH is not real object
    std::string key = serializedObjectId + ":" + sai_serialize_object_id(switchId);

    SWSS_LOG_NOTICE("flush key: %s, fields: %lu", key.c_str(), entry.size());

    m_communicationChannel->set(key, entry, REDIS_ASIC_STATE_COMMAND_FLUSH);

    auto status = waitForFlushFdbEntriesResponse();

    return status;
}

sai_status_t ClientSai::waitForFlushFdbEntriesResponse()
{
    SWSS_LOG_ENTER();

    swss::KeyOpFieldsValuesTuple kco;

    auto status = m_communicationChannel->wait(REDIS_ASIC_STATE_COMMAND_FLUSHRESPONSE, kco);

    return status;
}

// OBJECT TYPE GET AVAILABILITY

sai_status_t ClientSai::objectTypeGetAvailability(
        _In_ sai_object_id_t switchId,
        _In_ sai_object_type_t objectType,
        _In_ uint32_t attrCount,
        _In_ const sai_attribute_t *attrList,
        _Out_ uint64_t *count)
{
    MUTEX();
    SWSS_LOG_ENTER();
    REDIS_CHECK_API_INITIALIZED();

    auto strSwitchId = sai_serialize_object_id(switchId);

    auto entry = SaiAttributeList::serialize_attr_list(objectType, attrCount, attrList, false);

    entry.push_back(swss::FieldValueTuple("OBJECT_TYPE", sai_serialize_object_type(objectType)));

    SWSS_LOG_DEBUG(
            "Query arguments: switch: %s, attributes: %s",
            strSwitchId.c_str(),
            Globals::joinFieldValues(entry).c_str());

    // Syncd will pop this argument off before trying to deserialize the attribute list

    // This query will not put any data into the ASIC view, just into the
    // message queue
    m_communicationChannel->set(strSwitchId, entry, REDIS_ASIC_STATE_COMMAND_OBJECT_TYPE_GET_AVAILABILITY_QUERY);

    auto status = waitForObjectTypeGetAvailabilityResponse(count);

    return status;
}

sai_status_t ClientSai::waitForObjectTypeGetAvailabilityResponse(
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

// QUERY ATTRIBUTE CAPABILITY

sai_status_t ClientSai::queryAttributeCapability(
        _In_ sai_object_id_t switchId,
        _In_ sai_object_type_t objectType,
        _In_ sai_attr_id_t attrId,
        _Out_ sai_attr_capability_t *capability)
{
    MUTEX();
    SWSS_LOG_ENTER();
    REDIS_CHECK_API_INITIALIZED();

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

    m_communicationChannel->set(switchIdStr, entry, REDIS_ASIC_STATE_COMMAND_ATTR_CAPABILITY_QUERY);

    auto status = waitForQueryAttributeCapabilityResponse(capability);

    return status;
}

sai_status_t ClientSai::waitForQueryAttributeCapabilityResponse(
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
            (capability->create_implemented ? "true" : "false"),
            (capability->set_implemented ? "true" : "false"),
            (capability->get_implemented ? "true" : "false"));
    }

    return status;
}

// QUERY ATTRIBUTE ENUM CAPABILITY

sai_status_t ClientSai::queryAttributeEnumValuesCapability(
        _In_ sai_object_id_t switchId,
        _In_ sai_object_type_t objectType,
        _In_ sai_attr_id_t attrId,
        _Inout_ sai_s32_list_t *enumValuesCapability)
{
    MUTEX();
    SWSS_LOG_ENTER();
    REDIS_CHECK_API_INITIALIZED();

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

    m_communicationChannel->set(switch_id_str, entry, REDIS_ASIC_STATE_COMMAND_ATTR_ENUM_VALUES_CAPABILITY_QUERY);

    auto status = waitForQueryAattributeEnumValuesCapabilityResponse(enumValuesCapability);

    return status;
}

sai_status_t ClientSai::waitForQueryAattributeEnumValuesCapabilityResponse(
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
    else if (status ==  SAI_STATUS_BUFFER_OVERFLOW)
    {
        // TODO on sai status overflow we should populate correct count on the list

        SWSS_LOG_ERROR("TODO need to handle SAI_STATUS_BUFFER_OVERFLOW, FIXME");
    }

    return status;
}


// STATS API

sai_status_t ClientSai::getStats(
        _In_ sai_object_type_t object_type,
        _In_ sai_object_id_t object_id,
        _In_ uint32_t number_of_counters,
        _In_ const sai_stat_id_t *counter_ids,
        _Out_ uint64_t *counters)
{
    MUTEX();
    SWSS_LOG_ENTER();
    REDIS_CHECK_API_INITIALIZED();

    auto stats_enum = sai_metadata_get_object_type_info(object_type)->statenum;

    auto entry = serialize_counter_id_list(stats_enum, number_of_counters, counter_ids);

    std::string str_object_type = sai_serialize_object_type(object_type);

    std::string key = str_object_type + ":" + sai_serialize_object_id(object_id);

    SWSS_LOG_DEBUG("generic get stats key: %s, fields: %lu", key.c_str(), entry.size());

    // get_stats will not put data to asic view, only to message queue

    m_communicationChannel->set(key, entry, REDIS_ASIC_STATE_COMMAND_GET_STATS);

    return waitForGetStatsResponse(number_of_counters, counters);
}

sai_status_t ClientSai::waitForGetStatsResponse(
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

sai_status_t ClientSai::queryStatsCapability(
        _In_ sai_object_id_t switchId,
        _In_ sai_object_type_t objectType,
        _Inout_ sai_stat_capability_list_t *stats_capability)
{
    MUTEX();
    SWSS_LOG_ENTER();
    REDIS_CHECK_API_INITIALIZED();

    auto switchIdStr = sai_serialize_object_id(switchId);
    auto objectTypeStr = sai_serialize_object_type(objectType);

    if (stats_capability == NULL)
    {
        SWSS_LOG_ERROR("Failed to find stats-capability: switch %s object type %s", switchIdStr.c_str(), objectTypeStr.c_str());
        return SAI_STATUS_INVALID_PARAMETER;
    }

    if (stats_capability && stats_capability->list && (stats_capability->count))
    {
        // clear input list, since we use serialize to transfer values
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

    m_communicationChannel->set(switchIdStr, entry, REDIS_ASIC_STATE_COMMAND_STATS_CAPABILITY_QUERY);

    return waitForQueryStatsCapabilityResponse(stats_capability);
}


sai_status_t ClientSai::queryStatsStCapability(
    _In_ sai_object_id_t switchId,
    _In_ sai_object_type_t objectType,
    _Inout_ sai_stat_st_capability_list_t *stats_capability)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_NOT_IMPLEMENTED;
}

sai_status_t ClientSai::waitForQueryStatsCapabilityResponse(
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

sai_status_t ClientSai::getStatsExt(
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

    SWSS_LOG_ERROR("not implemented");

    // TODO could be the same as getStats but put mode at first argument

    return SAI_STATUS_NOT_IMPLEMENTED;
}

sai_status_t ClientSai::clearStats(
        _In_ sai_object_type_t object_type,
        _In_ sai_object_id_t object_id,
        _In_ uint32_t number_of_counters,
        _In_ const sai_stat_id_t *counter_ids)
{
    MUTEX();
    SWSS_LOG_ENTER();
    REDIS_CHECK_API_INITIALIZED();

    auto stats_enum = sai_metadata_get_object_type_info(object_type)->statenum;

    auto values = serialize_counter_id_list(stats_enum, number_of_counters, counter_ids);

    auto str_object_type = sai_serialize_object_type(object_type);

    auto key = str_object_type + ":" + sai_serialize_object_id(object_id);

    SWSS_LOG_DEBUG("generic clear stats key: %s, fields: %lu", key.c_str(), values.size());

    // clear_stats will not put data into asic view, only to message queue

    m_communicationChannel->set(key, values, REDIS_ASIC_STATE_COMMAND_CLEAR_STATS);

    auto status = waitForClearStatsResponse();

    return status;
}

sai_status_t ClientSai::waitForClearStatsResponse()
{
    SWSS_LOG_ENTER();

    swss::KeyOpFieldsValuesTuple kco;

    auto status = m_communicationChannel->wait(REDIS_ASIC_STATE_COMMAND_GETRESPONSE, kco);

    return status;
}

sai_status_t ClientSai::bulkGetStats(
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

    SWSS_LOG_ERROR("not implemented");

    return SAI_STATUS_NOT_IMPLEMENTED;
}

sai_status_t ClientSai::bulkClearStats(
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

    SWSS_LOG_ERROR("not implemented");

    return SAI_STATUS_NOT_IMPLEMENTED;
}

// BULK CREATE

sai_status_t ClientSai::bulkCreate(
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

    // NOTE: in client mode, each create oid must be retrieved from server since
    // server is actually creating new oid, and it's value must be transferred
    // over communication channel

    // TODO support mode

    std::vector<std::string> serialized_object_ids;

    // server is responsible for generate new OID but for that we need switch ID
    // to be sent to server as well, so instead of sending empty oids we will
    // send switch IDs
    for (uint32_t idx = 0; idx < object_count; idx++)
    {
        serialized_object_ids.emplace_back(sai_serialize_object_id(switch_id));
    }

    auto status = bulkCreate(
            object_type,
            serialized_object_ids,
            attr_count,
            attr_list,
            mode,
            object_statuses);

    for (uint32_t idx = 0; idx < object_count; idx++)
    {
        // since user requested create, OID value was created remotely and it
        // was returned in m_lastCreateOids

        if (object_statuses[idx] == SAI_STATUS_SUCCESS)
        {
            object_id[idx] = m_lastCreateOids.at(idx);
        }
        else
        {
            object_id[idx] = SAI_NULL_OBJECT_ID;
        }
    }

    return status;
}

sai_status_t ClientSai::bulkCreate(
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

    m_communicationChannel->set(key, entries, REDIS_ASIC_STATE_COMMAND_BULK_CREATE);

    return waitForBulkResponse(SAI_COMMON_API_BULK_CREATE, (uint32_t)serialized_object_ids.size(), object_statuses);
}

// BULK REMOVE

sai_status_t ClientSai::bulkRemove(
        _In_ sai_object_type_t object_type,
        _In_ uint32_t object_count,
        _In_ const sai_object_id_t *object_id,
        _In_ sai_bulk_op_error_mode_t mode,
        _Out_ sai_status_t *object_statuses)
{
    MUTEX();
    SWSS_LOG_ENTER();
    REDIS_CHECK_API_INITIALIZED();

    std::vector<std::string> serializedObjectIds;

    for (uint32_t idx = 0; idx < object_count; idx++)
    {
        serializedObjectIds.emplace_back(sai_serialize_object_id(object_id[idx]));
    }

    return bulkRemove(object_type, serializedObjectIds, mode, object_statuses);
}

// BULK REMOVE HELPERS

sai_status_t ClientSai::bulkRemove(
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

    m_communicationChannel->set(key, entries, REDIS_ASIC_STATE_COMMAND_BULK_REMOVE);

    return waitForBulkResponse(SAI_COMMON_API_BULK_REMOVE, (uint32_t)serialized_object_ids.size(), object_statuses);
}

// BULK SET

sai_status_t ClientSai::bulkSet(
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

    std::vector<std::string> serializedObjectIds;

    for (uint32_t idx = 0; idx < object_count; idx++)
    {
        serializedObjectIds.emplace_back(sai_serialize_object_id(object_id[idx]));
    }

    return bulkSet(object_type, serializedObjectIds, attr_list, mode, object_statuses);
}

// BULK SET HELPERS

sai_status_t ClientSai::bulkSet(
        _In_ sai_object_type_t object_type,
        _In_ const std::vector<std::string> &serialized_object_ids,
        _In_ const sai_attribute_t *attr_list,
        _In_ sai_bulk_op_error_mode_t mode,
        _Out_ sai_status_t *object_statuses)
{
    MUTEX();
    SWSS_LOG_ENTER();
    REDIS_CHECK_API_INITIALIZED();

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

    m_communicationChannel->set(key, entries, REDIS_ASIC_STATE_COMMAND_BULK_SET);

    return waitForBulkResponse(SAI_COMMON_API_BULK_SET, (uint32_t)serialized_object_ids.size(), object_statuses);
}

// BULK GET

sai_status_t ClientSai::bulkGet(
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

    SWSS_LOG_ERROR("not implemented, FIXME");

    return SAI_STATUS_NOT_IMPLEMENTED;
}

// BULK RESPONSE HELPERS

sai_status_t ClientSai::waitForBulkResponse(
        _In_ sai_common_api_t api,
        _In_ uint32_t object_count,
        _Out_ sai_status_t *object_statuses)
{
    SWSS_LOG_ENTER();

    swss::KeyOpFieldsValuesTuple kco;

    auto status = m_communicationChannel->wait(REDIS_ASIC_STATE_COMMAND_GETRESPONSE, kco);

    auto &values = kfvFieldsValues(kco);

    // double object count may show up when bulk create api is executed

    if ((values.size() != 2 * object_count) && (values.size() != object_count))
    {
        SWSS_LOG_THROW("wrong number of counters, got %zu, expected %u", values.size(), object_count);
    }

    // deserialize statuses for all objects

    for (uint32_t idx = 0; idx < object_count; idx++)
    {
        sai_deserialize_status(fvField(values[idx]), object_statuses[idx]);
    }

    if (api == SAI_COMMON_API_BULK_CREATE)
    {
        m_lastCreateOids.clear();

        // since last api was create, we need to extract OID that was created in that api
        // if create was for entry, oid is NULL

        for (uint32_t idx = object_count; idx < 2 * object_count; idx++)
        {
            const auto& field = fvField(values.at(idx));
            const auto& value = fvValue(values.at(idx));

            if (field == "oid")
            {
                sai_object_id_t oid;
                sai_deserialize_object_id(value, oid);

                m_lastCreateOids.push_back(oid);
            }
        }
    }

    return status;
}

void ClientSai::handleNotification(
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

    auto notification = NotificationFactory::deserialize(name, serializedNotification);

    if (notification)
    {
        MUTEX();

        if (!m_apiInitialized)
        {
            SWSS_LOG_ERROR("%s: api not initialized", __PRETTY_FUNCTION__);

            return;
        }

        auto sn = syncProcessNotification(notification);

        // execute callback from notification thread

        notification->executeCallback(sn);
    }
}

sai_switch_notifications_t ClientSai::syncProcessNotification(
        _In_ std::shared_ptr<Notification> notification)
{
    SWSS_LOG_ENTER();

    // NOTE: process metadata must be executed under sairedis API mutex since
    // it will access meta database and notification comes from different
    // thread, and this method is executed from notifications thread

    auto objectId = notification->getAnyObjectId();

    auto switchId = switchIdQuery(objectId);

    auto sw = m_switchContainer->getSwitch(switchId);

    if (sw)
    {
        return sw->getSwitchNotifications(); // explicit copy
    }

    SWSS_LOG_WARN("switch %s not present in container, returning empty switch notifications",
            sai_serialize_object_id(switchId).c_str());

    return { };
}

sai_object_type_t ClientSai::objectTypeQuery(
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

sai_object_id_t ClientSai::switchIdQuery(
        _In_ sai_object_id_t objectId)
{
    SWSS_LOG_ENTER();

    if (!m_apiInitialized)
    {
        SWSS_LOG_ERROR("%s: SAI API not initialized", __PRETTY_FUNCTION__);

        return SAI_OBJECT_TYPE_NULL;
    }

    return VirtualObjectIdManager::switchIdQuery(objectId);
}

sai_status_t ClientSai::logSet(
        _In_ sai_api_t api,
        _In_ sai_log_level_t log_level)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_SUCCESS;
}

sai_status_t ClientSai::queryApiVersion(
        _Out_ sai_api_version_t *version)
{
    MUTEX();
    SWSS_LOG_ENTER();
    REDIS_CHECK_API_INITIALIZED();

    SWSS_LOG_ERROR("queryApiVersion not implemented, FIXME");

    return SAI_STATUS_NOT_IMPLEMENTED;
}
