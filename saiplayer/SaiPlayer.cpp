#include "SaiPlayer.h"

#include "sairedis.h"
#include "sairediscommon.h"

#include "meta/sai_serialize.h"
#include "meta/SaiAttributeList.h"

#include "swss/logger.h"
#include "swss/tokenize.h"

#include <inttypes.h>
#include <getopt.h>
#include <unistd.h>
#include <string.h>

#include <iostream>
#include <stdexcept>
#include <sstream>
#include <string>

/*
 * Since this is player, we record actions from orchagent.  No special case
 * should be needed for switch in case it contains some oid values (like in
 * syncd cold restart) since orchagent should never create switch with oid
 * values set at creation time.
 */

using namespace saiplayer;
using namespace saimeta;
using namespace std::placeholders;

SaiPlayer::SaiPlayer(
        _In_ std::shared_ptr<sairedis::SaiInterface> sai,
        _In_ std::shared_ptr<CommandLineOptions> cmd):
    m_sai(sai),
    m_commandLineOptions(cmd)
{
    SWSS_LOG_ENTER();

    SWSS_LOG_NOTICE("cmd: %s", cmd->getCommandLineString().c_str());

    m_profileIter = m_profileMap.begin();

    m_smt.profileGetValue = std::bind(&SaiPlayer::profileGetValue, this, _1, _2);
    m_smt.profileGetNextValue = std::bind(&SaiPlayer::profileGetNextValue, this, _1, _2, _3);

    m_sn.onFdbEvent = std::bind(&SaiPlayer::onFdbEvent, this, _1, _2);
    m_sn.onPortStateChange = std::bind(&SaiPlayer::onPortStateChange, this, _1, _2);
    m_sn.onQueuePfcDeadlock = std::bind(&SaiPlayer::onQueuePfcDeadlock, this, _1, _2);
    m_sn.onSwitchShutdownRequest = std::bind(&SaiPlayer::onSwitchShutdownRequest, this, _1);
    m_sn.onSwitchStateChange = std::bind(&SaiPlayer::onSwitchStateChange, this, _1, _2);

    m_switchNotifications= m_sn.getSwitchNotifications();
}

SaiPlayer::~SaiPlayer()
{
    SWSS_LOG_ENTER();

    // empty
}

void SaiPlayer::onFdbEvent(
        _In_ uint32_t count,
        _In_ const sai_fdb_event_notification_data_t *data)
{
    SWSS_LOG_ENTER();

    // empty
}

void SaiPlayer::onPortStateChange(
        _In_ uint32_t count,
        _In_ const sai_port_oper_status_notification_t *data)
{
    SWSS_LOG_ENTER();

    // empty
}

void SaiPlayer::onQueuePfcDeadlock(
        _In_ uint32_t count,
        _In_ const sai_queue_deadlock_notification_data_t *data)
{
    SWSS_LOG_ENTER();

    // empty
}

void SaiPlayer::onSwitchShutdownRequest(
        _In_ sai_object_id_t switch_id)
{
    SWSS_LOG_ENTER();

    SWSS_LOG_ERROR("got shutdown request, syncd failed!");
    exit(EXIT_FAILURE);
}

void SaiPlayer::onSwitchStateChange(
        _In_ sai_object_id_t switch_id,
        _In_ sai_switch_oper_status_t switch_oper_status)
{
    SWSS_LOG_ENTER();

    // empty
}

#define EXIT_ON_ERROR(x)\
{\
    sai_status_t s = (x);\
    if (s != SAI_STATUS_SUCCESS)\
    {\
        SWSS_LOG_THROW("fail status: %s", sai_serialize_status(s).c_str());\
    }\
}

sai_object_id_t SaiPlayer::translate_local_to_redis(
        _In_ sai_object_id_t rid)
{
    SWSS_LOG_ENTER();

    SWSS_LOG_DEBUG("translating local RID %s",
            sai_serialize_object_id(rid).c_str());

    if (rid == SAI_NULL_OBJECT_ID)
    {
        return SAI_NULL_OBJECT_ID;
    }

    auto it = m_local_to_redis.find(rid);

    if (it == m_local_to_redis.end())
    {
        SWSS_LOG_THROW("failed to translate local RID %s",
                sai_serialize_object_id(rid).c_str());
    }

    return it->second;
}

void SaiPlayer::translate_local_to_redis(
        _Inout_ sai_object_list_t& element)
{
    SWSS_LOG_ENTER();

    for (uint32_t i = 0; i < element.count; i++)
    {
        element.list[i] = translate_local_to_redis(element.list[i]);
    }
}

