#include "ClientOtai.h"
#include "OtaiInternal.h"
#include "RedisRemoteOtaiInterface.h"
#include "ZeroMQChannel.h"
#include "Utils.h"
#include "otairediscommon.h"
#include "ClientConfig.h"

#include "swss/logger.h"

#include "meta/otai_serialize.h"
#include "meta/NotificationFactory.h"
#include "meta/OtaiAttributeList.h"
#include "meta/PerformanceIntervalTimer.h"
#include "meta/Globals.h"

#include <inttypes.h>

#define REDIS_CHECK_API_INITIALIZED()                                       \
    if (!m_apiInitialized) {                                                \
        SWSS_LOG_ERROR("%s: api not initialized", __PRETTY_FUNCTION__);     \
        return OTAI_STATUS_FAILURE; }

using namespace otairedis;
using namespace otairediscommon;
using namespace otaimeta;
using namespace std::placeholders;

// TODO how to tell if current OTAI is in init view or apply view ?

std::vector<swss::FieldValueTuple> serialize_counter_id_list(
        _In_ const otai_enum_metadata_t *stats_enum,
        _In_ uint32_t count,
        _In_ const otai_stat_id_t *counter_id_list);

ClientOtai::ClientOtai()
{
    SWSS_LOG_ENTER();

    m_apiInitialized = false;
}

ClientOtai::~ClientOtai()
{
    SWSS_LOG_ENTER();

    if (m_apiInitialized)
    {
        apiUninitialize();
    }
}

otai_status_t ClientOtai::apiInitialize(
        _In_ uint64_t flags,
        _In_ const otai_service_method_table_t *service_method_table)
{
    MUTEX();
    SWSS_LOG_ENTER();

    if (m_apiInitialized)
    {
        SWSS_LOG_ERROR("already initialized");

        return OTAI_STATUS_FAILURE;
    }

    if ((service_method_table == NULL) ||
            (service_method_table->profile_get_next_value == NULL) ||
            (service_method_table->profile_get_value == NULL))
    {
        SWSS_LOG_ERROR("invalid service_method_table handle passed to OTAI API initialize");

        return OTAI_STATUS_INVALID_PARAMETER;
    }

    // TODO support context config

    m_switchContainer = std::make_shared<SwitchContainer>();

    auto clientConfig = service_method_table->profile_get_value(0, OTAI_REDIS_KEY_CLIENT_CONFIG);

    auto cc = ClientConfig::loadFromFile(clientConfig);

    m_communicationChannel = std::make_shared<ZeroMQChannel>(
            cc->m_zmqEndpoint,
            cc->m_zmqNtfEndpoint,
            std::bind(&ClientOtai::handleNotification, this, _1, _2, _3));

    m_apiInitialized = true;

    return OTAI_STATUS_SUCCESS;
}

otai_status_t ClientOtai::apiUninitialize(void)
{
    SWSS_LOG_ENTER();
    REDIS_CHECK_API_INITIALIZED();

    SWSS_LOG_NOTICE("begin");

    m_communicationChannel = nullptr;

    m_apiInitialized = false;

    SWSS_LOG_NOTICE("end");

    return OTAI_STATUS_SUCCESS;
}

// QUAD API

