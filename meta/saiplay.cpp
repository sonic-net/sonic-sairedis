#include <getopt.h>
#include <unistd.h>

#include "string.h"
extern "C" {
#include "sai.h"
}

#include "meta/sai_serialize.h"
#include "meta/saiattributelist.h"
#include "meta/saiplay.h"
#include "swss/logger.h"

#include <iostream>
#include <stdexcept>
#include <sstream>
#include <string>

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
