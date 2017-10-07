#include <arpa/inet.h>

extern "C" {
#include <sai.h>
}

#include "swss/logger.h"
#include "sairedis.h"
#include "sai_redis.h"
#include "meta/saiserialize.h"

#include <map>
#include <unordered_map>
#include <vector>

#define ASSERT_SUCCESS(format,...) \
    if ((status)!=SAI_STATUS_SUCCESS) \
        SWSS_LOG_THROW(format ": %s", ##__VA_ARGS__, sai_serialize_status(status).c_str());

const char* profile_get_value(
        _In_ sai_switch_profile_id_t profile_id,
        _In_ const char* variable)
{
    SWSS_LOG_ENTER();

    return NULL;
}

int profile_get_next_value(
        _In_ sai_switch_profile_id_t profile_id,
        _Out_ const char** variable,
        _Out_ const char** value)
{
    SWSS_LOG_ENTER();

    if (value == NULL)
    {
        SWSS_LOG_INFO("resetting profile map iterator");

        return 0;
    }

    if (variable == NULL)
    {
        SWSS_LOG_WARN("variable is null");
        return -1;
    }

    SWSS_LOG_INFO("iterator reached end");
    return -1;
}

service_method_table_t test_services = {
    profile_get_value,
    profile_get_next_value
};

void test_sai_initialize()
{
    SWSS_LOG_ENTER();

    // NOTE: this is just testing whether test application will
    // link against libsairedis, this api requires running redis db
    // with enabled unix socket
    sai_status_t status = sai_api_initialize(0, (service_method_table_t*)&test_services);

    ASSERT_SUCCESS("Failed to initialize api");
}

void test_enable_recording()
{
    SWSS_LOG_ENTER();

    sai_attribute_t attr;
    attr.id = SAI_REDIS_SWITCH_ATTR_RECORD;
    attr.value.booldata = true;

    sai_switch_api_t *sai_switch_api = NULL;

    sai_api_query(SAI_API_SWITCH, (void**)&sai_switch_api);

    sai_status_t status = sai_switch_api->set_switch_attribute(SAI_NULL_OBJECT_ID, &attr);

    ASSERT_SUCCESS("Failed to enable recording");
}

class SaiAttrWrapper;
extern std::unordered_map<std::string,
       std::unordered_map<sai_attr_id_t,
       std::shared_ptr<SaiAttrWrapper>>> ObjectAttrHash;
extern void object_reference_insert(sai_object_id_t oid);

sai_object_id_t create_dummy_object_id(
        _In_ sai_object_type_t objecttype)
{
    SWSS_LOG_ENTER();

    static uint64_t index = 0;

    return (((sai_object_id_t)objecttype) << 48) | ++index;
}

void test_bulk_next_hop_group_member_create()
{
    SWSS_LOG_ENTER();

    swss::Logger::getInstance().setMinPrio(swss::Logger::SWSS_NOTICE);

    meta_init_db();
    redis_clear_switch_ids();

    swss::Logger::getInstance().setMinPrio(swss::Logger::SWSS_DEBUG);

    sai_status_t    status;

    sai_next_hop_group_api_t  *sai_next_hop_group_api = NULL;
    sai_switch_api_t *sai_switch_api = NULL;

    sai_api_query(SAI_API_NEXT_HOP_GROUP, (void**)&sai_next_hop_group_api);
    sai_api_query(SAI_API_SWITCH, (void**)&sai_switch_api);

    uint32_t count = 3;

    std::vector<sai_route_entry_t> routes;
    std::vector<sai_attribute_t> attrs;

    sai_attribute_t swattr;

    swattr.id = SAI_SWITCH_ATTR_INIT_SWITCH;
    swattr.value.booldata = true;

    sai_object_id_t switch_id;
    status = sai_switch_api->create_switch(&switch_id, 1, &swattr);

    ASSERT_SUCCESS("Failed to create switch");

    std::vector<std::vector<sai_attribute_t>> nhgm_attrs;
    std::vector<sai_attribute_t *> nhgm_attrs_array;
    std::vector<uint32_t> nhgm_attrs_count;

    // next hop group
    sai_object_id_t hopgroup = create_dummy_object_id(SAI_OBJECT_TYPE_NEXT_HOP_GROUP);
    object_reference_insert(hopgroup);
    sai_object_meta_key_t meta_key_hopgruop = { .objecttype = SAI_OBJECT_TYPE_NEXT_HOP_GROUP, .objectkey = { .key = { .object_id = hopgroup } } };
    std::string hopgroup_key = sai_serialize_object_meta_key(meta_key_hopgruop);
    ObjectAttrHash[hopgroup_key] = { };

    for (uint32_t i = 0; i <  count; ++i)
    {
        // next hop
        sai_object_id_t hop = create_dummy_object_id(SAI_OBJECT_TYPE_NEXT_HOP);
        object_reference_insert(hop);
        sai_object_meta_key_t meta_key_hop = { .objecttype = SAI_OBJECT_TYPE_NEXT_HOP, .objectkey = { .key = { .object_id = hop } } };
        std::string hop_key = sai_serialize_object_meta_key(meta_key_hop);
        ObjectAttrHash[hop_key] = { };

        std::vector<sai_attribute_t> list(2);
        sai_attribute_t &attr1 = list[0];
        sai_attribute_t &attr2 = list[1];

        attr1.id = SAI_NEXT_HOP_GROUP_MEMBER_ATTR_NEXT_HOP_GROUP_ID;
        attr1.value.oid = hopgroup;
        attr2.id = SAI_NEXT_HOP_GROUP_MEMBER_ATTR_NEXT_HOP_ID;
        attr2.value.oid = hop;
        nhgm_attrs.push_back(list);
        nhgm_attrs_count.push_back(2);
    }

    for (size_t j = 0; j < nhgm_attrs.size(); j++)
    {
        nhgm_attrs_array.push_back(nhgm_attrs[j].data());
    }

    std::vector<sai_status_t> statuses(count);
    std::vector<sai_object_id_t> object_id(count);
    redis_bulk_object_create_next_hop_group_members(switch_id, count, nhgm_attrs_count.data(), nhgm_attrs_array.data()
        , SAI_BULK_OP_TYPE_INGORE_ERROR, object_id.data(), statuses.data());
    ASSERT_SUCCESS("Failed to create nhgm");
    for (size_t j = 0; j < statuses.size(); j++)
    {
        status = statuses[j];
        ASSERT_SUCCESS("Failed to create nhgm # %zu", j);
    }
}

void test_bulk_route_set()
{
    SWSS_LOG_ENTER();

    swss::Logger::getInstance().setMinPrio(swss::Logger::SWSS_NOTICE);

    meta_init_db();
    redis_clear_switch_ids();

    swss::Logger::getInstance().setMinPrio(swss::Logger::SWSS_DEBUG);

    sai_status_t    status;

    sai_route_api_t  *sai_route_api = NULL;
    sai_switch_api_t *sai_switch_api = NULL;

    sai_api_query(SAI_API_ROUTE, (void**)&sai_route_api);
    sai_api_query(SAI_API_SWITCH, (void**)&sai_switch_api);

    uint32_t count = 3;

    std::vector<sai_route_entry_t> routes;
    std::vector<sai_attribute_t> attrs;

    uint32_t index = 15;

    sai_attribute_t swattr;

    swattr.id = SAI_SWITCH_ATTR_INIT_SWITCH;
    swattr.value.booldata = true;

    sai_object_id_t switch_id;
    status = sai_switch_api->create_switch(&switch_id, 1, &swattr);

    ASSERT_SUCCESS("Failed to create switch");

    std::vector<std::vector<sai_attribute_t>> route_attrs;
    std::vector<sai_attribute_t *> route_attrs_array;
    std::vector<uint32_t> route_attrs_count;

    for (uint32_t i = index; i < index + count; ++i)
    {
        sai_route_entry_t route_entry;

        // virtual router
        sai_object_id_t vr = create_dummy_object_id(SAI_OBJECT_TYPE_VIRTUAL_ROUTER);
        object_reference_insert(vr);
        sai_object_meta_key_t meta_key_vr = { .objecttype = SAI_OBJECT_TYPE_VIRTUAL_ROUTER, .objectkey = { .key = { .object_id = vr } } };
        std::string vr_key = sai_serialize_object_meta_key(meta_key_vr);
        ObjectAttrHash[vr_key] = { };

        // next hop
        sai_object_id_t hop = create_dummy_object_id(SAI_OBJECT_TYPE_NEXT_HOP);
        object_reference_insert(hop);
        sai_object_meta_key_t meta_key_hop = { .objecttype = SAI_OBJECT_TYPE_NEXT_HOP, .objectkey = { .key = { .object_id = hop } } };
        std::string hop_key = sai_serialize_object_meta_key(meta_key_hop);
        ObjectAttrHash[hop_key] = { };

        route_entry.destination.addr_family = SAI_IP_ADDR_FAMILY_IPV4;
        route_entry.destination.addr.ip4 = htonl(0x0a000000 | i);
        route_entry.destination.mask.ip4 = htonl(0xffffffff);
        route_entry.vr_id = vr;
        route_entry.switch_id = switch_id;
        route_entry.destination.addr_family = SAI_IP_ADDR_FAMILY_IPV4;
        routes.push_back(route_entry);

        std::vector<sai_attribute_t> list(2);
        sai_attribute_t &attr1 = list[0];
        sai_attribute_t &attr2 = list[1];

        attr1.id = SAI_ROUTE_ENTRY_ATTR_NEXT_HOP_ID;
        attr1.value.oid = hop;
        attr2.id = SAI_ROUTE_ENTRY_ATTR_PACKET_ACTION;
        attr2.value.s32 = SAI_PACKET_ACTION_FORWARD;
        route_attrs.push_back(list);
        route_attrs_count.push_back(2);
    }

    for (size_t j = 0; j < route_attrs.size(); j++)
    {
        route_attrs_array.push_back(route_attrs[j].data());
    }

    std::vector<sai_status_t> statuses(count);
    status = sai_bulk_create_route_entry(count, routes.data(), route_attrs_count.data(), route_attrs_array.data()
        , SAI_BULK_OP_TYPE_INGORE_ERROR, statuses.data());
    ASSERT_SUCCESS("Failed to create route");
    for (size_t j = 0; j < statuses.size(); j++)
    {
        status = statuses[j];
        ASSERT_SUCCESS("Failed to create route # %zu", j);
    }

    for (uint32_t i = index; i < index + count; ++i)
    {
        sai_attribute_t attr;
        attr.id = SAI_ROUTE_ENTRY_ATTR_PACKET_ACTION;
        attr.value.s32 = SAI_PACKET_ACTION_DROP;

        status = sai_route_api->set_route_entry_attribute(&routes[i - index], &attr);

        attrs.push_back(attr);

        ASSERT_SUCCESS("Failed to set route");
    }

    statuses.clear();
    statuses.resize(attrs.size());

    for (auto &attr: attrs)
    {
        attr.value.s32 = SAI_PACKET_ACTION_FORWARD;
    }

    status = sai_bulk_set_route_entry_attribute(
        count,
        routes.data(),
        attrs.data(),
        SAI_BULK_OP_TYPE_INGORE_ERROR,
        statuses.data());

    ASSERT_SUCCESS("Failed to bulk set route");

    for (auto s: statuses)
    {
        status = s;

        ASSERT_SUCCESS("Failed to bulk set route on one of the routes");
    }

    // TODO we need to add consumer producer test here to see
    // if after consume we get pop we get expectd parameters
}

int main()
{
    swss::Logger::getInstance().setMinPrio(swss::Logger::SWSS_DEBUG);

    SWSS_LOG_ENTER();

//    swss::Logger::getInstance().setMinPrio(swss::Logger::SWSS_INFO);

    try
    {
        test_sai_initialize();

        test_enable_recording();

        test_bulk_next_hop_group_member_create();

        test_bulk_route_set();

        sai_api_uninitialize();

        printf("\n[ %s ]\n\n", sai_serialize_status(SAI_STATUS_SUCCESS).c_str());
    }
    catch (const std::exception &e)
    {
        SWSS_LOG_ERROR("exception: %s", e.what());

        printf("\n[ %s ]\n\n%s\n\n", sai_serialize_status(SAI_STATUS_FAILURE).c_str(), e.what());

        exit(EXIT_FAILURE);
    }

    return 0;
}