otai_status_t ClientOtai::create(
        _In_ otai_object_type_t objectType,
        _Out_ otai_object_id_t* objectId,
        _In_ otai_object_id_t switchId,
        _In_ uint32_t attr_count,
        _In_ const otai_attribute_t *attr_list)
{
    MUTEX();
    SWSS_LOG_ENTER();
    REDIS_CHECK_API_INITIALIZED();

    // NOTE: in client mode, each create oid must be retrieved from server since
    // server is actually creating new oid, and it's value must be transferred
    // over communication channel

    *objectId = OTAI_NULL_OBJECT_ID;

    if (objectType == OTAI_OBJECT_TYPE_LINECARD && attr_count > 0 && attr_list)
    {
        auto initSwitchAttr = otai_metadata_get_attr_by_id(OTAI_LINECARD_ATTR_INIT_LINECARD, attr_count, attr_list);

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
                auto meta = otai_metadata_get_attr_metadata(OTAI_OBJECT_TYPE_LINECARD, attr_list[i].id);

                if (meta == NULL)
                {
                    SWSS_LOG_THROW("failed to find metadata for switch attribute %d", attr_list[i].id);
                }

                if (meta->attrid == OTAI_LINECARD_ATTR_INIT_LINECARD)
                    continue;

                //if (meta->attrid == OTAI_LINECARD_ATTR_LINECARD_HARDWARE_INFO)
                //    continue;

                if (meta->attrvaluetype == OTAI_ATTR_VALUE_TYPE_POINTER)
                {
                    SWSS_LOG_ERROR("notifications not supported yet, FIXME");

                    return OTAI_STATUS_FAILURE;
                }

                SWSS_LOG_ERROR("attribute %s not supported during INIT_LINECARD=false, expected HARDWARE_INFO or notification pointer", meta->attridname);

                return OTAI_STATUS_FAILURE;
            }
        }
        else
        {
            SWSS_LOG_ERROR("creating new switch not supported yet, use OTAI_LINECARD_ATTR_INIT_LINECARD=false");

            return OTAI_STATUS_FAILURE;
        }
    }

    auto status = create(
            objectType,
            otai_serialize_object_id(switchId), // using switch ID instead of oid to transfer to server
            attr_count,
            attr_list);

    if (status == OTAI_STATUS_SUCCESS)
    {
        // since user requested create, OID value was created remotely and it
        // was returned in m_lastCreateOids

        *objectId = m_lastCreateOids.at(0);
    }

    if (objectType == OTAI_OBJECT_TYPE_LINECARD && status == OTAI_STATUS_SUCCESS)
    {
        /*
         * When doing CREATE operation user may want to update notification
         * pointers, since notifications can be defined per switch we need to
         * update them.
         */

        SWSS_LOG_NOTICE("create switch OID = %s", otai_serialize_object_id(*objectId).c_str());

        auto sw = std::make_shared<Switch>(*objectId, attr_count, attr_list);

        m_switchContainer->insert(sw);
    }

    return status;
}

otai_status_t ClientOtai::remove(
        _In_ otai_object_type_t objectType,
        _In_ otai_object_id_t objectId)
{
    MUTEX();
    SWSS_LOG_ENTER();
    REDIS_CHECK_API_INITIALIZED();

    auto status = remove(
            objectType,
            otai_serialize_object_id(objectId));

    if (objectType == OTAI_OBJECT_TYPE_LINECARD && status == OTAI_STATUS_SUCCESS)
    {
        SWSS_LOG_NOTICE("removing switch id %s", otai_serialize_object_id(objectId).c_str());

        // remove switch from container
        m_switchContainer->removeSwitch(objectId);
    }

    return status;
}

otai_status_t ClientOtai::set(
        _In_ otai_object_type_t objectType,
        _In_ otai_object_id_t objectId,
        _In_ const otai_attribute_t *attr)
{
    MUTEX();
    SWSS_LOG_ENTER();
    REDIS_CHECK_API_INITIALIZED();

    if (RedisRemoteOtaiInterface::isRedisAttribute(objectType, attr))
    {
        SWSS_LOG_ERROR("otairedis extension attributes are not supported in CLIENT mode");

        return OTAI_STATUS_FAILURE;
    }

    auto status = set(
            objectType,
            otai_serialize_object_id(objectId),
            attr);

    if (objectType == OTAI_OBJECT_TYPE_LINECARD && status == OTAI_STATUS_SUCCESS)
    {
        auto sw = m_switchContainer->getSwitch(objectId);

        if (!sw)
        {
            SWSS_LOG_THROW("failed to find switch %s in container",
                    otai_serialize_object_id(objectId).c_str());
        }

        /*
         * When doing SET operation user may want to update notification
         * pointers.
         */

        sw->updateNotifications(1, attr);
    }

    return status;
}

otai_status_t ClientOtai::get(
        _In_ otai_object_type_t objectType,
        _In_ otai_object_id_t objectId,
        _In_ uint32_t attr_count,
        _Inout_ otai_attribute_t *attr_list)
{
    MUTEX();
    SWSS_LOG_ENTER();
    REDIS_CHECK_API_INITIALIZED();

    return get(
            objectType,
            otai_serialize_object_id(objectId),
            attr_count,
            attr_list);
}

// QUAD ENTRY API