void SaiPlayer::translate_local_to_redis(
        _In_ sai_object_type_t object_type,
        _In_ uint32_t attr_count,
        _In_ sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    for (uint32_t i = 0; i < attr_count; i++)
    {
        sai_attribute_t &attr = attr_list[i];

        auto meta = sai_metadata_get_attr_metadata(object_type, attr.id);

        if (meta == NULL)
        {
            SWSS_LOG_THROW("unable to get metadata for object type %s, attribute %d",
                    sai_serialize_object_type(object_type).c_str(),
                    attr.id);
        }

        switch (meta->attrvaluetype)
        {
            case SAI_ATTR_VALUE_TYPE_OBJECT_ID:
                attr.value.oid = translate_local_to_redis(attr.value.oid);
                break;

            case SAI_ATTR_VALUE_TYPE_OBJECT_LIST:
                translate_local_to_redis(attr.value.objlist);
                break;

            case SAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_OBJECT_ID:
                if (attr.value.aclfield.enable)
                    attr.value.aclfield.data.oid = translate_local_to_redis(attr.value.aclfield.data.oid);
                break;

            case SAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_OBJECT_LIST:
                if (attr.value.aclfield.enable)
                    translate_local_to_redis( attr.value.aclfield.data.objlist);
                break;

            case SAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_OBJECT_ID:
                if (attr.value.aclaction.enable)
                    attr.value.aclaction.parameter.oid = translate_local_to_redis(attr.value.aclaction.parameter.oid);
                break;

            case SAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_OBJECT_LIST:
                if (attr.value.aclaction.enable)
                    translate_local_to_redis(attr.value.aclaction.parameter.objlist);
                break;

            default:

                if (meta->isoidattribute)
                {
                    SWSS_LOG_THROW("attribute %s is oid attribute but not handled, FIXME", meta->attridname);
                }

                break;
        }
    }
}

sai_object_type_t SaiPlayer::deserialize_object_type(
        _In_ const std::string& s)
{
    SWSS_LOG_ENTER();

    sai_object_type_t object_type;

    sai_deserialize_object_type(s, object_type);

    return object_type;
}

const std::vector<swss::FieldValueTuple> SaiPlayer::get_values(
        _In_ const std::vector<std::string>& items)
{
    SWSS_LOG_ENTER();

    std::vector<swss::FieldValueTuple> values;

    // timestamp|action|objecttype:objectid|attrid=value,...
    for (size_t i = 3; i <items.size(); ++i)
    {
        const std::string& item = items[i];

        auto start = item.find_first_of("=");

        auto field = item.substr(0, start);
        auto value = item.substr(start + 1);

        swss::FieldValueTuple entry(field, value);

        values.push_back(entry);
    }

    return values;
}

#define CHECK_LIST(x)                           \
    if (attr.x.count != get_attr.x.count) {     \
        SWSS_LOG_THROW("get response list count not match recording %u vs %u (expected)", get_attr.x.count, attr.x.count); }

void SaiPlayer::match_list_lengths(
        _In_ sai_object_type_t object_type,
        _In_ uint32_t get_attr_count,
        _In_ sai_attribute_t* get_attr_list,
        _In_ uint32_t attr_count,
        _In_ sai_attribute_t* attr_list)
{
    SWSS_LOG_ENTER();

    if (get_attr_count != attr_count)
    {
        SWSS_LOG_THROW("list number don't match %u != %u", get_attr_count, attr_count);
    }

    for (uint32_t i = 0; i < attr_count; ++i)
    {
        sai_attribute_t &get_attr = get_attr_list[i];
        sai_attribute_t &attr = attr_list[i];

        auto meta = sai_metadata_get_attr_metadata(object_type, attr.id);

        if (meta == NULL)
        {
            SWSS_LOG_THROW("unable to get metadata for object type %s, attribute %d",
                    sai_serialize_object_type(object_type).c_str(),
                    attr.id);
        }

        switch (meta->attrvaluetype)
        {
            case SAI_ATTR_VALUE_TYPE_OBJECT_LIST:
                CHECK_LIST(value.objlist);
                break;

            case SAI_ATTR_VALUE_TYPE_UINT8_LIST:
                CHECK_LIST(value.u8list);
                break;

            case SAI_ATTR_VALUE_TYPE_INT8_LIST:
                CHECK_LIST(value.s8list);
                break;

            case SAI_ATTR_VALUE_TYPE_UINT16_LIST:
                CHECK_LIST(value.u16list);
                break;

            case SAI_ATTR_VALUE_TYPE_INT16_LIST:
                CHECK_LIST(value.s16list);
                break;

            case SAI_ATTR_VALUE_TYPE_UINT32_LIST:
                CHECK_LIST(value.u32list);
                break;

            case SAI_ATTR_VALUE_TYPE_INT32_LIST:
                CHECK_LIST(value.s32list);
                break;

            case SAI_ATTR_VALUE_TYPE_VLAN_LIST:
                CHECK_LIST(value.vlanlist);
                break;

            case SAI_ATTR_VALUE_TYPE_QOS_MAP_LIST:
                CHECK_LIST(value.qosmap);
                break;

            case SAI_ATTR_VALUE_TYPE_IP_ADDRESS_LIST:
                CHECK_LIST(value.ipaddrlist);
                break;

            case SAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_OBJECT_LIST:
                CHECK_LIST(value.aclfield.data.objlist);
                break;

            case SAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_UINT8_LIST:
                CHECK_LIST(value.aclfield.data.u8list);
                CHECK_LIST(value.aclfield.mask.u8list);
                break;

            case SAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_OBJECT_LIST:
                CHECK_LIST(value.aclaction.parameter.objlist);
                break;

            default:
                break;
        }
    }
}

void SaiPlayer::match_redis_with_rec(
        _In_ sai_object_id_t get_oid,
        _In_ sai_object_id_t oid)
{
    SWSS_LOG_ENTER();

    auto it = m_redis_to_local.find(get_oid);

    if (it == m_redis_to_local.end())
    {
        m_redis_to_local[get_oid] = oid;
        m_local_to_redis[oid] = get_oid;
    }

    if (oid != m_redis_to_local[get_oid])
    {
        SWSS_LOG_THROW("match failed, oid order is mismatch :( oid 0x%" PRIx64 " get_oid 0x%" PRIx64 " second 0x%" PRIx64,
                oid,
                get_oid,
                m_redis_to_local[get_oid]);
    }

    SWSS_LOG_DEBUG("map size: %zu", m_local_to_redis.size());
}

