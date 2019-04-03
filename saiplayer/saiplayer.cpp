#include <getopt.h>
#include <unistd.h>

#include "string.h"
extern "C" {
#include "sai.h"
}

#include "meta/sai_serialize.h"
#include "meta/saiattributelist.h"
#include "swss/logger.h"
#include "swss/tokenize.h"
#include "sairedis.h"

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

std::map<std::string, std::string> profile_map;

const char *test_profile_get_value (
        _In_ sai_switch_profile_id_t profile_id,
        _In_ const char *variable)
{
    SWSS_LOG_ENTER();

    auto it = profile_map.find(variable);

    if (it == profile_map.end())
    {
        return NULL;
    }

    return it->second.c_str();
}

int test_profile_get_next_value (
        _In_ sai_switch_profile_id_t profile_id,
        _Out_ const char **variable,
        _Out_ const char **value)
{
    SWSS_LOG_ENTER();

    return -1;
}

const sai_service_method_table_t test_services = {
    test_profile_get_value,
    test_profile_get_next_value
};

void on_switch_state_change(
        _In_ sai_switch_oper_status_t switch_oper_status)
{
    SWSS_LOG_ENTER();
}

void on_fdb_event(
        _In_ uint32_t count,
        _In_ sai_fdb_event_notification_data_t *data)
{
    SWSS_LOG_ENTER();
}

void on_port_state_change(
        _In_ uint32_t count,
        _In_ sai_port_oper_status_notification_t *data)
{
    SWSS_LOG_ENTER();
}

void on_switch_shutdown_request_notification(
        _In_ sai_object_id_t switch_id) __attribute__ ((noreturn));

void on_switch_shutdown_request_notification(
        _In_ sai_object_id_t switch_id)
{
    SWSS_LOG_ENTER();

    SWSS_LOG_ERROR("got shutdown request, syncd failed!");
    exit(EXIT_FAILURE);
}

void on_packet_event(
        _In_ const void *buffer,
        _In_ sai_size_t buffer_size,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();
}

#define EXIT_ON_ERROR(x)\
{\
    sai_status_t s = (x);\
    if (s != SAI_STATUS_SUCCESS)\
    {\
        SWSS_LOG_THROW("fail status: %s", sai_serialize_status(s).c_str());\
    }\
}

// to recorded
std::map<sai_object_id_t,sai_object_id_t> local_to_redis;
std::map<sai_object_id_t,sai_object_id_t> redis_to_local;

sai_object_id_t translate_local_to_redis(
        _In_ sai_object_id_t rid)
{
    SWSS_LOG_ENTER();

    SWSS_LOG_DEBUG("translating local RID %s",
            sai_serialize_object_id(rid).c_str());

    if (rid == SAI_NULL_OBJECT_ID)
    {
        return SAI_NULL_OBJECT_ID;
    }

    auto it = local_to_redis.find(rid);

    if (it == local_to_redis.end())
    {
        SWSS_LOG_THROW("failed to translate local RID %s",
                sai_serialize_object_id(rid).c_str());
    }

    return it->second;
}

    template <typename T>
void translate_local_to_redis(
        _In_ T &element)
{
    SWSS_LOG_ENTER();

    for (uint32_t i = 0; i < element.count; i++)
    {
        element.list[i] = translate_local_to_redis(element.list[i]);
    }
}

void translate_local_to_redis(
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
                translate_local_to_redis(attr.value.aclfield.data.objlist);
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

                // XXX if (meta->isoidattribute)
                if (meta->allowedobjecttypeslength > 0)
                {
                    SWSS_LOG_THROW("attribute %s is oid attribute but not handled, FIXME", meta->attridname);
                }

                break;
        }
    }
}

sai_object_type_t deserialize_object_type(const std::string& s)
{
    SWSS_LOG_ENTER();

    sai_object_type_t object_type;

    sai_deserialize_object_type(s, object_type);

    return object_type;
}

const std::vector<swss::FieldValueTuple> get_values(const std::vector<std::string>& items)
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

void match_list_lengths(
        sai_object_type_t object_type,
        uint32_t get_attr_count,
        sai_attribute_t* get_attr_list,
        uint32_t attr_count,
        sai_attribute_t* attr_list)
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

void match_redis_with_rec(
        sai_object_id_t get_oid,
        sai_object_id_t oid)
{
    SWSS_LOG_ENTER();

    auto it = redis_to_local.find(get_oid);

    if (it == redis_to_local.end())
    {
        redis_to_local[get_oid] = oid;
        local_to_redis[oid] = get_oid;
    }

    if (oid != redis_to_local[get_oid])
    {
        SWSS_LOG_THROW("match failed, oid order is mismatch :( oid 0x%lx get_oid 0x%lx second 0x%lx",
                oid,
                get_oid,
                redis_to_local[get_oid]);
    }

    SWSS_LOG_DEBUG("map size: %zu", local_to_redis.size());
}

