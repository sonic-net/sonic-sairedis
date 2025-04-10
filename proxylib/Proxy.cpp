#include "Proxy.h"

#include "swss/logger.h"
#include "swss/select.h"
#include "swss/table.h"

#include "meta/SaiAttributeList.h"
#include "meta/sai_serialize.h"
#include "meta/ZeroMQSelectableChannel.h"

#include "syncd/ZeroMQNotificationProducer.h"
#include "syncd/Workaround.h"

#include <inttypes.h>

#include <iterator>
#include <algorithm>

using namespace saiproxy;
using namespace std::placeholders;

// TODO handle diagnostic shell

Proxy::Proxy(
        _In_ std::shared_ptr<sairedis::SaiInterface> vendorSai):
    Proxy(vendorSai, std::make_shared<Options>())
{
    SWSS_LOG_ENTER();

    SWSS_LOG_NOTICE("using default options");
}

Proxy::Proxy(
        _In_ std::shared_ptr<sairedis::SaiInterface> vendorSai,
        _In_ std::shared_ptr<Options> options):
    m_vendorSai(vendorSai),
    m_options(options),
    m_apiInitialized(false),
    m_notificationsSentCount(0),
    m_apiVersion(SAI_VERSION(0,0,0))
{
    SWSS_LOG_ENTER();

    SWSS_LOG_NOTICE("Options: %s", m_options->getString().c_str());

    m_selectableChannel = std::make_shared<sairedis::ZeroMQSelectableChannel>(m_options->m_zmqChannel);
    m_notifications = std::make_shared<syncd::ZeroMQNotificationProducer>(m_options->m_zmqNtfChannel);

    loadProfileMap();

    m_smt.profileGetValue = std::bind(&Proxy::profileGetValue, this, _1, _2);
    m_smt.profileGetNextValue = std::bind(&Proxy::profileGetNextValue, this, _1, _2, _3);

    m_test_services = m_smt.getServiceMethodTable();

    memset(&m_sn, 0, sizeof(m_sn));

    m_swNtf.onFdbEvent = std::bind(&Proxy::onFdbEvent, this, _1, _2);
    m_swNtf.onNatEvent = std::bind(&Proxy::onNatEvent, this, _1, _2);
    m_swNtf.onPortStateChange = std::bind(&Proxy::onPortStateChange, this, _1, _2);
    m_swNtf.onQueuePfcDeadlock = std::bind(&Proxy::onQueuePfcDeadlock, this, _1, _2);
    m_swNtf.onSwitchAsicSdkHealthEvent = std::bind(&Proxy::onSwitchAsicSdkHealthEvent, this, _1, _2, _3, _4, _5, _6);
    m_swNtf.onSwitchShutdownRequest = std::bind(&Proxy::onSwitchShutdownRequest, this, _1);
    m_swNtf.onSwitchStateChange = std::bind(&Proxy::onSwitchStateChange, this, _1, _2);
    m_swNtf.onBfdSessionStateChange = std::bind(&Proxy::onBfdSessionStateChange, this, _1, _2);
    m_swNtf.onPortHostTxReady = std::bind(&Proxy::onPortHostTxReady, this, _1, _2, _3);
    m_swNtf.onTwampSessionEvent = std::bind(&Proxy::onTwampSessionEvent, this, _1, _2);
    m_swNtf.onTamTelTypeConfigChange = std::bind(&Proxy::onTamTelTypeConfigChange, this, _1);

    m_sn = m_swNtf.getSwitchNotifications();

    sai_status_t status = m_vendorSai->apiInitialize(0, &m_test_services);

    if (status != SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_ERROR("FATAL: failed to sai_api_initialize: %s",
                sai_serialize_status(status).c_str());
    }
    else
    {
        m_apiInitialized = true;

        SWSS_LOG_NOTICE("api initialized success");
    }

    auto st = m_vendorSai->queryApiVersion(&m_apiVersion);

    if (status != SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_WARN("failed to obtain libsai api version: %s", sai_serialize_status(st).c_str());
    }
    else
    {
        SWSS_LOG_NOTICE("libsai api version: %lu", m_apiVersion);
    }
}

Proxy::~Proxy()
{
    SWSS_LOG_ENTER();

    // TODO call stop()

    if (m_apiInitialized)
    {
        SWSS_LOG_NOTICE("calling api uninitialize");

        auto status = m_vendorSai->apiUninitialize();

        if (status != SAI_STATUS_SUCCESS)
        {
            SWSS_LOG_ERROR("api uninitialize failed: %s", sai_serialize_status(status).c_str());
        }
    }

    // TODO lock guard could be needed to wait for notifications destruction
    // until all possible notification thread will be sent

    m_notifications = nullptr;
}