void SaiPlayer::match_redis_with_rec(
        _In_ sai_object_list_t get_objlist,
        _In_ sai_object_list_t objlist)
{
    SWSS_LOG_ENTER();

    for (uint32_t i = 0 ; i < get_objlist.count; ++i)
    {
        match_redis_with_rec(get_objlist.list[i], objlist.list[i]);
    }
}

void SaiPlayer::match_redis_with_rec(
        _In_ sai_object_type_t object_type,
        _In_ uint32_t get_attr_count,
        _In_ sai_attribute_t* get_attr_list,
        _In_ uint32_t attr_count,
        _In_ sai_attribute_t* attr_list)
{
    SWSS_LOG_ENTER();

    if (get_attr_count != attr_count)
    {
        SWSS_LOG_THROW("list number don't match %u != %u", get_attr_count, attr_count);
    }

    for (uint32_t i = 0; i < attr_count; ++i)
    {
        sai_attribute_t &get_attr = get_attr_list[i];
        sai_attribute_t &attr = attr_list[i];

        auto meta = sai_metadata_get_attr_metadata(object_type, attr.id);

        if (meta == NULL)
        {
            SWSS_LOG_THROW("unable to get metadata for object type %s, attribute %d",
                    sai_serialize_object_type(object_type).c_str(),
                    attr.id);
        }

        switch (meta->attrvaluetype)
        {
            case SAI_ATTR_VALUE_TYPE_OBJECT_ID:
                match_redis_with_rec(get_attr.value.oid, attr.value.oid);
                break;

            case SAI_ATTR_VALUE_TYPE_OBJECT_LIST:
                match_redis_with_rec(get_attr.value.objlist, attr.value.objlist);
                break;

            case SAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_OBJECT_ID:
                if (attr.value.aclfield.enable)
                    match_redis_with_rec(get_attr.value.aclfield.data.oid, attr.value.aclfield.data.oid);
                break;

            case SAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_OBJECT_LIST:
                if (attr.value.aclfield.enable)
                    match_redis_with_rec(get_attr.value.aclfield.data.objlist, attr.value.aclfield.data.objlist);
                break;

            case SAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_OBJECT_ID:
                if (attr.value.aclaction.enable)
                    match_redis_with_rec(get_attr.value.aclaction.parameter.oid, attr.value.aclaction.parameter.oid);
                break;

            case SAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_OBJECT_LIST:
                if (attr.value.aclaction.enable)
                    match_redis_with_rec(get_attr.value.aclaction.parameter.objlist, attr.value.aclaction.parameter.objlist);
                break;

            default:

                // XXX if (meta->isoidattribute)
                if (meta->allowedobjecttypeslength > 0)
                {
                    SWSS_LOG_THROW("attribute %s is oid attribute but not handled, FIXME", meta->attridname);
                }

                break;
        }
    }
}

sai_status_t SaiPlayer::handle_fdb(
        _In_ const std::string &str_object_id,
        _In_ sai_common_api_t api,
        _In_ uint32_t attr_count,
        _In_ sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    sai_fdb_entry_t fdb_entry;
    sai_deserialize_fdb_entry(str_object_id, fdb_entry);

    fdb_entry.switch_id = translate_local_to_redis(fdb_entry.switch_id);
    fdb_entry.bv_id = translate_local_to_redis(fdb_entry.bv_id);

    switch (api)
    {
        case SAI_COMMON_API_CREATE:
            return m_sai->create(&fdb_entry, attr_count, attr_list);

        case SAI_COMMON_API_REMOVE:
            return m_sai->remove(&fdb_entry);

        case SAI_COMMON_API_SET:
            return m_sai->set(&fdb_entry, attr_list);

        case SAI_COMMON_API_GET:
            return m_sai->get(&fdb_entry, attr_count, attr_list);

        default:
            SWSS_LOG_THROW("fdb other apis not implemented");
    }

    return SAI_STATUS_SUCCESS;
}

sai_status_t SaiPlayer::handle_neighbor(
        _In_ const std::string &str_object_id,
        _In_ sai_common_api_t api,
        _In_ uint32_t attr_count,
        _In_ sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    sai_neighbor_entry_t neighbor_entry;
    sai_deserialize_neighbor_entry(str_object_id, neighbor_entry);

    neighbor_entry.switch_id = translate_local_to_redis(neighbor_entry.switch_id);
    neighbor_entry.rif_id = translate_local_to_redis(neighbor_entry.rif_id);

    switch(api)
    {
        case SAI_COMMON_API_CREATE:
            return m_sai->create(&neighbor_entry, attr_count, attr_list);

        case SAI_COMMON_API_REMOVE:
            return m_sai->remove(&neighbor_entry);

        case SAI_COMMON_API_SET:
            return m_sai->set(&neighbor_entry, attr_list);

        case SAI_COMMON_API_GET:
            return m_sai->get(&neighbor_entry, attr_count, attr_list);

        default:
            SWSS_LOG_THROW("neighbor other apis not implemented");
    }
}