void match_redis_with_rec(
        sai_object_list_t get_objlist,
        sai_object_list_t objlist)
{
    SWSS_LOG_ENTER();

    for (uint32_t i = 0 ; i < get_objlist.count; ++i)
    {
        match_redis_with_rec(get_objlist.list[i], objlist.list[i]);
    }
}

void match_redis_with_rec(
        sai_object_type_t object_type,
        uint32_t get_attr_count,
        sai_attribute_t* get_attr_list,
        uint32_t attr_count,
        sai_attribute_t* attr_list)
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

sai_status_t handle_fdb(
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
            return sai_metadata_sai_fdb_api->create_fdb_entry(&fdb_entry, attr_count, attr_list);

        case SAI_COMMON_API_REMOVE:
            return sai_metadata_sai_fdb_api->remove_fdb_entry(&fdb_entry);

        case SAI_COMMON_API_SET:
            return sai_metadata_sai_fdb_api->set_fdb_entry_attribute(&fdb_entry, attr_list);

        case SAI_COMMON_API_GET:
            return sai_metadata_sai_fdb_api->get_fdb_entry_attribute(&fdb_entry, attr_count, attr_list);

        default:
            SWSS_LOG_THROW("fdb other apis not implemented");
    }

    return SAI_STATUS_SUCCESS;
}

sai_status_t handle_neighbor(
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
            return sai_metadata_sai_neighbor_api->create_neighbor_entry(&neighbor_entry, attr_count, attr_list);

        case SAI_COMMON_API_REMOVE:
            return sai_metadata_sai_neighbor_api->remove_neighbor_entry(&neighbor_entry);

        case SAI_COMMON_API_SET:
            return sai_metadata_sai_neighbor_api->set_neighbor_entry_attribute(&neighbor_entry, attr_list);

        case SAI_COMMON_API_GET:
            return sai_metadata_sai_neighbor_api->get_neighbor_entry_attribute(&neighbor_entry, attr_count, attr_list);

        default:
            SWSS_LOG_THROW("neighbor other apis not implemented");
    }
}

sai_status_t handle_route(
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
            return sai_metadata_sai_route_api->create_route_entry(&route_entry, attr_count, attr_list);

        case SAI_COMMON_API_REMOVE:
            return sai_metadata_sai_route_api->remove_route_entry(&route_entry);

        case SAI_COMMON_API_SET:
            return sai_metadata_sai_route_api->set_route_entry_attribute(&route_entry, attr_list);

        case SAI_COMMON_API_GET:
            return sai_metadata_sai_route_api->get_route_entry_attribute(&route_entry, attr_count, attr_list);

        default:
            SWSS_LOG_THROW("route other apis not implemented");
    }
}

void update_notifications_pointers(
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


        switch (attr.id)
        {
            case SAI_SWITCH_ATTR_SWITCH_STATE_CHANGE_NOTIFY:
                attr.value.ptr = (void*)&on_switch_state_change;
                break;

            case SAI_SWITCH_ATTR_SHUTDOWN_REQUEST_NOTIFY:
                attr.value.ptr = (void*)&on_switch_shutdown_request_notification;
                break;

            case SAI_SWITCH_ATTR_FDB_EVENT_NOTIFY:
                attr.value.ptr = (void*)&on_fdb_event;
                break;

            case SAI_SWITCH_ATTR_PORT_STATE_CHANGE_NOTIFY:
                attr.value.ptr = (void*)&on_port_state_change;
                break;

            case SAI_SWITCH_ATTR_PACKET_EVENT_NOTIFY:
                attr.value.ptr = (void*)&on_packet_event;
                break;

            default:
                SWSS_LOG_ERROR("pointer for %s is not handled, FIXME!", meta->attridname);
                break;
        }
    }
}

sai_status_t handle_generic(
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

    auto info = sai_metadata_get_object_type_info(object_type);

    sai_object_meta_key_t meta_key;

    meta_key.objecttype = object_type;

    switch (api)
    {
        case SAI_COMMON_API_CREATE:

            {
                sai_object_id_t switch_id = sai_switch_id_query(local_id);

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

                sai_status_t status = info->create(&meta_key, switch_id, attr_count, attr_list);

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

                return info->remove(&meta_key);
            }

        case SAI_COMMON_API_SET:

            {
                if (object_type == SAI_OBJECT_TYPE_SWITCH)
                {
                    update_notifications_pointers(1, attr_list);
                }

                meta_key.objectkey.key.object_id = translate_local_to_redis(local_id);

                return info->set(&meta_key, attr_list);
            }

        case SAI_COMMON_API_GET:

            {
                meta_key.objectkey.key.object_id = translate_local_to_redis(local_id);

                return info->get(&meta_key, attr_count, attr_list);
            }

        default:
            SWSS_LOG_THROW("generic other apis not implemented");
    }
}