void Proxy::loadProfileMap()
{
    SWSS_LOG_ENTER();

    std::ifstream profile(m_options->m_config);

    if (!profile.is_open())
    {
        SWSS_LOG_WARN("failed to open profile map file: %s: %s",
                m_options->m_config.c_str(),
                strerror(errno));

        return;
    }

    std::string line;

    while (getline(profile, line))
    {
        if (line.size() > 0 && (line[0] == '#' || line[0] == ';'))
        {
            continue;
        }

        size_t pos = line.find("=");

        if (pos == std::string::npos)
        {
            SWSS_LOG_WARN("not found '=' in line %s", line.c_str());
            continue;
        }

        std::string key = line.substr(0, pos);
        std::string value = line.substr(pos + 1);

        m_profileMap[key] = value;

        SWSS_LOG_NOTICE("insert: %s:%s", key.c_str(), value.c_str());
    }
}

const char* Proxy::profileGetValue(
        _In_ sai_switch_profile_id_t profile_id,
        _In_ const char* variable)
{
    SWSS_LOG_ENTER();

    if (variable == NULL)
    {
        SWSS_LOG_WARN("variable is null");
        return NULL;
    }

    auto it = m_profileMap.find(variable);

    if (it == m_profileMap.end())
    {
        SWSS_LOG_NOTICE("%s: NULL", variable);
        return NULL;
    }

    SWSS_LOG_NOTICE("%s: %s", variable, it->second.c_str());

    return it->second.c_str();
}

int Proxy::profileGetNextValue(
        _In_ sai_switch_profile_id_t profile_id,
        _Out_ const char** variable,
        _Out_ const char** value)
{
    SWSS_LOG_ENTER();

    if (value == NULL)
    {
        SWSS_LOG_INFO("resetting profile map iterator");

        m_profileIter = m_profileMap.begin();
        return 0;
    }

    if (variable == NULL)
    {
        SWSS_LOG_WARN("variable is null");
        return -1;
    }

    if (m_profileIter == m_profileMap.end())
    {
        SWSS_LOG_INFO("iterator reached end");
        return -1;
    }

    *variable = m_profileIter->first.c_str();
    *value = m_profileIter->second.c_str();

    SWSS_LOG_INFO("key: %s:%s", *variable, *value);

    m_profileIter++;

    return 0;
}

void Proxy::run()
{
    SWSS_LOG_ENTER();

    SWSS_LOG_NOTICE("thread start");

    std::shared_ptr<swss::Select> s = std::make_shared<swss::Select>();

    // TODO will we need this thread at all?, maybe just resend notification as they
    // arrive ?  could this potentially cause deadlock ? if we will be invoking
    // some api under mutex and then that ntf would be called from same thread
    // as api invoked that's why we have processing queue on syncd here we would
    // need only protect ntf sender- notification channel

    // m_processor->startNotificationsProcessingThread();

    s->addSelectable(m_selectableChannel.get());
    s->addSelectable(&m_stopEvent);

    SWSS_LOG_NOTICE("entering main thread loop");

    while (true)
    {
        swss::Selectable *sel = NULL;

        int result = s->select(&sel);

        if (sel == &m_stopEvent)
        {
            break;
        }
        else if (sel == m_selectableChannel.get())
        {
            processEvent(*m_selectableChannel.get());
        }
        else
        {
            SWSS_LOG_ERROR("select failed: %d", result);
        }
    }

    SWSS_LOG_NOTICE("thread end");
}

void Proxy::stop()
{
    SWSS_LOG_ENTER();

    SWSS_LOG_NOTICE("stop thread requested");

    // TODO maybe not needed
    std::lock_guard<std::mutex> lock(m_mutex);

    m_stopEvent.notify();
}

void Proxy::processEvent(
        _In_ sairedis::SelectableChannel& consumer)
{
    SWSS_LOG_ENTER();

    std::lock_guard<std::mutex> lock(m_mutex);

    do
    {
        swss::KeyOpFieldsValuesTuple kco;

        consumer.pop(kco, false);

        processSingleEvent(kco);
    }
    while (!consumer.empty());
}