sai_status_t SaiPlayer::handle_route(
        _In_ const std::string &str_object_id,
        _In_ sai_common_api_t api,
        _In_ uint32_t attr_count,
        _In_ sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    sai_route_entry_t route_entry;
    sai_deserialize_route_entry(str_object_id, route_entry);

    route_entry.switch_id = translate_local_to_redis(route_entry.switch_id);
    route_entry.vr_id = translate_local_to_redis(route_entry.vr_id);

    switch(api)
    {
        case SAI_COMMON_API_CREATE:
            return m_sai->create(&route_entry, attr_count, attr_list);

        case SAI_COMMON_API_REMOVE:
            return m_sai->remove(&route_entry);

        case SAI_COMMON_API_SET:
            return m_sai->set(&route_entry, attr_list);

        case SAI_COMMON_API_GET:
            return m_sai->get(&route_entry, attr_count, attr_list);

        default:
            SWSS_LOG_THROW("route other apis not implemented");
    }
}

void SaiPlayer::update_notifications_pointers(
        _In_ uint32_t attr_count,
        _Inout_ sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    /*
     * Sairedis is updating notifications pointers based on attribute, so when
     * we will do replay it will have invalid pointers from orchagent, so we
     * need to override them after create, and after set.
     *
     * NOTE: This needs to be updated every time new pointer will be added.
     */

    for (uint32_t index = 0; index < attr_count; ++index)
    {
        sai_attribute_t &attr = attr_list[index];

        auto meta = sai_metadata_get_attr_metadata(SAI_OBJECT_TYPE_SWITCH, attr.id);

        if (meta->attrvaluetype != SAI_ATTR_VALUE_TYPE_POINTER)
        {
            continue;
        }

        if (attr.value.ptr == nullptr) // allow nulls
            continue;

        switch (attr.id)
        {
            case SAI_SWITCH_ATTR_SWITCH_STATE_CHANGE_NOTIFY:
                attr.value.ptr = (void*)&m_switchNotifications.on_switch_state_change;
                break;

            case SAI_SWITCH_ATTR_SHUTDOWN_REQUEST_NOTIFY:
                attr.value.ptr = (void*)&m_switchNotifications.on_switch_shutdown_request;
                break;

            case SAI_SWITCH_ATTR_FDB_EVENT_NOTIFY:
                attr.value.ptr = (void*)&m_switchNotifications.on_fdb_event;
                break;

            case SAI_SWITCH_ATTR_PORT_STATE_CHANGE_NOTIFY:
                attr.value.ptr = (void*)&m_switchNotifications.on_port_state_change;
                break;

            case SAI_SWITCH_ATTR_QUEUE_PFC_DEADLOCK_NOTIFY:
                attr.value.ptr = (void*)&m_switchNotifications.on_queue_pfc_deadlock;
                break;

            default:
                SWSS_LOG_ERROR("pointer for %s is not handled, FIXME!", meta->attridname);
                break;
        }
    }
}

sai_status_t SaiPlayer::handle_generic(
        _In_ sai_object_type_t object_type,
        _In_ const std::string &str_object_id,
        _In_ sai_common_api_t api,
        _In_ uint32_t attr_count,
        _In_ sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    sai_object_id_t local_id;
    sai_deserialize_object_id(str_object_id, local_id);

    SWSS_LOG_DEBUG("generic %s for %s:%s",
            sai_serialize_common_api(api).c_str(),
            sai_serialize_object_type(object_type).c_str(),
            str_object_id.c_str());

    sai_object_meta_key_t meta_key;

    meta_key.objecttype = object_type;

    switch (api)
    {
        case SAI_COMMON_API_CREATE:

            {
                sai_object_id_t switch_id = m_sai->switchIdQuery(local_id);

                if (switch_id == SAI_NULL_OBJECT_ID)
                {
                    SWSS_LOG_THROW("invalid switch_id translated from VID %s",
                            sai_serialize_object_id(local_id).c_str());
                }

                if (object_type == SAI_OBJECT_TYPE_SWITCH)
                {
                    update_notifications_pointers(attr_count, attr_list);

                    /*
                     * We are creating switch, in both cases local and redis
                     * switch id is deterministic and should be the same.
                     */
                }
                else
                {
                    /*
                     * When we creating switch, then switch_id parameter is
                     * ignored, but we can't convert it using vid to rid map,
                     * since rid don't exist yet, so skip translate for switch,
                     * but use translate for all other objects.
                     */

                    switch_id = translate_local_to_redis(switch_id);
                }

                sai_status_t status = m_sai->create(meta_key, switch_id, attr_count, attr_list);

                if (status == SAI_STATUS_SUCCESS)
                {
                    sai_object_id_t rid = meta_key.objectkey.key.object_id;

                    match_redis_with_rec(rid, local_id);

                    SWSS_LOG_INFO("saved VID %s to RID %s",
                            sai_serialize_object_id(local_id).c_str(),
                            sai_serialize_object_id(rid).c_str());
                }
                else
                {
                    SWSS_LOG_ERROR("failed to create %s",
                            sai_serialize_status(status).c_str());
                }

                return status;
            }

        case SAI_COMMON_API_REMOVE:

            {
                meta_key.objectkey.key.object_id = translate_local_to_redis(local_id);

                return m_sai->remove(meta_key);
            }

        case SAI_COMMON_API_SET:

            {
                if (object_type == SAI_OBJECT_TYPE_SWITCH)
                {
                    update_notifications_pointers(1, attr_list);
                }

                meta_key.objectkey.key.object_id = translate_local_to_redis(local_id);

                return m_sai->set(meta_key, attr_list);
            }

        case SAI_COMMON_API_GET:

            {
                meta_key.objectkey.key.object_id = translate_local_to_redis(local_id);

                return m_sai->get(meta_key, attr_count, attr_list);
            }

        default:
            SWSS_LOG_THROW("generic other apis not implemented");
    }
}