void handle_get_response(
        sai_object_type_t object_type,
        uint32_t get_attr_count,
        sai_attribute_t* get_attr_list,
        const std::string& response,
        sai_status_t status)
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

void performSleep(const std::string& line)
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

bool g_notifySyncd = true;

void performNotifySyncd(const std::string& request, const std::string response)
{
    SWSS_LOG_ENTER();

    // timestamp|action|data
    auto r = swss::tokenize(request, '|');
    auto R = swss::tokenize(response, '|');

    if (r[1] != "a" || R[1] != "A")
    {
        SWSS_LOG_THROW("invalid syncd notify request/response %s/%s", request.c_str(), response.c_str());
    }

    if (g_notifySyncd == false)
    {
        SWSS_LOG_NOTICE("skipping notify syncd, selected by user");
        return;
    }

    // tell syncd that we are compiling new view
    sai_attribute_t attr;
    attr.id = SAI_REDIS_SWITCH_ATTR_NOTIFY_SYNCD;

    const std::string requestAction = r[2];

    if (requestAction == SYNCD_INIT_VIEW)
    {
        attr.value.s32 = SAI_REDIS_NOTIFY_SYNCD_INIT_VIEW;
    }
    else if (requestAction == SYNCD_APPLY_VIEW)
    {
        attr.value.s32 = SAI_REDIS_NOTIFY_SYNCD_APPLY_VIEW;
    }
    else
    {
        SWSS_LOG_THROW("invalid syncd notify request: %s", request.c_str());
    }

    /*
     * NOTE: We don't need actual switch to set those attributes.
     */

    sai_object_id_t switch_id = SAI_NULL_OBJECT_ID;

    sai_status_t status = sai_metadata_sai_switch_api->set_switch_attribute(switch_id, &attr);

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

std::vector<std::string> tokenize(
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

sai_status_t handle_bulk_route(
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

    if (api == (sai_common_api_t)SAI_COMMON_API_BULK_SET)
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

        sai_status_t status = sai_bulk_set_route_entry_attribute(
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
    else
    {
        SWSS_LOG_THROW("api %d is not supported in bulk route", api);
    }
}

void processBulk(
        _In_ sai_common_api_t api,
        _In_ const std::string &line)
{
    SWSS_LOG_ENTER();

    if (!line.size())
    {
        return;
    }

    if (api != (sai_common_api_t)SAI_COMMON_API_BULK_SET)
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

        if (api != (sai_common_api_t)SAI_COMMON_API_BULK_GET)
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

bool g_sleep = false;

int replay(int argc, char **argv)
{
    //swss::Logger::getInstance().setMinPrio(swss::Logger::SWSS_DEBUG);

    SWSS_LOG_ENTER();

    if (argc == 0)
    {
        fprintf(stderr, "ERR: need to specify filename\n");

        return -1;
    }

    char* filename = argv[argc - 1];

    SWSS_LOG_NOTICE("using file: %s", filename);

    std::ifstream infile(filename);

    if (!infile.is_open())
    {
        SWSS_LOG_ERROR("failed to open file %s", filename);
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
                processBulk((sai_common_api_t)SAI_COMMON_API_BULK_SET, line);
                continue;
            case 'g':
                api = SAI_COMMON_API_GET;
                break;
            case '#':
            case 'n':
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

    SWSS_LOG_NOTICE("finished replaying %s with SUCCESS", filename);

    if (g_sleep)
    {
        fprintf(stderr, "Reply SUCCESS, sleeping, watching for notifications\n");

        sleep(-1);
    }

    return 0;
}

void printUsage()
{
    SWSS_LOG_ENTER();

    std::cout << "Usage: saiplayer [-h] recordfile" << std::endl << std::endl;
    std::cout << "    -C --skipNotifySyncd:" << std::endl;
    std::cout << "        Will not send notify init/apply view to syncd" << std::endl << std::endl;
    std::cout << "    -d --enableDebug:" << std::endl;
    std::cout << "        Enable syslog debug messages" << std::endl << std::endl;
    std::cout << "    -u --useTempView:" << std::endl;
    std::cout << "        Enable temporary view between init and apply" << std::endl << std::endl;
    std::cout << "    -i --inspectAsic:" << std::endl;
    std::cout << "        Inspect ASIC by ASIC DB" << std::endl << std::endl;
    std::cout << "    -s --sleep:" << std::endl;
    std::cout << "        Sleep after success reply, to notice any switch notifications" << std::endl << std::endl;
    std::cout << "    -h --help:" << std::endl;
    std::cout << "        Print out this message" << std::endl << std::endl;
}

bool g_useTempView = false;
bool g_inspectAsic = false;

int handleCmdLine(int argc, char **argv)
{
    SWSS_LOG_ENTER();

    while(true)
    {
        static struct option long_options[] =
        {
            { "useTempView",      no_argument,       0, 'u' },
            { "help",             no_argument,       0, 'h' },
            { "skipNotifySyncd",  no_argument,       0, 'C' },
            { "enableDebug",      no_argument,       0, 'd' },
            { "inspectAsic",      no_argument,       0, 'i' },
            { "sleep",            no_argument,       0, 's' },
            { 0,                  0,                 0,  0  }
        };

        const char* const optstring = "hCduis";

        int c = getopt_long(argc, argv, optstring, long_options, 0);

        if (c == -1)
            break;

        switch (c)
        {
            case 'd':
                swss::Logger::getInstance().setMinPrio(swss::Logger::SWSS_DEBUG);
                break;

            case 'u':
                g_useTempView = false;
                break;

            case 'C':
                g_notifySyncd = false;
                break;

            case 'i':
                g_inspectAsic = true;
                break;

            case 's':
                g_sleep = true;
                break;

            case 'h':
                printUsage();
                exit(EXIT_SUCCESS);

            case '?':
                SWSS_LOG_WARN("unknown option %c", optopt);
                printUsage();
                exit(EXIT_FAILURE);

            default:
                SWSS_LOG_ERROR("getopt_long failure");
                exit(EXIT_FAILURE);
        }
    }

    return optind;
}

void sai_meta_log_syncd(
        _In_ sai_log_level_t log_level,
        _In_ const char *file,
        _In_ int line,
        _In_ const char *func,
        _In_ const char *format,
        ...)
    __attribute__ ((format (printf, 5, 6)));

void sai_meta_log_syncd(
        _In_ sai_log_level_t log_level,
        _In_ const char *file,
        _In_ int line,
        _In_ const char *func,
        _In_ const char *format,
        ...)
{
    char buffer[0x1000];

    va_list ap;
    va_start(ap, format);
    vsnprintf(buffer, 0x1000, format, ap);
    va_end(ap);

    swss::Logger::Priority p = swss::Logger::SWSS_NOTICE;

    switch (log_level)
    {
        case SAI_LOG_LEVEL_DEBUG:
            p = swss::Logger::SWSS_DEBUG;
            break;
        case SAI_LOG_LEVEL_INFO:
            p = swss::Logger::SWSS_INFO;
            break;
        case SAI_LOG_LEVEL_ERROR:
            p = swss::Logger::SWSS_ERROR;
            fprintf(stderr, "ERROR: %s: %s\n", func, buffer);
            break;
        case SAI_LOG_LEVEL_WARN:
            p = swss::Logger::SWSS_WARN;
            fprintf(stderr, "WARN: %s: %s\n", func, buffer);
            break;
        case SAI_LOG_LEVEL_CRITICAL:
            p = swss::Logger::SWSS_CRIT;
            break;

        default:
            p = swss::Logger::SWSS_NOTICE;
            break;
    }

    swss::Logger::getInstance().write(p, ":- %s: %s", func, buffer);
}

int main(int argc, char **argv)
{
    swss::Logger::getInstance().setMinPrio(swss::Logger::SWSS_DEBUG);

    SWSS_LOG_ENTER();

    swss::Logger::getInstance().setMinPrio(swss::Logger::SWSS_NOTICE);

    int handled = handleCmdLine(argc, argv);
    argc -= handled;
    argv += handled;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsuggest-attribute=format"
    sai_metadata_log = &sai_meta_log_syncd;
#pragma GCC diagnostic pop

    EXIT_ON_ERROR(sai_api_initialize(0, (const sai_service_method_table_t *)&test_services));

    sai_apis_t apis;
    sai_metadata_apis_query(sai_api_query, &apis);

    sai_attribute_t attr;

    /*
     * Notice that we use null object id as switch id, which is fine since
     * those attributes don't need switch.
     */

    sai_object_id_t switch_id = SAI_NULL_OBJECT_ID;

    if (g_inspectAsic)
    {
        attr.id = SAI_REDIS_SWITCH_ATTR_NOTIFY_SYNCD;
        attr.value.s32 = SAI_REDIS_NOTIFY_SYNCD_INSPECT_ASIC;
        EXIT_ON_ERROR(sai_metadata_sai_switch_api->set_switch_attribute(switch_id, &attr));
    }

    int exitcode = 0;
    if (argc > 0)
    {
        attr.id = SAI_REDIS_SWITCH_ATTR_USE_TEMP_VIEW;
        attr.value.booldata = g_useTempView;

        EXIT_ON_ERROR(sai_metadata_sai_switch_api->set_switch_attribute(switch_id, &attr));

        exitcode = replay(argc, argv);
    }

    sai_api_uninitialize();

    return exitcode;
}