void Proxy::processSingleEvent(
        _In_ const swss::KeyOpFieldsValuesTuple &kco)
{
    SWSS_LOG_ENTER();

    auto& key = kfvKey(kco);
    auto& op = kfvOp(kco);

    SWSS_LOG_INFO("key: %s op: %s", key.c_str(), op.c_str());

    if (key.length() == 0)
    {
        SWSS_LOG_DEBUG("no elements in m_buffer");

        return;
    }

    // TODO implement bulk create/remove/set/get

    if (op == "create")
        return processCreate(kco);

    if (op == "remove")
        return processRemove(kco);

    if (op == "set")
        return processSet(kco);

    if (op == "get")
        return processGet(kco);

    if (op == "create_entry")
        return processCreateEntry(kco);

    if (op == "flush_fdb_entries")
        return processFlushFdbEntries(kco);

    if (op == "object_type_get_availability")
        return processObjectTypeGetAvailability(kco);

    if (op == "query_attribute_capability")
        return processQueryAttributeCapability(kco);

    if (op == "query_attribute_enum_values_capability")
        return processQueryAttributeEnumValuesCapability(kco);

    if (op == "object_type_query")
        return processObjectTypeQuery(kco);

    if (op == "switch_id_query")
        return processSwitchIdQuery(kco);

    if (op == "query_api_version")
        return processQueryApiVersion(kco);

    if (op == "log_set")
        return processLogSet(kco);

    if (op == "get_stats")
        return processGetStats(kco);

    if (op == "get_stats_ext")
        return processGetStatsExt(kco);

    if (op == "clear_stats")
        return processClearStats(kco);

    SWSS_LOG_THROW("event op '%s' is not implemented, FIXME", op.c_str());
}

void Proxy::processCreate(
        _In_ const swss::KeyOpFieldsValuesTuple &kco)
{
    SWSS_LOG_ENTER();

    const std::string& key = kfvKey(kco);

    const std::string& strObjectId = key.substr(key.find(":") + 1);

    sai_object_meta_key_t metaKey;
    sai_deserialize_object_meta_key(key, metaKey);

    if (!sai_metadata_is_object_type_valid(metaKey.objecttype))
    {
        SWSS_LOG_THROW("invalid object type %s", key.c_str());
    }

    auto values = kfvFieldsValues(kco);

    sai_object_id_t switchId = SAI_NULL_OBJECT_ID;

    auto vv = values.back();

    values.pop_back();

    SWSS_LOG_NOTICE("removed last field: %s: %s", fvField(vv).c_str(), fvValue(vv).c_str());

    sai_deserialize_object_id(fvValue(vv), switchId);

    for (auto& v: values)
    {
        SWSS_LOG_DEBUG("attr: %s: %s", fvField(v).c_str(), fvValue(v).c_str());
    }

    saimeta::SaiAttributeList list(metaKey.objecttype, values, false);

    sai_attribute_t *attr_list = list.get_attr_list();
    uint32_t attr_count = list.get_attr_count();

    if (metaKey.objecttype == SAI_OBJECT_TYPE_SWITCH)
    {
        /*
         * TODO: must be done per switch, and switch may not exists yet
         */

        updateAttributteNotificationPointers(attr_count, attr_list);
    }

    sai_object_id_t newObjectId = SAI_NULL_OBJECT_ID;;

    sai_status_t status = m_vendorSai->create(metaKey.objecttype, &newObjectId, switchId, attr_count, attr_list);

    std::vector<swss::FieldValueTuple> entry;

    std::string strStatus = sai_serialize_status(status);

    swss::FieldValueTuple fvt("OBJECT_ID", sai_serialize_object_id(newObjectId));

    entry.push_back(fvt);

    m_selectableChannel->set(strStatus, entry, "create_response");
}

void Proxy::processRemove(
        _In_ const swss::KeyOpFieldsValuesTuple &kco)
{
    SWSS_LOG_ENTER();

    const std::string& key = kfvKey(kco);

    const std::string& strObjectId = key.substr(key.find(":") + 1);

    sai_object_meta_key_t metaKey;
    sai_deserialize_object_meta_key(key, metaKey);

    if (!sai_metadata_is_object_type_valid(metaKey.objecttype))
    {
        SWSS_LOG_THROW("invalid object type %s", key.c_str());
    }

    sai_status_t status = m_vendorSai->remove(metaKey);

    std::string strStatus = sai_serialize_status(status);

    m_selectableChannel->set(strStatus, {}, "remove_response");
}