void SaiPlayer::handle_get_response(
        _In_ sai_object_type_t object_type,
        _In_ uint32_t get_attr_count,
        _In_ sai_attribute_t* get_attr_list,
        _In_ const std::string& response,
        _In_ sai_status_t status)
{
    SWSS_LOG_ENTER();

    // timestamp|action|objecttype:objectid|attrid=value,...
    auto v = swss::tokenize(response, '|');

    if (status != SAI_STATUS_SUCCESS)
    {
        sai_status_t expectedStatus;
        sai_deserialize_status(v.at(2), expectedStatus);

        if (status == expectedStatus)
        {
            // GET api was not successful but status is equal to recording
            return;
        }

        SWSS_LOG_WARN("status is: %s but expected: %s",
                sai_serialize_status(status).c_str(),
                sai_serialize_status(expectedStatus).c_str());
        return;
    }

    //std::cout << "processing " << response << std::endl;

    auto values = get_values(v);

    SaiAttributeList list(object_type, values, false);

    sai_attribute_t *attr_list = list.get_attr_list();
    uint32_t attr_count = list.get_attr_count();

    match_list_lengths(object_type, get_attr_count, get_attr_list, attr_count, attr_list);

    SWSS_LOG_DEBUG("list match");

    match_redis_with_rec(object_type, get_attr_count, get_attr_list, attr_count, attr_list);

    // NOTE: Primitive values are not matched (recording vs switch/vs), we can add that check
}

void SaiPlayer::performSleep(
        _In_ const std::string& line)
{
    SWSS_LOG_ENTER();

    // timestamp|action|sleeptime
    auto v = swss::tokenize(line, '|');

    if (v.size() < 3)
    {
        SWSS_LOG_THROW("invalid line %s", line.c_str());
    }

    uint32_t useconds;
    sai_deserialize_number(v[2], useconds);

    if (useconds > 0)
    {
        useconds *= 1000; // 1ms resolution is enough for sleep

        SWSS_LOG_NOTICE("usleep(%u)", useconds);
        usleep(useconds);
    }
}

void SaiPlayer::performNotifySyncd(
        _In_ const std::string& request, 
        _In_ const std::string& response)
{
    SWSS_LOG_ENTER();

    // timestamp|action|data
    auto r = swss::tokenize(request, '|');
    auto R = swss::tokenize(response, '|');

    if (r[1] != "a" || R[1] != "A")
    {
        SWSS_LOG_THROW("invalid syncd notify request/response %s/%s", request.c_str(), response.c_str());
    }

    if (m_commandLineOptions->m_skipNotifySyncd)
    {
        SWSS_LOG_NOTICE("skipping notify syncd, selected by user");
        return;
    }

    // tell syncd that we are compiling new view
    sai_attribute_t attr;
    attr.id = SAI_REDIS_SWITCH_ATTR_NOTIFY_SYNCD;
    attr.value.s32 = sai_deserialize_redis_notify_syncd(r[2]);

    /*
     * NOTE: We don't need actual switch to set those attributes.
     */

    sai_object_id_t switch_id = SAI_NULL_OBJECT_ID;

    sai_status_t status = m_sai->set(SAI_OBJECT_TYPE_SWITCH, switch_id, &attr);

    const std::string& responseStatus = R[2];

    sai_status_t response_status;
    sai_deserialize_status(responseStatus, response_status);

    if (status != response_status)
    {
        SWSS_LOG_THROW("response status %s is different than syncd status %s",
                responseStatus.c_str(),
                sai_serialize_status(status).c_str());
    }

    if (status != SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_THROW("failed to notify syncd %s",
                sai_serialize_status(status).c_str());
    }

    // OK
}

void SaiPlayer::performFdbFlush(
        _In_ const std::string& request,
        _In_ const std::string response)
{
    SWSS_LOG_ENTER();

    // 2017-05-13.20:47:24.883499|f|SAI_OBJECT_TYPE_FDB_FLUSH:oid:0x21000000000000|
    // 2017-05-13.20:47:24.883499|F|SAI_STATUS_SUCCESS

    // timestamp|action|data
    auto r = swss::tokenize(request, '|');
    auto R = swss::tokenize(response, '|');

    if (r[1] != "f" || R[1] != "F")
    {
        SWSS_LOG_THROW("invalid fdb flush request/response %s/%s", request.c_str(), response.c_str());
    }

    if (r.size() > 3 && r[3].size() != 0)
    {
        SWSS_LOG_NOTICE("%zu %zu, %s", r.size(), r[3].size(), r[3].c_str());
        // TODO currently we support only flush fdb entries with no attributes
        SWSS_LOG_THROW("currently fdb flush supports only no attributes, but some given: %s", request.c_str());
    }

    // objecttype:objectid (object id may contain ':')
    auto& data = r[2];
    auto start = data.find_first_of(":");
    auto str_object_type = data.substr(0, start);
    auto str_object_id  = data.substr(start + 1);

    sai_object_type_t ot = deserialize_object_type(str_object_type);

    if (ot != SAI_OBJECT_TYPE_FDB_FLUSH)
    {
        SWSS_LOG_THROW("expected object type %s, but got: %s on %s",
                sai_serialize_object_type(SAI_OBJECT_TYPE_FDB_FLUSH).c_str(),
                str_object_type.c_str(),
                request.c_str());
    }

    sai_object_id_t local_switch_id;
    sai_deserialize_object_id(str_object_id, local_switch_id);

    if (m_sai->switchIdQuery(local_switch_id) != local_switch_id)
    {
        SWSS_LOG_THROW("fdb flush object is not switch id: %s, switch_id_query: %s",
                str_object_id.c_str(),
                sai_serialize_object_id(m_sai->switchIdQuery(local_switch_id)).c_str());
    }

    auto switch_id = translate_local_to_redis(local_switch_id);

    // TODO currently we support only flush fdb entries with no attributes
    sai_status_t status = m_sai->flushFdbEntries(switch_id, 0, NULL);

    // check status
    sai_status_t expected_status;
    sai_deserialize_status(R[2], expected_status);

    if (status != expected_status)
    {
        SWSS_LOG_THROW("fdb flush got status %s, but expecting: %s",
                sai_serialize_status(status).c_str(),
                R[2].c_str());
    }

    // fdb flush OK
}