#define DECLARE_CREATE_ENTRY(OT,ot)                             \
otai_status_t ClientOtai::create(                                 \
        _In_ const otai_ ## ot ## _t* ot,                        \
        _In_ uint32_t attr_count,                               \
        _In_ const otai_attribute_t *attr_list)                  \
{                                                               \
    MUTEX();                                                    \
    SWSS_LOG_ENTER();                                           \
    REDIS_CHECK_API_INITIALIZED();                              \
    return create(                                              \
            (otai_object_type_t)OTAI_OBJECT_TYPE_ ## OT,          \
            otai_serialize_ ## ot(*ot),                          \
            attr_count,                                         \
            attr_list);                                         \
}

OTAIREDIS_DECLARE_EVERY_ENTRY(DECLARE_CREATE_ENTRY);

#define DECLARE_REMOVE_ENTRY(OT,ot)                             \
otai_status_t ClientOtai::remove(                                 \
        _In_ const otai_ ## ot ## _t* ot)                        \
{                                                               \
    MUTEX();                                                    \
    SWSS_LOG_ENTER();                                           \
    REDIS_CHECK_API_INITIALIZED();                              \
    return remove(                                              \
            (otai_object_type_t)OTAI_OBJECT_TYPE_ ## OT,          \
            otai_serialize_ ## ot(*ot));                         \
}

OTAIREDIS_DECLARE_EVERY_ENTRY(DECLARE_REMOVE_ENTRY);

#define DECLARE_SET_ENTRY(OT,ot)                                \
otai_status_t ClientOtai::set(                                    \
        _In_ const otai_ ## ot ## _t* ot,                        \
        _In_ const otai_attribute_t *attr)                       \
{                                                               \
    MUTEX();                                                    \
    SWSS_LOG_ENTER();                                           \
    REDIS_CHECK_API_INITIALIZED();                              \
    return set(                                                 \
            (otai_object_type_t)OTAI_OBJECT_TYPE_ ## OT,          \
            otai_serialize_ ## ot(*ot),                          \
            attr);                                              \
}

OTAIREDIS_DECLARE_EVERY_ENTRY(DECLARE_SET_ENTRY);

#define DECLARE_GET_ENTRY(OT,ot)                                \
otai_status_t ClientOtai::get(                                    \
        _In_ const otai_ ## ot ## _t* ot,                        \
        _In_ uint32_t attr_count,                               \
        _Inout_ otai_attribute_t *attr_list)                     \
{                                                               \
    MUTEX();                                                    \
    SWSS_LOG_ENTER();                                           \
    REDIS_CHECK_API_INITIALIZED();                              \
    return get(                                                 \
            (otai_object_type_t)OTAI_OBJECT_TYPE_ ## OT,          \
            otai_serialize_ ## ot(*ot),                          \
            attr_count,                                         \
            attr_list);                                         \
}

OTAIREDIS_DECLARE_EVERY_ENTRY(DECLARE_GET_ENTRY);

#define DECLARE_BULK_CREATE_ENTRY(OT,ot)                                       \
otai_status_t ClientOtai::bulkCreate(                                            \
        _In_ uint32_t object_count,                                            \
        _In_ const otai_ ## ot ## _t *ot,                                       \
        _In_ const uint32_t *attr_count,                                       \
        _In_ const otai_attribute_t **attr_list,                                \
        _In_ otai_bulk_op_error_mode_t mode,                                    \
        _Out_ otai_status_t *object_statuses)                                   \
{                                                                              \
    MUTEX();                                                                   \
    SWSS_LOG_ENTER();                                                          \
    REDIS_CHECK_API_INITIALIZED();                                             \
    static PerformanceIntervalTimer timer("ClientOtai::bulkCreate(" #ot ")");   \
    timer.start();                                                             \
    std::vector<std::string> serialized_object_ids;                            \
    for (uint32_t idx = 0; idx < object_count; idx++)                          \
    {                                                                          \
        std::string str_object_id = otai_serialize_ ##ot (ot[idx]);             \
        serialized_object_ids.push_back(str_object_id);                        \
    }                                                                          \
    auto status = bulkCreate(                                                  \
            (otai_object_type_t)OTAI_OBJECT_TYPE_ ## OT,                         \
            serialized_object_ids,                                             \
            attr_count,                                                        \
            attr_list,                                                         \
            mode,                                                              \
            object_statuses);                                                  \
    timer.stop();                                                              \
    timer.inc(object_count);                                                   \
    return status;                                                             \
}

OTAIREDIS_DECLARE_EVERY_BULK_ENTRY(DECLARE_BULK_CREATE_ENTRY)

#define DECLARE_BULK_REMOVE_ENTRY(OT,ot)                                                                        \
otai_status_t ClientOtai::bulkRemove(                                                                             \
        _In_ uint32_t object_count,                                                                             \
        _In_ const otai_ ## ot ## _t *ot,                                                                        \
        _In_ otai_bulk_op_error_mode_t mode,                                                                     \
        _Out_ otai_status_t *object_statuses)                                                                    \
{                                                                                                               \
    MUTEX();                                                                                                    \
    SWSS_LOG_ENTER();                                                                                           \
    REDIS_CHECK_API_INITIALIZED();                                                                              \
    std::vector<std::string> serializedObjectIds;                                                               \
    for (uint32_t idx = 0; idx < object_count; idx++)                                                           \
    {                                                                                                           \
        serializedObjectIds.emplace_back(otai_serialize_ ##ot (ot[idx]));                                        \
    }                                                                                                           \
    return bulkRemove((otai_object_type_t)OTAI_OBJECT_TYPE_ ## OT, serializedObjectIds, mode, object_statuses);   \
}

OTAIREDIS_DECLARE_EVERY_BULK_ENTRY(DECLARE_BULK_REMOVE_ENTRY)

#define DECLARE_BULK_SET_ENTRY(OT,ot)                                                                     \
otai_status_t ClientOtai::bulkSet(                                                                          \
        _In_ uint32_t object_count,                                                                       \
        _In_ const otai_ ## ot ## _t *ot,                                                                  \
        _In_ const otai_attribute_t *attr_list,                                                            \
        _In_ otai_bulk_op_error_mode_t mode,                                                               \
        _Out_ otai_status_t *object_statuses)                                                              \
{                                                                                                         \
    MUTEX();                                                                                              \
    SWSS_LOG_ENTER();                                                                                     \
    REDIS_CHECK_API_INITIALIZED();                                                                        \
    std::vector<std::string> serializedObjectIds;                                                         \
    for (uint32_t idx = 0; idx < object_count; idx++)                                                     \
    {                                                                                                     \
        serializedObjectIds.emplace_back(otai_serialize_ ##ot (ot[idx]));                                  \
    }                                                                                                     \
    return bulkSet(OTAI_OBJECT_TYPE_ROUTE_ENTRY, serializedObjectIds, attr_list, mode, object_statuses);   \
}

OTAIREDIS_DECLARE_EVERY_BULK_ENTRY(DECLARE_BULK_SET_ENTRY)

// BULK GET

#define DECLARE_BULK_GET_ENTRY(OT,ot)                       \
otai_status_t ClientOtai::bulkGet(                            \
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

OTAIREDIS_DECLARE_EVERY_BULK_ENTRY(DECLARE_BULK_GET_ENTRY)

// QUAD API HELPERS

otai_status_t ClientOtai::create(
        _In_ otai_object_type_t object_type,
        _In_ const std::string& serializedObjectId,
        _In_ uint32_t attr_count,
        _In_ const otai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    auto entry = OtaiAttributeList::serialize_attr_list(
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

    auto serializedObjectType = otai_serialize_object_type(object_type);

    const std::string key = serializedObjectType + ":" + serializedObjectId;

    SWSS_LOG_DEBUG("generic create key: %s, fields: %" PRIu64, key.c_str(), entry.size());

    m_communicationChannel->set(key, entry, REDIS_ASIC_STATE_COMMAND_CREATE);

    auto status = waitForResponse(OTAI_COMMON_API_CREATE);

    return status;
}

otai_status_t ClientOtai::remove(
        _In_ otai_object_type_t objectType,
        _In_ const std::string& serializedObjectId)
{
    SWSS_LOG_ENTER();

    auto serializedObjectType = otai_serialize_object_type(objectType);

    const std::string key = serializedObjectType + ":" + serializedObjectId;

    SWSS_LOG_DEBUG("generic remove key: %s", key.c_str());

    m_communicationChannel->set(key, {}, REDIS_ASIC_STATE_COMMAND_REMOVE);

    auto status = waitForResponse(OTAI_COMMON_API_REMOVE);

    return status;
}

otai_status_t ClientOtai::set(
        _In_ otai_object_type_t objectType,
        _In_ const std::string &serializedObjectId,
        _In_ const otai_attribute_t *attr)
{
    SWSS_LOG_ENTER();

    auto entry = OtaiAttributeList::serialize_attr_list(
            objectType,
            1,
            attr,
            false);

    auto serializedObjectType = otai_serialize_object_type(objectType);

    std::string key = serializedObjectType + ":" + serializedObjectId;

    SWSS_LOG_DEBUG("generic set key: %s, fields: %lu", key.c_str(), entry.size());

    m_communicationChannel->set(key, entry, REDIS_ASIC_STATE_COMMAND_SET);

    auto status = waitForResponse(OTAI_COMMON_API_SET);

    return status;
}

otai_status_t ClientOtai::get(
        _In_ otai_object_type_t objectType,
        _In_ const std::string& serializedObjectId,
        _In_ uint32_t attr_count,
        _Inout_ otai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    /*
     * Since user may reuse buffers, then oid list buffers maybe not cleared
     * and contain some garbage, let's clean them so we send all oids as null to
     * syncd.
     */

    Utils::clearOidValues(objectType, attr_count, attr_list);

    auto entry = OtaiAttributeList::serialize_attr_list(objectType, attr_count, attr_list, false);

    std::string serializedObjectType = otai_serialize_object_type(objectType);

    std::string key = serializedObjectType + ":" + serializedObjectId;

    SWSS_LOG_DEBUG("generic get key: %s, fields: %lu", key.c_str(), entry.size());

    // get is special, it will not put data
    // into asic view, only to message queue
    m_communicationChannel->set(key, entry, REDIS_ASIC_STATE_COMMAND_GET);

    auto status = waitForGetResponse(objectType, attr_count, attr_list);

    return status;
}

// QUAD API RESPONSE HELPERS

otai_status_t ClientOtai::waitForResponse(
        _In_ otai_common_api_t api)
{
    SWSS_LOG_ENTER();

    swss::KeyOpFieldsValuesTuple kco;

    auto status = m_communicationChannel->wait(REDIS_ASIC_STATE_COMMAND_GETRESPONSE, kco);

    if (api == OTAI_COMMON_API_CREATE && status == OTAI_STATUS_SUCCESS)
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
            otai_object_id_t oid;
            otai_deserialize_object_id(value, oid);

            m_lastCreateOids.push_back(oid);
        }
    }

    return status;
}

otai_status_t ClientOtai::waitForGetResponse(
        _In_ otai_object_type_t objectType,
        _In_ uint32_t attr_count,
        _Inout_ otai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    swss::KeyOpFieldsValuesTuple kco;

    auto status = m_communicationChannel->wait(REDIS_ASIC_STATE_COMMAND_GETRESPONSE, kco);

    auto &values = kfvFieldsValues(kco);

    if (status == OTAI_STATUS_SUCCESS)
    {
        if (values.size() == 0)
        {
            SWSS_LOG_THROW("logic error, get response returned 0 values!, send api response or sync/async issue?");
        }

        OtaiAttributeList list(objectType, values, false);

        transfer_attributes(objectType, attr_count, list.get_attr_list(), attr_list, false);
    }
    else if (status == OTAI_STATUS_BUFFER_OVERFLOW)
    {
        if (values.size() == 0)
        {
            SWSS_LOG_THROW("logic error, get response returned 0 values!, send api response or sync/async issue?");
        }

        OtaiAttributeList list(objectType, values, true);

        // no need for id fix since this is overflow
        transfer_attributes(objectType, attr_count, list.get_attr_list(), attr_list, true);
    }

    return status;
}

// FLUSH FDB ENTRIES

// OBJECT TYPE GET AVAILABILITY

otai_status_t ClientOtai::objectTypeGetAvailability(
        _In_ otai_object_id_t switchId,
        _In_ otai_object_type_t objectType,
        _In_ uint32_t attrCount,
        _In_ const otai_attribute_t *attrList,
        _Out_ uint64_t *count)
{
    MUTEX();
    SWSS_LOG_ENTER();
    REDIS_CHECK_API_INITIALIZED();

    auto strSwitchId = otai_serialize_object_id(switchId);

    auto entry = OtaiAttributeList::serialize_attr_list(objectType, attrCount, attrList, false);

    entry.push_back(swss::FieldValueTuple("OBJECT_TYPE", otai_serialize_object_type(objectType)));

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

otai_status_t ClientOtai::waitForObjectTypeGetAvailabilityResponse(
        _Inout_ uint64_t *count)
{
    SWSS_LOG_ENTER();

    swss::KeyOpFieldsValuesTuple kco;

    auto status = m_communicationChannel->wait(REDIS_ASIC_STATE_COMMAND_OBJECT_TYPE_GET_AVAILABILITY_RESPONSE, kco);

    if (status == OTAI_STATUS_SUCCESS)
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

// QUERY ATTRIBUTE ENUM CAPABILITY

otai_status_t ClientOtai::queryAttributeEnumValuesCapability(
        _In_ otai_object_id_t switchId,
        _In_ otai_object_type_t objectType,
        _In_ otai_attr_id_t attrId,
        _Inout_ otai_s32_list_t *enumValuesCapability)
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

    auto switch_id_str = otai_serialize_object_id(switchId);
    auto object_type_str = otai_serialize_object_type(objectType);

    auto meta = otai_metadata_get_attr_metadata(objectType, attrId);

    if (meta == NULL)
    {
        SWSS_LOG_ERROR("Failed to find attribute metadata: object type %s, attr id %d", object_type_str.c_str(), attrId);
        return OTAI_STATUS_INVALID_PARAMETER;
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

otai_status_t ClientOtai::waitForQueryAattributeEnumValuesCapabilityResponse(
        _Inout_ otai_s32_list_t* enumValuesCapability)
{
    SWSS_LOG_ENTER();

    swss::KeyOpFieldsValuesTuple kco;

    auto status = m_communicationChannel->wait(REDIS_ASIC_STATE_COMMAND_ATTR_ENUM_VALUES_CAPABILITY_RESPONSE, kco);

    if (status == OTAI_STATUS_SUCCESS)
    {
        const std::vector<swss::FieldValueTuple> &values = kfvFieldsValues(kco);

        if (values.size() != 2)
        {
            SWSS_LOG_ERROR("Invalid response from syncd: expected 2 values, received %zu", values.size());

            return OTAI_STATUS_FAILURE;
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
    else if (status ==  OTAI_STATUS_BUFFER_OVERFLOW)
    {
        // TODO on otai status overflow we should populate correct count on the list

        SWSS_LOG_ERROR("TODO need to handle OTAI_STATUS_BUFFER_OVERFLOW, FIXME");
    }

    return status;
}


// STATS API

otai_status_t ClientOtai::getStats(
        _In_ otai_object_type_t object_type,
        _In_ otai_object_id_t object_id,
        _In_ uint32_t number_of_counters,
        _In_ const otai_stat_id_t *counter_ids,
        _Out_ uint64_t *counters)
{
    MUTEX();
    SWSS_LOG_ENTER();
    REDIS_CHECK_API_INITIALIZED();

    auto stats_enum = otai_metadata_get_object_type_info(object_type)->statenum;

    auto entry = serialize_counter_id_list(stats_enum, number_of_counters, counter_ids);

    std::string str_object_type = otai_serialize_object_type(object_type);

    std::string key = str_object_type + ":" + otai_serialize_object_id(object_id);

    SWSS_LOG_DEBUG("generic get stats key: %s, fields: %lu", key.c_str(), entry.size());

    // get_stats will not put data to asic view, only to message queue

    m_communicationChannel->set(key, entry, REDIS_ASIC_STATE_COMMAND_GET_STATS);

    return waitForGetStatsResponse(number_of_counters, counters);
}

otai_status_t ClientOtai::waitForGetStatsResponse(
        _In_ uint32_t number_of_counters,
        _Out_ uint64_t *counters)
{
    SWSS_LOG_ENTER();

    swss::KeyOpFieldsValuesTuple kco;

    auto status = m_communicationChannel->wait(REDIS_ASIC_STATE_COMMAND_GETRESPONSE, kco);

    if (status == OTAI_STATUS_SUCCESS)
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

otai_status_t ClientOtai::getStatsExt(
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

    SWSS_LOG_ERROR("not implemented");

    // TODO could be the same as getStats but put mode at first argument

    return OTAI_STATUS_NOT_IMPLEMENTED;
}

otai_status_t ClientOtai::clearStats(
        _In_ otai_object_type_t object_type,
        _In_ otai_object_id_t object_id,
        _In_ uint32_t number_of_counters,
        _In_ const otai_stat_id_t *counter_ids)
{
    MUTEX();
    SWSS_LOG_ENTER();
    REDIS_CHECK_API_INITIALIZED();

    auto stats_enum = otai_metadata_get_object_type_info(object_type)->statenum;

    auto values = serialize_counter_id_list(stats_enum, number_of_counters, counter_ids);

    auto str_object_type = otai_serialize_object_type(object_type);

    auto key = str_object_type + ":" + otai_serialize_object_id(object_id);

    SWSS_LOG_DEBUG("generic clear stats key: %s, fields: %lu", key.c_str(), values.size());

    // clear_stats will not put data into asic view, only to message queue

    m_communicationChannel->set(key, values, REDIS_ASIC_STATE_COMMAND_CLEAR_STATS);

    auto status = waitForClearStatsResponse();

    return status;
}

otai_status_t ClientOtai::waitForClearStatsResponse()
{
    SWSS_LOG_ENTER();

    swss::KeyOpFieldsValuesTuple kco;

    auto status = m_communicationChannel->wait(REDIS_ASIC_STATE_COMMAND_GETRESPONSE, kco);

    return status;
}

otai_status_t ClientOtai::bulkGetStats(
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

    SWSS_LOG_ERROR("not implemented");

    return OTAI_STATUS_NOT_IMPLEMENTED;
}

otai_status_t ClientOtai::bulkClearStats(
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

    SWSS_LOG_ERROR("not implemented");

    return OTAI_STATUS_NOT_IMPLEMENTED;
}

// BULK CREATE

otai_status_t ClientOtai::bulkCreate(
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
        serialized_object_ids.emplace_back(otai_serialize_object_id(switch_id));
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

        if (object_statuses[idx] == OTAI_STATUS_SUCCESS)
        {
            object_id[idx] = m_lastCreateOids.at(idx);
        }
        else
        {
            object_id[idx] = OTAI_NULL_OBJECT_ID;
        }
    }

    return status;
}

otai_status_t ClientOtai::bulkCreate(
        _In_ otai_object_type_t object_type,
        _In_ const std::vector<std::string> &serialized_object_ids,
        _In_ const uint32_t *attr_count,
        _In_ const otai_attribute_t **attr_list,
        _In_ otai_bulk_op_error_mode_t mode,
        _Inout_ otai_status_t *object_statuses)
{
    SWSS_LOG_ENTER();

    // TODO support mode

    std::string str_object_type = otai_serialize_object_type(object_type);

    std::vector<swss::FieldValueTuple> entries;

    for (size_t idx = 0; idx < serialized_object_ids.size(); ++idx)
    {
        auto entry = OtaiAttributeList::serialize_attr_list(object_type, attr_count[idx], attr_list[idx], false);

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

    return waitForBulkResponse(OTAI_COMMON_API_BULK_CREATE, (uint32_t)serialized_object_ids.size(), object_statuses);
}

// BULK REMOVE

otai_status_t ClientOtai::bulkRemove(
        _In_ otai_object_type_t object_type,
        _In_ uint32_t object_count,
        _In_ const otai_object_id_t *object_id,
        _In_ otai_bulk_op_error_mode_t mode,
        _Out_ otai_status_t *object_statuses)
{
    MUTEX();
    SWSS_LOG_ENTER();
    REDIS_CHECK_API_INITIALIZED();

    std::vector<std::string> serializedObjectIds;

    for (uint32_t idx = 0; idx < object_count; idx++)
    {
        serializedObjectIds.emplace_back(otai_serialize_object_id(object_id[idx]));
    }

    return bulkRemove(object_type, serializedObjectIds, mode, object_statuses);
}

// BULK REMOVE HELPERS

otai_status_t ClientOtai::bulkRemove(
        _In_ otai_object_type_t object_type,
        _In_ const std::vector<std::string> &serialized_object_ids,
        _In_ otai_bulk_op_error_mode_t mode,
        _Out_ otai_status_t *object_statuses)
{
    SWSS_LOG_ENTER();

    // TODO support mode, this will need to go as extra parameter and needs to
    // be supported by LUA script passed as first or last entry in values,
    // currently mode is ignored

    std::string serializedObjectType = otai_serialize_object_type(object_type);

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

    return waitForBulkResponse(OTAI_COMMON_API_BULK_REMOVE, (uint32_t)serialized_object_ids.size(), object_statuses);
}

// BULK SET

otai_status_t ClientOtai::bulkSet(
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

    std::vector<std::string> serializedObjectIds;

    for (uint32_t idx = 0; idx < object_count; idx++)
    {
        serializedObjectIds.emplace_back(otai_serialize_object_id(object_id[idx]));
    }

    return bulkSet(object_type, serializedObjectIds, attr_list, mode, object_statuses);
}

// BULK SET HELPERS

otai_status_t ClientOtai::bulkSet(
        _In_ otai_object_type_t object_type,
        _In_ const std::vector<std::string> &serialized_object_ids,
        _In_ const otai_attribute_t *attr_list,
        _In_ otai_bulk_op_error_mode_t mode,
        _Out_ otai_status_t *object_statuses)
{
    MUTEX();
    SWSS_LOG_ENTER();
    REDIS_CHECK_API_INITIALIZED();

    // TODO support mode

    std::vector<swss::FieldValueTuple> entries;

    for (size_t idx = 0; idx < serialized_object_ids.size(); ++idx)
    {
        auto entry = OtaiAttributeList::serialize_attr_list(object_type, 1, &attr_list[idx], false);

        std::string str_attr = Globals::joinFieldValues(entry);

        swss::FieldValueTuple value(serialized_object_ids[idx], str_attr);

        entries.push_back(value);
    }

    /*
     * We are adding number of entries to actually add ':' to be compatible
     * with previous
     */

    auto serializedObjectType = otai_serialize_object_type(object_type);

    std::string key = serializedObjectType + ":" + std::to_string(entries.size());

    m_communicationChannel->set(key, entries, REDIS_ASIC_STATE_COMMAND_BULK_SET);

    return waitForBulkResponse(OTAI_COMMON_API_BULK_SET, (uint32_t)serialized_object_ids.size(), object_statuses);
}

// BULK GET

otai_status_t ClientOtai::bulkGet(
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

// BULK RESPONSE HELPERS

otai_status_t ClientOtai::waitForBulkResponse(
        _In_ otai_common_api_t api,
        _In_ uint32_t object_count,
        _Out_ otai_status_t *object_statuses)
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
        otai_deserialize_status(fvField(values[idx]), object_statuses[idx]);
    }

    if (api == OTAI_COMMON_API_BULK_CREATE)
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
                otai_object_id_t oid;
                otai_deserialize_object_id(value, oid);

                m_lastCreateOids.push_back(oid);
            }
        }
    }

    return status;
}

void ClientOtai::handleNotification(
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

otai_linecard_notifications_t ClientOtai::syncProcessNotification(
        _In_ std::shared_ptr<Notification> notification)
{
    SWSS_LOG_ENTER();

    // NOTE: process metadata must be executed under otairedis API mutex since
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
            otai_serialize_object_id(switchId).c_str());

    return { };
}

otai_object_type_t ClientOtai::objectTypeQuery(
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

otai_object_id_t ClientOtai::switchIdQuery(
        _In_ otai_object_id_t objectId)
{
    SWSS_LOG_ENTER();

    if (!m_apiInitialized)
    {
        SWSS_LOG_ERROR("%s: OTAI API not initialized", __PRETTY_FUNCTION__);

        return OTAI_OBJECT_TYPE_NULL;
    }

    return VirtualObjectIdManager::switchIdQuery(objectId);
}

otai_status_t ClientOtai::logSet(
        _In_ otai_api_t api,
        _In_ otai_log_level_t log_level)
{
    SWSS_LOG_ENTER();

    return OTAI_STATUS_SUCCESS;
}