void Proxy::processSet(
        _In_ const swss::KeyOpFieldsValuesTuple &kco)
{
    SWSS_LOG_ENTER();

    const std::string& key = kfvKey(kco);

    const std::string& strObjectId = key.substr(key.find(":") + 1);

    sai_object_meta_key_t metaKey;
    sai_deserialize_object_meta_key(key, metaKey);

    if (!sai_metadata_is_object_type_valid(metaKey.objecttype))
    {
        SWSS_LOG_THROW("invalid object type %s", key.c_str());
    }

    auto values = kfvFieldsValues(kco);

    for (auto& v: values)
    {
        SWSS_LOG_DEBUG("attr: %s: %s", fvField(v).c_str(), fvValue(v).c_str());
    }

    saimeta::SaiAttributeList list(metaKey.objecttype, values, false);

    sai_attribute_t *attr_list = list.get_attr_list();

    if (metaKey.objecttype == SAI_OBJECT_TYPE_SWITCH)
    {
        /*
         * TODO: must be done per switch, and switch may not exists yet
         */

        updateAttributteNotificationPointers(1, attr_list);
    }

    sai_status_t status = m_vendorSai->set(metaKey, attr_list);

    std::vector<swss::FieldValueTuple> entry;

    std::string strStatus = sai_serialize_status(status);

    m_selectableChannel->set(strStatus, {}, "set_response");
}