std::vector<std::string> SaiPlayer::tokenize(
        _In_ std::string input,
        _In_ const std::string &delim)
{
    SWSS_LOG_ENTER();

    /*
     * input is modified so it can't be passed as reference
     */

    std::vector<std::string> tokens;

    size_t pos = 0;

    while ((pos = input.find(delim)) != std::string::npos)
    {
        std::string token = input.substr(0, pos);

        input.erase(0, pos + delim.length());
        tokens.push_back(token);
    }

    tokens.push_back(input);

    return tokens;
}

sai_status_t SaiPlayer::handle_bulk_route(
        _In_ const std::vector<std::string> &object_ids,
        _In_ sai_common_api_t api,
        _In_ const std::vector<std::shared_ptr<SaiAttributeList>> &attributes,
        _In_ const std::vector<sai_status_t> &recorded_statuses)
{
    SWSS_LOG_ENTER();

    std::vector<sai_route_entry_t> routes;

    for (size_t i = 0; i < object_ids.size(); ++i)
    {
        sai_route_entry_t route_entry;
        sai_deserialize_route_entry(object_ids[i], route_entry);

        route_entry.vr_id = translate_local_to_redis(route_entry.vr_id);

        routes.push_back(route_entry);

        SWSS_LOG_DEBUG("route: %s", object_ids[i].c_str());
    }

    std::vector<sai_status_t> statuses;

    statuses.resize(recorded_statuses.size());

    if (api == SAI_COMMON_API_BULK_SET)
    {
        /*
         * TODO: since SDK don't support bulk route api yet, we just use our
         * implementation, and later on we can switch to SDK api.
         *
         * TODO: we need to get operation type from recording, currently is not
         * serialized and it is hard coded here.
         */

        std::vector<sai_attribute_t> attrs;

        for (const auto &a: attributes)
        {
            /*
             * Set has only 1 attribute, so we can just join them nicely here.
             */

            attrs.push_back(a->get_attr_list()[0]);
        }

        sai_status_t status = m_sai->bulkSet(
                (uint32_t)routes.size(),
                routes.data(),
                attrs.data(),
                SAI_BULK_OP_ERROR_MODE_IGNORE_ERROR, // TODO we need to get that from recording
                statuses.data());

        if (status != SAI_STATUS_SUCCESS)
        {
            /*
             * Entire API fails, so no need to compare statuses.
             */

            return status;
        }

        for (size_t i = 0; i < statuses.size(); ++i)
        {
            if (statuses[i] != recorded_statuses[i])
            {
                /*
                 * If recorded statuses are different than received, throw
                 * exception since data don't match.
                 */

                SWSS_LOG_THROW("recorded status is %s but returned is %s on %s",
                        sai_serialize_status(recorded_statuses[i]).c_str(),
                        sai_serialize_status(statuses[i]).c_str(),
                        object_ids[i].c_str());
            }
        }

        return status;
    }
    else if (api == SAI_COMMON_API_BULK_CREATE)
    {
        std::vector<uint32_t> attr_count;

        std::vector<const sai_attribute_t*> attr_list;

        // route can have multiple attributes, so we need to handle them all
        for (const auto &alist: attributes)
        {
            attr_list.push_back(alist->get_attr_list());
            attr_count.push_back(alist->get_attr_count());
        }

        SWSS_LOG_NOTICE("executing BULK route create with %zu routes", attr_count.size());

        sai_status_t status = m_sai->bulkCreate(
                (uint32_t)routes.size(),
                routes.data(),
                attr_count.data(),
                attr_list.data(),
                SAI_BULK_OP_ERROR_MODE_IGNORE_ERROR, // TODO we need to get that from recording
                statuses.data());

        if (status != SAI_STATUS_SUCCESS)
        {
            // Entire API fails, so no need to compare statuses.
            return status;
        }

        for (size_t i = 0; i < statuses.size(); ++i)
        {
            if (statuses[i] != recorded_statuses[i])
            {
                /*
                 * If recorded statuses are different than received, throw
                 * exception since data don't match.
                 */

                SWSS_LOG_THROW("recorded status is %s but returned is %s on %s",
                        sai_serialize_status(recorded_statuses[i]).c_str(),
                        sai_serialize_status(statuses[i]).c_str(),
                        object_ids[i].c_str());
            }
        }

        return status;

    }
    else
    {
        SWSS_LOG_THROW("api %d is not supported in bulk route", api);
    }
}

void SaiPlayer::processBulk(
        _In_ sai_common_api_t api,
        _In_ const std::string &line)
{
    SWSS_LOG_ENTER();

    if (!line.size())
    {
        return;
    }

    if (api != SAI_COMMON_API_BULK_SET &&
            api != SAI_COMMON_API_BULK_CREATE)
    {
        SWSS_LOG_THROW("bulk common api %d is not supported yet, FIXME", api);
    }

    /*
     * Here we know we have bulk SET api
     */

    // timestamp|action|objecttype||objectid|attrid=value|...|status||objectid||objectid|attrid=value|...|status||...
    auto fields = tokenize(line, "||");

    auto first = fields.at(0); // timestamp|action|objecttype

    std::string str_object_type = swss::tokenize(first, '|').at(2);

    sai_object_type_t object_type = deserialize_object_type(str_object_type);

    std::vector<std::string> object_ids;

    std::vector<std::shared_ptr<SaiAttributeList>> attributes;

    std::vector<sai_status_t> statuses;

    for (size_t idx = 1; idx < fields.size(); ++idx)
    {
        // object_id|attr=value|...|status
        const std::string &joined = fields[idx];

        auto split = swss::tokenize(joined, '|');

        std::string str_object_id = split.front();

        object_ids.push_back(str_object_id);

        std::string str_status = split.back();

        sai_status_t status;

        sai_deserialize_status(str_status, status);

        statuses.push_back(status);

        std::vector<swss::FieldValueTuple> entries; // attributes per object id

        // skip front object_id and back status

        SWSS_LOG_DEBUG("processing: %s", joined.c_str());

        for (size_t i = 1; i < split.size() - 1; ++i)
        {
            const auto &item = split[i];

            auto start = item.find_first_of("=");

            auto field = item.substr(0, start);
            auto value = item.substr(start + 1);

            swss::FieldValueTuple entry(field, value);

            entries.push_back(entry);
        }

        // since now we converted this to proper list, we can extract attributes

        std::shared_ptr<SaiAttributeList> list =
            std::make_shared<SaiAttributeList>(object_type, entries, false);

        sai_attribute_t *attr_list = list->get_attr_list();

        uint32_t attr_count = list->get_attr_count();

        if (api != SAI_COMMON_API_BULK_GET)
        {
            translate_local_to_redis(object_type, attr_count, attr_list);
        }

        attributes.push_back(list);
    }

    sai_status_t status;

    switch (object_type)
    {
        case SAI_OBJECT_TYPE_ROUTE_ENTRY:
            status = handle_bulk_route(object_ids, api, attributes, statuses);
            break;

        default:

            SWSS_LOG_THROW("bulk op for %s is not supported yet, FIXME",
                    sai_serialize_object_type(object_type).c_str());
    }

    if (status != SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_THROW("failed to execute bulk api, FIXME");
    }
}