void Proxy::processGet(
        _In_ const swss::KeyOpFieldsValuesTuple &kco)
{
    SWSS_LOG_ENTER();

    const std::string& key = kfvKey(kco);
    const std::string& strObjectId = key.substr(key.find(":") + 1);

    sai_object_meta_key_t metaKey;
    sai_deserialize_object_meta_key(key, metaKey);

    if (!sai_metadata_is_object_type_valid(metaKey.objecttype))
    {
        SWSS_LOG_THROW("invalid object type %s", key.c_str());
    }

    auto& values = kfvFieldsValues(kco);

    for (auto& v: values)
    {
        SWSS_LOG_DEBUG("attr: %s: %s", fvField(v).c_str(), fvValue(v).c_str());
    }

    saimeta::SaiAttributeList list(metaKey.objecttype, values, false);

    sai_attribute_t *attr_list = list.get_attr_list();
    uint32_t attr_count = list.get_attr_count();

    auto status = m_vendorSai->get(metaKey, attr_count, attr_list);

    std::vector<swss::FieldValueTuple> entry;

    if (status == SAI_STATUS_SUCCESS)
    {
        entry = saimeta::SaiAttributeList::serialize_attr_list(
                metaKey.objecttype,
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

        entry = saimeta::SaiAttributeList::serialize_attr_list(
                metaKey.objecttype,
                attr_count,
                attr_list,
                true);
    }
    else
    {
        /*
         * Some other error, don't send attributes at all.
         */

        SWSS_LOG_WARN("api failed: %s", sai_serialize_status(status).c_str());
    }

    std::string strStatus = sai_serialize_status(status);

    SWSS_LOG_INFO("sending response for GET api with status: %s", strStatus.c_str());

    /*
     * Since we have only one get at a time, we don't have to serialize object
     * type and object id, only get status is required to be returned.  Get
     * response will not put any data to table, only queue is used.
     */

    m_selectableChannel->set(strStatus, entry, "get_response");
}

void Proxy::processCreateEntry(
        _In_ const swss::KeyOpFieldsValuesTuple &kco)
{
    SWSS_LOG_ENTER();

    const std::string& key = kfvKey(kco);
    // const std::string& op = kfvOp(kco);

    const std::string& strObjectId = key.substr(key.find(":") + 1);

    sai_object_meta_key_t metaKey;
    sai_deserialize_object_meta_key(key, metaKey);

    if (!sai_metadata_is_object_type_valid(metaKey.objecttype))
    {
        SWSS_LOG_THROW("invalid object type %s", key.c_str());
    }

    auto values = kfvFieldsValues(kco);

    saimeta::SaiAttributeList list(metaKey.objecttype, values, false);

    sai_attribute_t *attr_list = list.get_attr_list();
    uint32_t attr_count = list.get_attr_count();

    // since this is only used for create entries, there is no need for notification
    // pointers translation, since object entries don't contain pointers

    sai_status_t status = m_vendorSai->create(metaKey, SAI_NULL_OBJECT_ID, attr_count, attr_list);

    std::string strStatus = sai_serialize_status(status);

    m_selectableChannel->set(strStatus, {}, "create_entry_response");
}

void Proxy::processFlushFdbEntries(
        _In_ const swss::KeyOpFieldsValuesTuple &kco)
{
    SWSS_LOG_ENTER();

    auto& key = kfvKey(kco);
    auto strSwitchId = key.substr(key.find(":") + 1);

    sai_object_id_t switchId;
    sai_deserialize_object_id(strSwitchId, switchId);

    auto& values = kfvFieldsValues(kco);

    for (const auto &v: values)
    {
        SWSS_LOG_NOTICE("attr: %s: %s", fvField(v).c_str(), fvValue(v).c_str());
    }

    saimeta::SaiAttributeList list(SAI_OBJECT_TYPE_FDB_FLUSH, values, false);

    sai_attribute_t *attr_list = list.get_attr_list();
    uint32_t attr_count = list.get_attr_count();

    sai_status_t status = m_vendorSai->flushFdbEntries(switchId, attr_count, attr_list);

    std::string strStatus = sai_serialize_status(status);

    m_selectableChannel->set(strStatus, {}, "flush_fdb_entries_response");
}

void Proxy::processObjectTypeGetAvailability(
    _In_ const swss::KeyOpFieldsValuesTuple &kco)
{
    SWSS_LOG_ENTER();

    auto& strSwitchId = kfvKey(kco);

    sai_object_id_t switchId;
    sai_deserialize_object_id(strSwitchId, switchId);

    std::vector<swss::FieldValueTuple> values = kfvFieldsValues(kco);

    sai_object_type_t objectType;
    sai_deserialize_object_type(fvValue(values.back()), objectType);

    values.pop_back();

    saimeta::SaiAttributeList list(objectType, values, false);

    sai_attribute_t *attr_list = list.get_attr_list();
    uint32_t attr_count = list.get_attr_count();

    uint64_t count;

    sai_status_t status = m_vendorSai->objectTypeGetAvailability(
            switchId,
            objectType,
            attr_count,
            attr_list,
            &count);

    std::vector<swss::FieldValueTuple> entry;

    if (status == SAI_STATUS_SUCCESS)
    {
        entry.emplace_back("COUNT", std::to_string(count));
    }

    std::string strStatus = sai_serialize_status(status);

    m_selectableChannel->set(strStatus, entry, "object_type_get_availability_response");
}

void Proxy::processQueryAttributeCapability(
        _In_ const swss::KeyOpFieldsValuesTuple &kco)
{
    SWSS_LOG_ENTER();

    auto& strSwitchId = kfvKey(kco);

    sai_object_id_t switchId;
    sai_deserialize_object_id(strSwitchId, switchId);

    auto& values = kfvFieldsValues(kco);

    if (values.size() != 2)
    {
        SWSS_LOG_ERROR("logic error, expected 2 arguments, received %zu", values.size());

        auto strStatus = sai_serialize_status(SAI_STATUS_INVALID_PARAMETER);

        m_selectableChannel->set(strStatus, {}, "query_attribute_capability_response");

        return;
    }

    sai_object_type_t objectType;
    sai_deserialize_object_type(fvValue(values[0]), objectType);

    sai_attr_id_t attrId;
    sai_deserialize_attr_id(fvValue(values[1]), attrId);

    sai_attr_capability_t capability;

    sai_status_t status = m_vendorSai->queryAttributeCapability(switchId, objectType, attrId, &capability);

    std::vector<swss::FieldValueTuple> entry;

    if (status == SAI_STATUS_SUCCESS)
    {
        entry =
        {
            swss::FieldValueTuple("CREATE_IMPLEMENTED", (capability.create_implemented ? "true" : "false")),
            swss::FieldValueTuple("SET_IMPLEMENTED",    (capability.set_implemented    ? "true" : "false")),
            swss::FieldValueTuple("GET_IMPLEMENTED",    (capability.get_implemented    ? "true" : "false"))
        };
    }

    auto strStatus = sai_serialize_status(status);

    m_selectableChannel->set(strStatus, entry, "query_attribute_capability_response");
}

void Proxy::processQueryAttributeEnumValuesCapability(
        _In_ const swss::KeyOpFieldsValuesTuple &kco)
{
    SWSS_LOG_ENTER();

    auto& strSwitchId = kfvKey(kco);

    sai_object_id_t switchId;
    sai_deserialize_object_id(strSwitchId, switchId);

    auto& values = kfvFieldsValues(kco);

    if (values.size() != 3)
    {
        SWSS_LOG_ERROR("logic error, expected 3 arguments, received %zu", values.size());

        auto strStatus = sai_serialize_status(SAI_STATUS_INVALID_PARAMETER);

        m_selectableChannel->set(strStatus, {}, "query_attribute_enum_values_capability_response");

        return;
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

    sai_status_t status = m_vendorSai->queryAttributeEnumValuesCapability(switchId, objectType, attrId, &enumCapList);

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
    }
    else if (status == SAI_STATUS_BUFFER_OVERFLOW)
    {
        entry =
        {
            swss::FieldValueTuple("ENUM_COUNT", std::to_string(enumCapList.count))
        };
    }

    auto strStatus = sai_serialize_status(status);

    m_selectableChannel->set(strStatus, entry, "query_attribute_enum_values_capability_response");
}

void Proxy::processObjectTypeQuery(
        _In_ const swss::KeyOpFieldsValuesTuple &kco)
{
    SWSS_LOG_ENTER();

    const std::string& key = kfvKey(kco);

    sai_object_id_t objectId;
    sai_deserialize_object_id(key, objectId);

    sai_object_type_t objectType = m_vendorSai->objectTypeQuery(objectId);

    std::vector<swss::FieldValueTuple> entry;

    std::string strStatus = sai_serialize_status(SAI_STATUS_SUCCESS); // we assume success

    entry.emplace_back("OBJECT_TYPE", sai_serialize_object_type(objectType));

    m_selectableChannel->set(strStatus, entry, "object_type_query_response");
}

void Proxy::processSwitchIdQuery(
        _In_ const swss::KeyOpFieldsValuesTuple &kco)
{
    SWSS_LOG_ENTER();

    const std::string& key = kfvKey(kco);

    sai_object_id_t objectId;
    sai_deserialize_object_id(key, objectId);

    sai_object_id_t switchId = m_vendorSai->switchIdQuery(objectId);

    std::vector<swss::FieldValueTuple> entry;

    std::string strStatus = sai_serialize_status(SAI_STATUS_SUCCESS); // we assume success

    entry.emplace_back("OBJECT_ID", sai_serialize_object_id(switchId));

    m_selectableChannel->set(strStatus, entry, "switch_id_query_response");
}

void Proxy::processQueryApiVersion(
        _In_ const swss::KeyOpFieldsValuesTuple &kco)
{
    SWSS_LOG_ENTER();

    sai_api_version_t version;

    sai_status_t status = m_vendorSai->queryApiVersion(&version);

    std::vector<swss::FieldValueTuple> entry;

    if (status == SAI_STATUS_SUCCESS)
    {
        entry.emplace_back("VERSION", std::to_string(version));

        SWSS_LOG_NOTICE("query api version returned: %ld", version);
    }

    auto strStatus = sai_serialize_status(status);

    m_selectableChannel->set(strStatus, entry, "query_api_version_response");
}

void Proxy::processLogSet(
        _In_ const swss::KeyOpFieldsValuesTuple &kco)
{
    SWSS_LOG_ENTER();

    auto& key = kfvKey(kco);

    auto strApi = key.substr(0, key.find(":"));
    auto strLogLevel = key.substr(key.find(":") + 1);

    sai_api_t api;
    sai_deserialize_api(strApi, api);

    sai_log_level_t level;
    sai_deserialize_log_level(strLogLevel, level);

    sai_status_t status = m_vendorSai->logSet(api, level);

    auto strStatus = sai_serialize_status(status);

    m_selectableChannel->set(strStatus, {}, "log_set_response");
}

void Proxy::processGetStats(
        _In_ const swss::KeyOpFieldsValuesTuple &kco)
{
    SWSS_LOG_ENTER();

    const std::string& key = kfvKey(kco);

    sai_object_meta_key_t metaKey;
    sai_deserialize_object_meta_key(key, metaKey);

    if (!sai_metadata_is_object_type_valid(metaKey.objecttype))
    {
        SWSS_LOG_THROW("invalid object type %s", key.c_str());
    }

    auto oi = sai_metadata_get_object_type_info(metaKey.objecttype);

    if (oi == NULL)
    {
        SWSS_LOG_ERROR("invalid object type: %s", sai_serialize_object_type(metaKey.objecttype).c_str());

        std::string strStatus = sai_serialize_status(SAI_STATUS_FAILURE);

        m_selectableChannel->set(strStatus, {}, "get_stats_response");

        return;
    }

    auto &values = kfvFieldsValues(kco);

    uint32_t numberOfCounters = (uint32_t)values.size();

    std::vector<sai_stat_id_t> counterIds;
    std::vector<uint64_t> counters;

    for (const auto&fv: values)
    {
        sai_stat_id_t stat;
        sai_deserialize_enum(fvField(fv), oi->statenum, (int32_t&)stat);

        counters.push_back(stat);
    }

    auto status = m_vendorSai->getStats(
            metaKey.objecttype,
            metaKey.objectkey.key.object_id,
            numberOfCounters,
            counterIds.data(),
            counters.data());

    std::vector<swss::FieldValueTuple> entry;

    if (status == SAI_STATUS_SUCCESS)
    {
        for (auto c: counters)
        {
            entry.emplace_back("", std::to_string(c));
        }
    }

    std::string strStatus = sai_serialize_status(status);

    m_selectableChannel->set(strStatus, entry, "get_stats_response");
}

void Proxy::processGetStatsExt(
        _In_ const swss::KeyOpFieldsValuesTuple &kco)
{
    SWSS_LOG_ENTER();

    const std::string& key = kfvKey(kco);

    sai_object_meta_key_t metaKey;
    sai_deserialize_object_meta_key(key, metaKey);

    if (!sai_metadata_is_object_type_valid(metaKey.objecttype))
    {
        SWSS_LOG_THROW("invalid object type %s", key.c_str());
    }

    auto oi = sai_metadata_get_object_type_info(metaKey.objecttype);

    if (oi == NULL)
    {
        SWSS_LOG_ERROR("invalid object type: %s", sai_serialize_object_type(metaKey.objecttype).c_str());

        std::string strStatus = sai_serialize_status(SAI_STATUS_FAILURE);

        m_selectableChannel->set(strStatus, {}, "get_stats_ext_response");

        return;
    }

    auto values = kfvFieldsValues(kco);

    sai_stats_mode_t mode = (sai_stats_mode_t)stoull(fvValue(values.back()));

    values.pop_back();

    uint32_t numberOfCounters = (uint32_t)values.size();

    std::vector<sai_stat_id_t> counterIds;
    std::vector<uint64_t> counters;

    for (const auto&fv: values)
    {
        sai_stat_id_t stat;
        sai_deserialize_enum(fvField(fv), oi->statenum, (int32_t&)stat);

        counters.push_back(stat);
    }

    auto status = m_vendorSai->getStatsExt(
            metaKey.objecttype,
            metaKey.objectkey.key.object_id,
            numberOfCounters,
            counterIds.data(),
            mode,
            counters.data());

    std::vector<swss::FieldValueTuple> entry;

    if (status == SAI_STATUS_SUCCESS)
    {
        for (auto c: counters)
        {
            entry.emplace_back("", std::to_string(c));
        }
    }

    std::string strStatus = sai_serialize_status(status);

    m_selectableChannel->set(strStatus, entry, "get_stats_ext_response");
}

void Proxy::processClearStats(
        _In_ const swss::KeyOpFieldsValuesTuple &kco)
{
    SWSS_LOG_ENTER();

    const std::string& key = kfvKey(kco);

    sai_object_meta_key_t metaKey;
    sai_deserialize_object_meta_key(key, metaKey);

    if (!sai_metadata_is_object_type_valid(metaKey.objecttype))
    {
        SWSS_LOG_THROW("invalid object type %s", key.c_str());
    }

    auto oi = sai_metadata_get_object_type_info(metaKey.objecttype);

    if (oi == NULL)
    {
        SWSS_LOG_ERROR("invalid object type: %s", sai_serialize_object_type(metaKey.objecttype).c_str());

        std::string strStatus = sai_serialize_status(SAI_STATUS_FAILURE);

        m_selectableChannel->set(strStatus, {}, "clear_stats_response");

        return;
    }

    auto &values = kfvFieldsValues(kco);

    uint32_t numberOfCounters = (uint32_t)values.size();

    std::vector<sai_stat_id_t> counterIds;
    std::vector<uint64_t> counters;

    for (const auto&fv: values)
    {
        sai_stat_id_t stat;
        sai_deserialize_enum(fvField(fv), oi->statenum, (int32_t&)stat);

        counters.push_back(stat);
    }

    auto status = m_vendorSai->clearStats(
            metaKey.objecttype,
            metaKey.objectkey.key.object_id,
            numberOfCounters,
            counterIds.data());

    std::vector<swss::FieldValueTuple> entry;

    std::string strStatus = sai_serialize_status(status);

    m_selectableChannel->set(strStatus, entry, "clear_stats_response");
}

void Proxy::updateAttributteNotificationPointers(
        _In_ uint32_t count,
        _Inout_ sai_attribute_t* attr_list)
{
    SWSS_LOG_ENTER();

    sai_metadata_update_attribute_notification_pointers(&m_sn, count, attr_list);
}

// TODO move to notification handler class

void Proxy::onFdbEvent(
        _In_ uint32_t count,
        _In_ const sai_fdb_event_notification_data_t *data)
{
    SWSS_LOG_ENTER();

    std::string s = sai_serialize_fdb_event_ntf(count, data);

    sendNotification(SAI_SWITCH_NOTIFICATION_NAME_FDB_EVENT, s);
}

void Proxy::onNatEvent(
        _In_ uint32_t count,
        _In_ const sai_nat_event_notification_data_t *data)
{
    SWSS_LOG_ENTER();

    std::string s = sai_serialize_nat_event_ntf(count, data);

    sendNotification(SAI_SWITCH_NOTIFICATION_NAME_NAT_EVENT, s);
}

void Proxy::onPortStateChange(
        _In_ uint32_t count,
        _In_ const sai_port_oper_status_notification_t *data)
{
    SWSS_LOG_ENTER();

    auto ntfdata = syncd::Workaround::convertPortOperStatusNotification(count, data, m_apiVersion);

    auto s = sai_serialize_port_oper_status_ntf((uint32_t)ntfdata.size(), ntfdata.data());

    sendNotification(SAI_SWITCH_NOTIFICATION_NAME_PORT_STATE_CHANGE, s);
}

void Proxy::onPortHostTxReady(
        _In_ sai_object_id_t switch_id,
        _In_ sai_object_id_t port_id,
        _In_ sai_port_host_tx_ready_status_t host_tx_ready_status)
{
    SWSS_LOG_ENTER();

    auto s = sai_serialize_port_host_tx_ready_ntf(switch_id, port_id, host_tx_ready_status);

    sendNotification(SAI_SWITCH_NOTIFICATION_NAME_PORT_HOST_TX_READY, s);
}

void Proxy::onQueuePfcDeadlock(
        _In_ uint32_t count,
        _In_ const sai_queue_deadlock_notification_data_t *data)
{
    SWSS_LOG_ENTER();

    auto s = sai_serialize_queue_deadlock_ntf(count, data);

    sendNotification(SAI_SWITCH_NOTIFICATION_NAME_QUEUE_PFC_DEADLOCK, s);
}

void Proxy::onSwitchShutdownRequest(
        _In_ sai_object_id_t switch_id)
{
    SWSS_LOG_ENTER();

    auto s = sai_serialize_switch_shutdown_request(switch_id);

    sendNotification(SAI_SWITCH_NOTIFICATION_NAME_SWITCH_SHUTDOWN_REQUEST, s);
}

void Proxy::onSwitchAsicSdkHealthEvent(
        _In_ sai_object_id_t switch_id,
        _In_ sai_switch_asic_sdk_health_severity_t severity,
        _In_ sai_timespec_t timestamp,
        _In_ sai_switch_asic_sdk_health_category_t category,
        _In_ sai_switch_health_data_t data,
        _In_ const sai_u8_list_t description)
{
    SWSS_LOG_ENTER();

    std::string s = sai_serialize_switch_asic_sdk_health_event(switch_id, severity, timestamp, category, data, description);

    sendNotification(SAI_SWITCH_NOTIFICATION_NAME_SWITCH_ASIC_SDK_HEALTH_EVENT, s);
}

void Proxy::onSwitchStateChange(
        _In_ sai_object_id_t switch_id,
        _In_ sai_switch_oper_status_t switch_oper_status)
{
    SWSS_LOG_ENTER();

    auto s = sai_serialize_switch_oper_status(switch_id, switch_oper_status);

    sendNotification(SAI_SWITCH_NOTIFICATION_NAME_SWITCH_STATE_CHANGE, s);
}

void Proxy::onBfdSessionStateChange(
        _In_ uint32_t count,
        _In_ const sai_bfd_session_state_notification_t *data)
{
    SWSS_LOG_ENTER();

    std::string s = sai_serialize_bfd_session_state_ntf(count, data);

    sendNotification(SAI_SWITCH_NOTIFICATION_NAME_BFD_SESSION_STATE_CHANGE, s);
}

void Proxy::onTwampSessionEvent(
        _In_ uint32_t count,
        _In_ const sai_twamp_session_event_notification_data_t *data)
{
    SWSS_LOG_ENTER();

    std::string s = sai_serialize_twamp_session_event_ntf(count, data);

    sendNotification(SAI_SWITCH_NOTIFICATION_NAME_TWAMP_SESSION_EVENT, s);
}

void Proxy::onTamTelTypeConfigChange(
    _In_ sai_object_id_t tam_tel_id)
{
    SWSS_LOG_ENTER();

    std::string s = sai_serialize_object_id(tam_tel_id);

    sendNotification(SAI_SWITCH_NOTIFICATION_NAME_TAM_TEL_TYPE_CONFIG_CHANGE, s);
}

void Proxy::sendNotification(
        _In_ const std::string& op,
        _In_ const std::string& data)
{
    SWSS_LOG_ENTER();

    std::vector<swss::FieldValueTuple> entry;

    swss::KeyOpFieldsValuesTuple item(op, data, entry);

    SWSS_LOG_INFO("%s %s", op.c_str(), data.c_str());

    m_notificationsSentCount++;

    m_notifications->send(op, data, entry);
}

uint64_t Proxy::getNotificationsSentCount() const
{
    SWSS_LOG_ENTER();

    return m_notificationsSentCount;
}