int SaiPlayer::replay()
{
    //swss::Logger::getInstance().setMinPrio(swss::Logger::SWSS_DEBUG);

    SWSS_LOG_ENTER();

    if (m_commandLineOptions->m_files.size() == 0)
    {
        fprintf(stderr, "ERR: need to specify filename\n");

        return -1;
    }

    auto filename = m_commandLineOptions->m_files.at(0);

    SWSS_LOG_NOTICE("using file: %s", filename.c_str());

    std::ifstream infile(filename);

    if (!infile.is_open())
    {
        SWSS_LOG_ERROR("failed to open file %s", filename.c_str());
        return -1;
    }

    std::string line;

    while (std::getline(infile, line))
    {
        // std::cout << "processing " << line << std::endl;

        sai_common_api_t api = SAI_COMMON_API_CREATE;

        auto p = line.find_first_of("|");

        char op = line[p+1];

        switch (op)
        {
            case 'a':
                {
                    std::string response;

                    do
                    {
                        // this line may be notification, we need to skip
                        if (!std::getline(infile, response))
                        {
                            SWSS_LOG_THROW("failed to read next file from file, previous: %s", line.c_str());
                        }
                    }
                    while (response[response.find_first_of("|") + 1] == 'n');

                    performNotifySyncd(line, response);
                }
                continue;

            case 'f':
                {
                    std::string response;

                    do
                    {
                        // this line may be notification, we need to skip
                        if (!std::getline(infile, response))
                        {
                            SWSS_LOG_THROW("failed to read next file from file, previous: %s", line.c_str());
                        }
                    }
                    while (response[response.find_first_of("|") + 1] == 'n');

                    performFdbFlush(line, response);
                }
                continue;

            case '@':
                performSleep(line);
                continue;
            case 'c':
                api = SAI_COMMON_API_CREATE;
                break;
            case 'r':
                api = SAI_COMMON_API_REMOVE;
                break;
            case 's':
                api = SAI_COMMON_API_SET;
                break;
            case 'S':
                processBulk(SAI_COMMON_API_BULK_SET, line);
                continue;
            case 'C':
                processBulk(SAI_COMMON_API_BULK_CREATE, line);
                continue;
            case 'g':
                api = SAI_COMMON_API_GET;
                break;
            case 'q':
                // TODO: implement SAI player support for query commands
                continue;
            case 'Q':
                continue; // skip over query responses
            case '#':
            case 'n':
                SWSS_LOG_INFO("skipping op %c line %s", op, line.c_str());
                continue; // skip comment and notification

            default:
                SWSS_LOG_THROW("unknown op %c on line %s", op, line.c_str());
        }

        // timestamp|action|objecttype:objectid|attrid=value,...
        auto fields = swss::tokenize(line, '|');

        // objecttype:objectid (object id may contain ':')
        auto start = fields[2].find_first_of(":");

        auto str_object_type = fields[2].substr(0, start);
        auto str_object_id  = fields[2].substr(start + 1);

        sai_object_type_t object_type = deserialize_object_type(str_object_type);

        auto values = get_values(fields);

        SaiAttributeList list(object_type, values, false);

        sai_attribute_t *attr_list = list.get_attr_list();

        uint32_t attr_count = list.get_attr_count();

        SWSS_LOG_DEBUG("attr count: %u", list.get_attr_count());

        if (api != SAI_COMMON_API_GET)
        {
            translate_local_to_redis(object_type, attr_count, attr_list);
        }

        sai_status_t status;

        auto info = sai_metadata_get_object_type_info(object_type);

        switch (object_type)
        {
            case SAI_OBJECT_TYPE_FDB_ENTRY:
                status = handle_fdb(str_object_id, api, attr_count, attr_list);
                break;

            case SAI_OBJECT_TYPE_NEIGHBOR_ENTRY:
                status = handle_neighbor(str_object_id, api, attr_count, attr_list);
                break;

            case SAI_OBJECT_TYPE_ROUTE_ENTRY:
                status = handle_route(str_object_id, api, attr_count, attr_list);
                break;

            default:

                if (info->isnonobjectid)
                {
                    SWSS_LOG_THROW("object %s:%s is non object id, but not handled, FIXME",
                            sai_serialize_object_type(object_type).c_str(),
                            str_object_id.c_str());
                }

                status = handle_generic(object_type, str_object_id, api, attr_count, attr_list);
                break;
        }

        if (status != SAI_STATUS_SUCCESS)
        {
            if (api == SAI_COMMON_API_GET)
            {
                // GET status is checked in handle response
            }
            else
                SWSS_LOG_THROW("failed to execute api: %c: %s", op, sai_serialize_status(status).c_str());
        }

        if (api == SAI_COMMON_API_GET)
        {
            std::string response;

            do
            {
                // this line may be notification, we need to skip
                std::getline(infile, response);
            }
            while (response[response.find_first_of("|") + 1] == 'n');

            try
            {
                handle_get_response(object_type, attr_count, attr_list, response, status);
            }
            catch (const std::exception &e)
            {
                SWSS_LOG_NOTICE("line: %s", line.c_str());
                SWSS_LOG_NOTICE("resp (expected): %s", response.c_str());
                SWSS_LOG_NOTICE("got: %s", sai_serialize_status(status).c_str());

                if (api == SAI_COMMON_API_GET && (status == SAI_STATUS_SUCCESS || status == SAI_STATUS_BUFFER_OVERFLOW))
                {
                    // log each get parameter
                    for (uint32_t i = 0; i < attr_count; ++i)
                    {
                        auto meta = sai_metadata_get_attr_metadata(object_type, attr_list[i].id);

                        auto val = sai_serialize_attr_value(*meta, attr_list[i]);

                        SWSS_LOG_NOTICE(" - %s:%s", meta->attridname, val.c_str());
                    }
                }

                exit(EXIT_FAILURE);
            }

            if (api == SAI_COMMON_API_GET && (status == SAI_STATUS_SUCCESS || status == SAI_STATUS_BUFFER_OVERFLOW))
            {
                // log each get parameter
                for (uint32_t i = 0; i < attr_count; ++i)
                {
                    auto meta = sai_metadata_get_attr_metadata(object_type, attr_list[i].id);

                    auto val = sai_serialize_attr_value(*meta, attr_list[i]);

                    SWSS_LOG_NOTICE(" - %s:%s", meta->attridname, val.c_str());
                }
            }
        }
    }

    infile.close();

    SWSS_LOG_NOTICE("finished replaying %s with SUCCESS", filename.c_str());

    if (m_commandLineOptions->m_sleep)
    {
        fprintf(stderr, "Reply SUCCESS, sleeping, watching for notifications\n");

        sleep(-1);
    }

    return 0;
}

const char* SaiPlayer::profileGetValue(
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

int SaiPlayer::profileGetNextValue(
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

int SaiPlayer::run()
{
    SWSS_LOG_ENTER();

    if (m_commandLineOptions->m_enableDebug)
    {
        swss::Logger::getInstance().setMinPrio(swss::Logger::SWSS_NOTICE);
    }

    m_test_services = m_smt.getServiceMethodTable();

    EXIT_ON_ERROR(m_sai->initialize(0, &m_test_services));

    sai_attribute_t attr;

    /*
     * Notice that we use null object id as switch id, which is fine since
     * those attributes don't need switch.
     */

    sai_object_id_t switch_id = SAI_NULL_OBJECT_ID;

    if (m_commandLineOptions->m_inspectAsic)
    {
        attr.id = SAI_REDIS_SWITCH_ATTR_NOTIFY_SYNCD;
        attr.value.s32 = SAI_REDIS_NOTIFY_SYNCD_INSPECT_ASIC;

        EXIT_ON_ERROR(m_sai->set(SAI_OBJECT_TYPE_SWITCH, switch_id, &attr));
    }

    int exitcode = 0;

    if (m_commandLineOptions->m_files.size() > 0)
    {
        attr.id = SAI_REDIS_SWITCH_ATTR_USE_TEMP_VIEW;
        attr.value.booldata = m_commandLineOptions->m_useTempView;

        EXIT_ON_ERROR(m_sai->set(SAI_OBJECT_TYPE_SWITCH, switch_id, &attr));

        exitcode = replay();
    }

    m_sai->uninitialize();

    return exitcode;

}
