#include "sai_meta.h"
#include "sai_extra.h"
#include "sai_serialize.h"

#include <string.h>
#include <arpa/inet.h>

#include <map>
#include <iterator>
#include <unordered_map>
#include <memory>
#include <vector>

class SaiAttrWrapper;
extern std::unordered_map<std::string,std::unordered_map<sai_attr_id_t,std::shared_ptr<SaiAttrWrapper>>> ObjectAttrHash;
extern bool is_ipv6_mask_valid(const uint8_t* mask);
extern bool object_exists(const std::string& key);
extern bool object_reference_exists(sai_object_id_t oid);
extern void object_reference_inc(sai_object_id_t oid);
extern void object_reference_dec(sai_object_id_t oid);
extern void object_reference_dec(const sai_object_list_t& list);
extern void object_reference_insert(sai_object_id_t oid);
extern int32_t object_reference_count(sai_object_id_t oid);

std::string construct_key(
        _In_ const sai_object_meta_key_t& meta_key,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t* attr_list);

sai_object_type_t sai_object_type_query(
        _In_ sai_object_id_t oid)
{
    SWSS_LOG_ENTER();

    if (oid == SAI_NULL_OBJECT_ID)
    {
        return SAI_OBJECT_TYPE_NULL;
    }

    sai_object_type_t objecttype = (sai_object_type_t)((oid >> 48) & 0xFF);

    if ((objecttype <= SAI_OBJECT_TYPE_NULL) ||
            (objecttype >= SAI_OBJECT_TYPE_EXTENSIONS_MAX))
    {
        SWSS_LOG_THROW("invalid oid 0x%lx", oid);
    }

    return objecttype;
}

int switch_index = 0;
uint64_t vid_index = 0;


void clear_local()
{
    SWSS_LOG_ENTER();

    switch_index = 0;

    vid_index = 0;
}

int get_switch_index(
        _In_ sai_object_id_t switch_id)
{
    SWSS_LOG_ENTER();

    if (switch_id == SAI_NULL_OBJECT_ID)
    {
        SWSS_LOG_THROW("switch id can't be NULL");
    }

    sai_object_type_t ot = sai_object_type_query(switch_id);

    if (ot != SAI_OBJECT_TYPE_SWITCH)
    {
        SWSS_LOG_THROW("expected switch id, got %s", sai_serialize_object_type(ot).c_str());
    }

    return (int)((switch_id >> 56) & 0xFF);
}

sai_object_id_t construct_object_id(
        _In_ sai_object_type_t object_type,
        _In_ int sw_index,
        _In_ uint64_t virtual_id)
{
    SWSS_LOG_ENTER();

    return (sai_object_id_t)(((uint64_t)sw_index << 56) | ((uint64_t)object_type << 48) | virtual_id);
}

sai_object_id_t create_dummy_object_id(
        _In_ sai_object_type_t object_type,
        _In_ sai_object_id_t switch_id)
{
    SWSS_LOG_ENTER();

    if ((object_type <= SAI_OBJECT_TYPE_NULL) ||
            (object_type >= SAI_OBJECT_TYPE_EXTENSIONS_MAX))
    {
        SWSS_LOG_THROW("invalid objct type: %d", object_type);
    }

    if (object_type == SAI_OBJECT_TYPE_SWITCH)
    {
        if (switch_index > 0)
        {
            SWSS_LOG_THROW("creating second switch, not supported yet");
        }

        sai_object_id_t oid = construct_object_id(object_type, switch_index, switch_index);

        switch_index++;

        return oid;
    }

    int sw_index = get_switch_index(switch_id);

    sai_object_id_t oid = construct_object_id(object_type, sw_index, vid_index++);

    SWSS_LOG_DEBUG("created oid 0x%lx", oid);

    return oid;
}

sai_object_id_t sai_switch_id_query(
        _In_ sai_object_id_t oid)
{
    SWSS_LOG_ENTER();

    if (oid == SAI_NULL_OBJECT_ID)
    {
        return oid;
    }

    sai_object_type_t object_type = sai_object_type_query(oid);

    if (object_type == SAI_OBJECT_TYPE_NULL)
    {
        SWSS_LOG_THROW("invalid object type of oid 0x%lx", oid);
    }

    if (object_type == SAI_OBJECT_TYPE_SWITCH)
    {
        return oid;
    }

    int sw_index = (int)((oid >> 56) & 0xFF);

    sai_object_id_t sw_id = construct_object_id(SAI_OBJECT_TYPE_SWITCH, sw_index, sw_index);

    return sw_id;
}

// FDB ENTRY DUMMY FUNCTIONS

sai_status_t dummy_success_sai_set_fdb_entry(
        _In_ const sai_fdb_entry_t* fdb_entry,
        _In_ const sai_attribute_t *attr)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_SUCCESS;
}

sai_status_t dummy_failure_sai_set_fdb_entry(
        _In_ const sai_fdb_entry_t* fdb_entry,
        _In_ const sai_attribute_t *attr)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_FAILURE;
}

sai_status_t dummy_success_sai_get_fdb_entry(
        _In_ const sai_fdb_entry_t* fdb_entry,
        _In_ uint32_t attr_count,
        _Inout_ sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_SUCCESS;
}

sai_status_t dummy_failure_sai_get_fdb_entry(
        _In_ const sai_fdb_entry_t* fdb_entry,
        _In_ uint32_t attr_count,
        _Inout_ sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_FAILURE;
}

sai_status_t dummy_success_sai_create_fdb_entry(
        _In_ const sai_fdb_entry_t* fdb_entry,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_SUCCESS;
}

sai_status_t dummy_failure_sai_create_fdb_entry(
        _In_ const sai_fdb_entry_t* fdb_entry,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_FAILURE;
}

sai_status_t dummy_success_sai_remove_fdb_entry(
        _In_ const sai_fdb_entry_t* fdb_entry)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_SUCCESS;
}

sai_status_t dummy_failure_sai_remove_fdb_entry(
        _In_ const sai_fdb_entry_t* fdb_entry)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_FAILURE;
}

// NEIGHBOR ENTRY DUMMY FUNCTIONS

sai_status_t dummy_success_sai_set_neighbor_entry(
        _In_ const sai_neighbor_entry_t* neighbor_entry,
        _In_ const sai_attribute_t *attr)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_SUCCESS;
}

sai_status_t dummy_failure_sai_set_neighbor_entry(
        _In_ const sai_neighbor_entry_t* neighbor_entry,
        _In_ const sai_attribute_t *attr)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_FAILURE;
}

sai_status_t dummy_success_sai_get_neighbor_entry(
        _In_ const sai_neighbor_entry_t* neighbor_entry,
        _In_ uint32_t attr_count,
        _Inout_ sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_SUCCESS;
}

sai_status_t dummy_failure_sai_get_neighbor_entry(
        _In_ const sai_neighbor_entry_t* neighbor_entry,
        _In_ uint32_t attr_count,
        _Inout_ sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_FAILURE;
}

sai_status_t dummy_success_sai_create_neighbor_entry(
        _In_ const sai_neighbor_entry_t* neighbor_entry,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_SUCCESS;
}

sai_status_t dummy_failure_sai_create_neighbor_entry(
        _In_ const sai_neighbor_entry_t* neighbor_entry,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_FAILURE;
}

sai_status_t dummy_success_sai_remove_neighbor_entry(
        _In_ const sai_neighbor_entry_t* neighbor_entry)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_SUCCESS;
}

sai_status_t dummy_failure_sai_remove_neighbor_entry(
        _In_ const sai_neighbor_entry_t* neighbor_entry)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_FAILURE;
}

// ROUTE ENTRY DUMMY FUNCTIONS

sai_status_t dummy_success_sai_set_route_entry(
        _In_ const sai_route_entry_t* unicast_route_entry,
        _In_ const sai_attribute_t *attr)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_SUCCESS;
}

sai_status_t dummy_failure_sai_set_route_entry(
        _In_ const sai_route_entry_t* unicast_route_entry,
        _In_ const sai_attribute_t *attr)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_FAILURE;
}

sai_status_t dummy_success_sai_get_route_entry(
        _In_ const sai_route_entry_t* unicast_route_entry,
        _In_ uint32_t attr_count,
        _Inout_ sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_SUCCESS;
}

sai_status_t dummy_failure_sai_get_route_entry(
        _In_ const sai_route_entry_t* unicast_route_entry,
        _In_ uint32_t attr_count,
        _Inout_ sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_FAILURE;
}

sai_status_t dummy_success_sai_create_route_entry(
        _In_ const sai_route_entry_t* unicast_route_entry,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_SUCCESS;
}

sai_status_t dummy_failure_sai_create_route_entry(
        _In_ const sai_route_entry_t* unicast_route_entry,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_FAILURE;
}

sai_status_t dummy_success_sai_remove_route_entry(
        _In_ const sai_route_entry_t* unicast_route_entry)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_SUCCESS;
}

sai_status_t dummy_failure_sai_remove_route_entry(
        _In_ const sai_route_entry_t* unicast_route_entry)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_FAILURE;
}

// GENERIC DUMMY FUNCTIONS

sai_status_t dummy_success_sai_set_oid(
        _In_ sai_object_type_t object_type,
        _Out_ sai_object_id_t oid,
        _In_ const sai_attribute_t *attr)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_SUCCESS;
}

sai_status_t dummy_failure_sai_set_oid(
        _In_ sai_object_type_t object_type,
        _Out_ sai_object_id_t oid,
        _In_ const sai_attribute_t *attr)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_FAILURE;
}

sai_status_t dummy_success_sai_get_oid(
        _In_ sai_object_type_t object_type,
        _Out_ sai_object_id_t oid,
        _In_ uint32_t attr_count,
        _Inout_ sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_SUCCESS;
}

sai_status_t dummy_failure_sai_get_oid(
        _In_ sai_object_type_t object_type,
        _Out_ sai_object_id_t oid,
        _In_ uint32_t attr_count,
        _Inout_ sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_FAILURE;
}

sai_status_t dummy_success_sai_create_oid(
        _In_ sai_object_type_t object_type,
        _Out_ sai_object_id_t* oid,
        _In_ sai_object_id_t switch_id,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    *oid = create_dummy_object_id(object_type, switch_id);

    if (*oid == SAI_NULL_OBJECT_ID)
    {
        return SAI_STATUS_FAILURE;
    }

    return SAI_STATUS_SUCCESS;
}

sai_status_t dummy_failure_sai_create_oid(
        _In_ sai_object_type_t object_type,
        _Out_ sai_object_id_t* oid,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_FAILURE;
}

sai_status_t dummy_success_sai_remove_oid(
        _In_ sai_object_type_t object_type,
        _Out_ sai_object_id_t oid)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_SUCCESS;
}

sai_status_t dummy_failure_sai_remove_oid(
        _In_ sai_object_type_t object_type,
        _Out_ sai_object_id_t oid)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_FAILURE;
}

// META ASSERTS

#define META_ASSERT_SUCCESS(x) \
    if (x != SAI_STATUS_SUCCESS) \
{\
    SWSS_LOG_ERROR("expected success");\
    throw;\
}

#define META_ASSERT_FAIL(x) \
    if (x == SAI_STATUS_SUCCESS) \
{\
    SWSS_LOG_ERROR("expected failure");\
    throw;\
}

#define META_ASSERT_TRUE(x) \
    if (!(x)) \
{\
    SWSS_LOG_ERROR("assert true failed '%s'", # x);\
    throw;\
}

// SWITCH TESTS

sai_status_t switch_get_virtual_router_id(
        _In_ sai_object_type_t object_type,
        _In_ sai_object_id_t object_id,
        _In_ uint32_t attr_count,
        _Inout_ sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    sai_object_id_t switch_id = sai_switch_id_query(object_id);

    attr_list[0].value.u32 = 32;
    attr_list[1].value.oid = create_dummy_object_id(SAI_OBJECT_TYPE_VIRTUAL_ROUTER, switch_id);

    return SAI_STATUS_SUCCESS;
};

sai_object_id_t create_switch()
{
    SWSS_LOG_ENTER();

    sai_status_t    status;
    sai_attribute_t attr;

    sai_object_id_t switchid;

    attr.id = SAI_SWITCH_ATTR_INIT_SWITCH;
    attr.value.booldata = true;

    status = meta_sai_create_oid(SAI_OBJECT_TYPE_SWITCH, &switchid, SAI_NULL_OBJECT_ID, 1, &attr, dummy_success_sai_create_oid);
    META_ASSERT_SUCCESS(status);

    return switchid;
}

void test_switch_set()
{
    SWSS_LOG_ENTER();

    clear_local();
    meta_init_db();

    sai_status_t    status;
    sai_attribute_t attr;

    sai_object_id_t switchid = create_switch();

    status = meta_sai_set_oid(SAI_OBJECT_TYPE_SWITCH, switchid, NULL, &dummy_success_sai_set_oid);
    META_ASSERT_FAIL(status);

    status = meta_sai_set_oid(SAI_OBJECT_TYPE_SWITCH, switchid, &attr, NULL);
    META_ASSERT_FAIL(status);

    // id outside range
    attr.id = -1;
    status = meta_sai_set_oid(SAI_OBJECT_TYPE_SWITCH, switchid, &attr, &dummy_success_sai_set_oid);
    META_ASSERT_FAIL(status);

    attr.id = SAI_SWITCH_ATTR_PORT_NUMBER;
    status = meta_sai_set_oid(SAI_OBJECT_TYPE_SWITCH, switchid, &attr, &dummy_failure_sai_set_oid);
    META_ASSERT_FAIL(status);

    attr.id = SAI_SWITCH_ATTR_PORT_NUMBER;
    status = meta_sai_set_oid(SAI_OBJECT_TYPE_SWITCH, switchid, &attr, &dummy_success_sai_set_oid);
    META_ASSERT_FAIL(status);

    attr.id = SAI_SWITCH_ATTR_SWITCHING_MODE;
    attr.value.s32 = 0x1000;
    status = meta_sai_set_oid(SAI_OBJECT_TYPE_SWITCH, switchid, &attr, &dummy_success_sai_set_oid);
    META_ASSERT_FAIL(status);

    // enum
    attr.id = SAI_SWITCH_ATTR_SWITCHING_MODE;
    attr.value.s32 = SAI_SWITCH_SWITCHING_MODE_STORE_AND_FORWARD;
    status = meta_sai_set_oid(SAI_OBJECT_TYPE_SWITCH, switchid, &attr, &dummy_success_sai_set_oid);
    META_ASSERT_SUCCESS(status);

    // bool
    attr.id = SAI_SWITCH_ATTR_BCAST_CPU_FLOOD_ENABLE;
    attr.value.booldata = SAI_SWITCH_SWITCHING_MODE_STORE_AND_FORWARD;
    status = meta_sai_set_oid(SAI_OBJECT_TYPE_SWITCH, switchid, &attr, &dummy_success_sai_set_oid);
    META_ASSERT_SUCCESS(status);

    // mac
    sai_mac_t mac = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66};

    attr.id = SAI_SWITCH_ATTR_SRC_MAC_ADDRESS;
    memcpy(attr.value.mac, mac, sizeof(mac));
    status = meta_sai_set_oid(SAI_OBJECT_TYPE_SWITCH, switchid, &attr, &dummy_success_sai_set_oid);
    META_ASSERT_SUCCESS(status);

    // uint8
    attr.id = SAI_SWITCH_ATTR_QOS_DEFAULT_TC;
    attr.value.u8 = 0x11;
    status = meta_sai_set_oid(SAI_OBJECT_TYPE_SWITCH, switchid, &attr, &dummy_success_sai_set_oid);
    META_ASSERT_SUCCESS(status);

    // object id with not allowed null

// currently hash is read only
//
//    // null oid
//    attr.id = SAI_SWITCH_ATTR_LAG_HASH_IPV6;
//    attr.value.oid = SAI_NULL_OBJECT_ID;
//    status = meta_sai_set_oid(SAI_OBJECT_TYPE_SWITCH, switchid, &attr, &dummy_success_sai_set_oid);
//    META_ASSERT_FAIL(status);
//
//    // wrong object type
//    attr.id = SAI_SWITCH_ATTR_LAG_HASH_IPV6;
//    attr.value.oid = create_dummy_object_id(SAI_OBJECT_TYPE_LAG);
//    status = meta_sai_set_oid(SAI_OBJECT_TYPE_SWITCH, switchid, &attr, &dummy_success_sai_set_oid);
//    META_ASSERT_FAIL(status);
//
//    // valid object (object must exist)
//    attr.id = SAI_SWITCH_ATTR_LAG_HASH_IPV6;
//    attr.value.oid = create_dummy_object_id(SAI_OBJECT_TYPE_HASH);
//
//    object_reference_insert(attr.value.oid);
//
//    status = meta_sai_set_oid(SAI_OBJECT_TYPE_SWITCH, switchid, &attr, &dummy_success_sai_set_oid);
//    META_ASSERT_SUCCESS(status);
//
//    META_ASSERT_TRUE(object_reference_count(attr.value.oid) == 1);

    // object id with allowed null

    // null oid
    attr.id = SAI_SWITCH_ATTR_QOS_DOT1P_TO_TC_MAP;
    attr.value.oid = SAI_NULL_OBJECT_ID;
    status = meta_sai_set_oid(SAI_OBJECT_TYPE_SWITCH, switchid, &attr, &dummy_success_sai_set_oid);
    META_ASSERT_SUCCESS(status);

    // wrong object
    attr.id = SAI_SWITCH_ATTR_QOS_DOT1P_TO_TC_MAP;
    attr.value.oid = create_dummy_object_id(SAI_OBJECT_TYPE_LAG, switchid);

    object_reference_insert(attr.value.oid);

    status = meta_sai_set_oid(SAI_OBJECT_TYPE_SWITCH, switchid, &attr, &dummy_success_sai_set_oid);
    META_ASSERT_FAIL(status);

    // good object
    attr.id = SAI_SWITCH_ATTR_QOS_DOT1P_TO_TC_MAP;
    attr.value.oid = create_dummy_object_id(SAI_OBJECT_TYPE_QOS_MAP, switchid);

    sai_object_id_t oid = attr.value.oid;

    object_reference_insert(attr.value.oid);

    status = meta_sai_set_oid(SAI_OBJECT_TYPE_SWITCH, switchid, &attr, &dummy_success_sai_set_oid);
    META_ASSERT_SUCCESS(status);

    META_ASSERT_TRUE(object_reference_count(attr.value.oid) == 1);

    attr.id = SAI_SWITCH_ATTR_QOS_DOT1P_TO_TC_MAP;
    attr.value.oid = SAI_NULL_OBJECT_ID;
    status = meta_sai_set_oid(SAI_OBJECT_TYPE_SWITCH, switchid, &attr, &dummy_success_sai_set_oid);
    META_ASSERT_SUCCESS(status);

    // check if it was decreased
    META_ASSERT_TRUE(object_reference_count(oid) == 0);
}

void test_switch_get()
{
    SWSS_LOG_ENTER();

    clear_local();
    meta_init_db();

    sai_status_t    status;
    sai_attribute_t attr;

    sai_object_id_t switchid = create_switch();

    attr.id = SAI_SWITCH_ATTR_PORT_NUMBER;
    status = meta_sai_get_oid(SAI_OBJECT_TYPE_SWITCH, switchid, 0, &attr, &dummy_success_sai_get_oid);
    META_ASSERT_FAIL(status);

    status = meta_sai_get_oid(SAI_OBJECT_TYPE_SWITCH, switchid, 1000, &attr, &dummy_success_sai_get_oid);
    META_ASSERT_FAIL(status);

    status = meta_sai_get_oid(SAI_OBJECT_TYPE_SWITCH, switchid, 1, NULL, &dummy_success_sai_get_oid);
    META_ASSERT_FAIL(status);

    status = meta_sai_get_oid(SAI_OBJECT_TYPE_SWITCH, switchid, 1, NULL, &dummy_success_sai_get_oid);
    META_ASSERT_FAIL(status);

    status = meta_sai_get_oid(SAI_OBJECT_TYPE_SWITCH, switchid, 1, &attr, NULL);
    META_ASSERT_FAIL(status);

    attr.id = -1;
    status = meta_sai_get_oid(SAI_OBJECT_TYPE_SWITCH, switchid, 1, &attr, &dummy_success_sai_get_oid);
    META_ASSERT_FAIL(status);

    attr.id = SAI_SWITCH_ATTR_PORT_NUMBER;
    attr.value.u32 = 0;
    status = meta_sai_get_oid(SAI_OBJECT_TYPE_SWITCH, switchid, 1, &attr, &dummy_success_sai_get_oid);
    META_ASSERT_SUCCESS(status);

    sai_attribute_t attr1;
    attr1.id = SAI_SWITCH_ATTR_PORT_NUMBER;

    sai_attribute_t attr2;
    attr2.id = SAI_SWITCH_ATTR_DEFAULT_VIRTUAL_ROUTER_ID;
    sai_attribute_t list[2] = { attr1, attr2 };

    status = meta_sai_get_oid(SAI_OBJECT_TYPE_SWITCH, switchid, 2, list, &switch_get_virtual_router_id);
    META_ASSERT_SUCCESS(status);
}

// FDB TESTS

sai_status_t fdb_entry_get_port_id(
        _In_ const sai_fdb_entry_t* fdb_entry,
        _In_ uint32_t attr_count,
        _Inout_ sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    sai_object_id_t switch_id = fdb_entry->switch_id;

    attr_list[0].value.s32 = SAI_FDB_ENTRY_TYPE_STATIC;
    attr_list[1].value.oid = create_dummy_object_id(SAI_OBJECT_TYPE_PORT,switch_id);

    return SAI_STATUS_SUCCESS;
};

void test_fdb_entry_create()
{
    SWSS_LOG_ENTER();

    clear_local();
    meta_init_db();

    sai_status_t    status;
    sai_attribute_t attr;

    sai_object_id_t switch_id = create_switch();

    sai_fdb_entry_t fdb_entry;

    sai_mac_t mac = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66};

    memcpy(fdb_entry.mac_address, mac, sizeof(mac));
    fdb_entry.switch_id = switch_id;
    sai_object_id_t bridge_id = create_dummy_object_id(SAI_OBJECT_TYPE_BRIDGE, switch_id);
    object_reference_insert(bridge_id);
    fdb_entry.bv_id = bridge_id;

    sai_object_id_t port = create_dummy_object_id(SAI_OBJECT_TYPE_BRIDGE_PORT, switch_id);
    object_reference_insert(port);

    SWSS_LOG_NOTICE("create tests");

    SWSS_LOG_NOTICE("zero attribute count (but there are mandatory attributes)");
    attr.id = SAI_FDB_ENTRY_ATTR_TYPE;
    status = meta_sai_create_fdb_entry(&fdb_entry, 0, &attr, &dummy_success_sai_create_fdb_entry);
    META_ASSERT_FAIL(status);

    SWSS_LOG_NOTICE("attr is null");
    status = meta_sai_create_fdb_entry(&fdb_entry, 1, NULL, &dummy_success_sai_create_fdb_entry);
    META_ASSERT_FAIL(status);

    SWSS_LOG_NOTICE("fdb entry is null");
    status = meta_sai_create_fdb_entry(NULL, 1, &attr, &dummy_success_sai_create_fdb_entry);
    META_ASSERT_FAIL(status);

    sai_attribute_t list1[4] = { };

    sai_attribute_t &attr1 = list1[0];
    sai_attribute_t &attr2 = list1[1];
    sai_attribute_t &attr3 = list1[2];
    sai_attribute_t &attr4 = list1[3];

    attr1.id = SAI_FDB_ENTRY_ATTR_TYPE;
    attr1.value.s32 = SAI_FDB_ENTRY_TYPE_STATIC;

    attr2.id = SAI_FDB_ENTRY_ATTR_BRIDGE_PORT_ID;
    attr2.value.oid = port;

    attr3.id = SAI_FDB_ENTRY_ATTR_PACKET_ACTION;
    attr3.value.s32 = SAI_PACKET_ACTION_FORWARD;

    attr4.id = -1;

    SWSS_LOG_NOTICE("create function is null");
    status = meta_sai_create_fdb_entry(&fdb_entry, 3, list1, NULL);
    META_ASSERT_FAIL(status);

    SWSS_LOG_NOTICE("invalid attribute id");
    status = meta_sai_create_fdb_entry(&fdb_entry, 4, list1, &dummy_success_sai_create_fdb_entry);
    META_ASSERT_FAIL(status);

//    // packet action is now optional
//    SWSS_LOG_NOTICE("passing optional attribute");
//    status = meta_sai_create_fdb_entry(&fdb_entry, 1, list1, &dummy_success_sai_create_fdb_entry);
//    META_ASSERT_SUCCESS(status);

    attr2.value.oid = create_dummy_object_id(SAI_OBJECT_TYPE_HASH, switch_id);

    SWSS_LOG_NOTICE("invalid attribute value on oid");
    status = meta_sai_create_fdb_entry(&fdb_entry, 3, list1, &dummy_success_sai_create_fdb_entry);
    META_ASSERT_FAIL(status);

    attr2.value.oid = create_dummy_object_id(SAI_OBJECT_TYPE_PORT, switch_id);

    SWSS_LOG_NOTICE("non existing object on oid");
    status = meta_sai_create_fdb_entry(&fdb_entry, 3, list1, &dummy_success_sai_create_fdb_entry);
    META_ASSERT_FAIL(status);

    attr2.value.oid = port;
    attr3.value.s32 = 0x100;

    SWSS_LOG_NOTICE("invalid attribute value on enum");
    status = meta_sai_create_fdb_entry(&fdb_entry, 3, list1, &dummy_success_sai_create_fdb_entry);
    META_ASSERT_FAIL(status);

    attr3.value.s32 = SAI_PACKET_ACTION_FORWARD;

    sai_attribute_t list2[4] = { attr1, attr2, attr3, attr3 };

    SWSS_LOG_NOTICE("repeated attribute id");
    status = meta_sai_create_fdb_entry(&fdb_entry, 4, list2, &dummy_success_sai_create_fdb_entry);
    META_ASSERT_FAIL(status);

    SWSS_LOG_NOTICE("correct");
    status = meta_sai_create_fdb_entry(&fdb_entry, 3, list2, &dummy_success_sai_create_fdb_entry);
    META_ASSERT_SUCCESS(status);

    SWSS_LOG_NOTICE("already exists");
    status = meta_sai_create_fdb_entry(&fdb_entry, 3, list2, &dummy_success_sai_create_fdb_entry);
    META_ASSERT_FAIL(status);
}

void test_fdb_entry_remove()
{
    SWSS_LOG_ENTER();

    clear_local();
    meta_init_db();

    sai_status_t    status;

    sai_object_id_t switch_id = create_switch();

    sai_fdb_entry_t fdb_entry;

    sai_mac_t mac = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66};

    memcpy(fdb_entry.mac_address, mac, sizeof(mac));
    fdb_entry.switch_id = switch_id;
    sai_object_id_t bridge_id = create_dummy_object_id(SAI_OBJECT_TYPE_BRIDGE, switch_id);
    object_reference_insert(bridge_id);
    fdb_entry.bv_id= bridge_id;

    sai_object_id_t port = create_dummy_object_id(SAI_OBJECT_TYPE_BRIDGE_PORT,switch_id);
    object_reference_insert(port);

    sai_attribute_t list1[3] = { };

    sai_attribute_t &attr1 = list1[0];
    sai_attribute_t &attr2 = list1[1];
    sai_attribute_t &attr3 = list1[2];

    attr1.id = SAI_FDB_ENTRY_ATTR_TYPE;
    attr1.value.s32 = SAI_FDB_ENTRY_TYPE_STATIC;

    attr2.id = SAI_FDB_ENTRY_ATTR_BRIDGE_PORT_ID;
    attr2.value.oid = port;

    attr3.id = SAI_FDB_ENTRY_ATTR_PACKET_ACTION;
    attr3.value.s32 = SAI_PACKET_ACTION_FORWARD;

    SWSS_LOG_NOTICE("creating fdb entry");
    status = meta_sai_create_fdb_entry(&fdb_entry, 3, list1, &dummy_success_sai_create_fdb_entry);
    META_ASSERT_SUCCESS(status);

    SWSS_LOG_NOTICE("remove tests");

    SWSS_LOG_NOTICE("fdb_entry is null");
    status = meta_sai_remove_fdb_entry(NULL, &dummy_success_sai_remove_fdb_entry);
    META_ASSERT_FAIL(status);

    //SWSS_LOG_NOTICE("invalid vlan");
    //status = meta_sai_remove_fdb_entry(&fdb_entry, &dummy_success_sai_remove_fdb_entry);
    //META_ASSERT_FAIL(status);

    fdb_entry.mac_address[0] = 1;

    SWSS_LOG_NOTICE("invalid mac");
    status = meta_sai_remove_fdb_entry(&fdb_entry, &dummy_success_sai_remove_fdb_entry);
    META_ASSERT_FAIL(status);

    fdb_entry.mac_address[0] = 0x11;

    sai_object_meta_key_t meta = { .objecttype = SAI_OBJECT_TYPE_FDB_ENTRY, .objectkey = { .key = { .fdb_entry = fdb_entry } } };

    std::string key = sai_serialize_object_meta_key(meta);

    META_ASSERT_TRUE(object_exists(key));

    SWSS_LOG_NOTICE("success");
    status = meta_sai_remove_fdb_entry(&fdb_entry, &dummy_success_sai_remove_fdb_entry);
    META_ASSERT_SUCCESS(status);

    META_ASSERT_TRUE(!object_exists(key));
}

void test_fdb_entry_set()
{
    SWSS_LOG_ENTER();

    clear_local();
    meta_init_db();

    sai_status_t    status;
    sai_attribute_t attr;

    sai_object_id_t switch_id = create_switch();

    sai_fdb_entry_t fdb_entry;

    sai_mac_t mac = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66};

    memcpy(fdb_entry.mac_address, mac, sizeof(mac));
    fdb_entry.switch_id = switch_id;

    // TODO we should use CREATE for this
    sai_object_meta_key_t meta_key_fdb = { .objecttype = SAI_OBJECT_TYPE_FDB_ENTRY, .objectkey = { .key = { .fdb_entry = fdb_entry } } };
    std::string fdb_key = sai_serialize_object_meta_key(meta_key_fdb);
    ObjectAttrHash[fdb_key] = { };

    SWSS_LOG_NOTICE("attr is null");
    status = meta_sai_set_fdb_entry(&fdb_entry, NULL, &dummy_success_sai_set_fdb_entry);
    META_ASSERT_FAIL(status);

    SWSS_LOG_NOTICE("fdb entry is null");
    status = meta_sai_set_fdb_entry(NULL, &attr, &dummy_success_sai_set_fdb_entry);
    META_ASSERT_FAIL(status);

    SWSS_LOG_NOTICE("setting read only object");
    attr.id = SAI_FDB_ENTRY_ATTR_BRIDGE_PORT_ID;
    attr.value.oid = create_dummy_object_id(SAI_OBJECT_TYPE_BRIDGE_PORT,switch_id);

    status = meta_sai_set_fdb_entry(&fdb_entry, &attr, &dummy_success_sai_set_fdb_entry);
    META_ASSERT_FAIL(status);

    SWSS_LOG_NOTICE("setting invalid attrib id");
    attr.id = -1;
    status = meta_sai_set_fdb_entry(&fdb_entry, &attr, &dummy_success_sai_set_fdb_entry);
    META_ASSERT_FAIL(status);

    //SWSS_LOG_NOTICE("invalid vlan");
    //attr.id = SAI_FDB_ENTRY_ATTR_TYPE;
    //attr.value.s32 = SAI_FDB_ENTRY_TYPE_STATIC;
    //status = meta_sai_set_fdb_entry(&fdb_entry, &attr, &dummy_success_sai_set_fdb_entry);
    //META_ASSERT_FAIL(status);

    //SWSS_LOG_NOTICE("vlan outside range");
    //attr.id = SAI_FDB_ENTRY_ATTR_TYPE;
    //attr.value.s32 = SAI_FDB_ENTRY_TYPE_STATIC;
    //status = meta_sai_set_fdb_entry(&fdb_entry, &attr, &dummy_success_sai_set_fdb_entry);
    //META_ASSERT_FAIL(status);

    // correct
    attr.id = SAI_FDB_ENTRY_ATTR_TYPE;
    attr.value.s32 = SAI_FDB_ENTRY_TYPE_STATIC;
    status = meta_sai_set_fdb_entry(&fdb_entry, &attr, &dummy_success_sai_set_fdb_entry);
    META_ASSERT_SUCCESS(status);

    // TODO check references ?
}

void test_fdb_entry_get()
{
    SWSS_LOG_ENTER();

    clear_local();
    meta_init_db();

    sai_status_t    status;
    sai_attribute_t attr;

    sai_fdb_entry_t fdb_entry;

    sai_mac_t mac = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66};

    sai_object_id_t switch_id = create_switch();

    memcpy(fdb_entry.mac_address, mac, sizeof(mac));
    fdb_entry.switch_id = switch_id;

    sai_object_id_t bridge_id = create_dummy_object_id(SAI_OBJECT_TYPE_BRIDGE, switch_id);
    object_reference_insert(bridge_id);
    fdb_entry.bv_id = bridge_id;

    // TODO we should use CREATE for this
    sai_object_meta_key_t meta_key_fdb = { .objecttype = SAI_OBJECT_TYPE_FDB_ENTRY, .objectkey = { .key = { .fdb_entry = fdb_entry } } };
    std::string fdb_key = sai_serialize_object_meta_key(meta_key_fdb);
    ObjectAttrHash[fdb_key] = { };

    attr.id = SAI_FDB_ENTRY_ATTR_TYPE;
    attr.value.s32 = SAI_FDB_ENTRY_TYPE_STATIC;
    status = meta_sai_set_fdb_entry(&fdb_entry, &attr, &dummy_success_sai_set_fdb_entry);
    META_ASSERT_SUCCESS(status);

    SWSS_LOG_NOTICE("get test");

    // zero attribute count
    attr.id = SAI_FDB_ENTRY_ATTR_TYPE;
    status = meta_sai_get_fdb_entry(&fdb_entry, 0, &attr, &dummy_success_sai_get_fdb_entry);
    META_ASSERT_FAIL(status);

    // attr is null
    status = meta_sai_get_fdb_entry(&fdb_entry, 1, NULL, &dummy_success_sai_get_fdb_entry);
    META_ASSERT_FAIL(status);

    // fdb entry is null
    status = meta_sai_get_fdb_entry(NULL, 1, &attr, &dummy_success_sai_get_fdb_entry);
    META_ASSERT_FAIL(status);

    // get function is null
    status = meta_sai_get_fdb_entry(&fdb_entry, 1, &attr, NULL);
    META_ASSERT_FAIL(status);

    // attr id out of range
    attr.id = -1;
    status = meta_sai_get_fdb_entry(&fdb_entry, 1, &attr, &dummy_success_sai_get_fdb_entry);
    META_ASSERT_FAIL(status);

    // correct single valid attribute
    attr.id = SAI_FDB_ENTRY_ATTR_TYPE;
    status = meta_sai_get_fdb_entry(&fdb_entry, 1, &attr, &dummy_success_sai_get_fdb_entry);
    META_ASSERT_SUCCESS(status);

    // correct 2 attributes
    sai_attribute_t attr1;
    attr1.id = SAI_FDB_ENTRY_ATTR_TYPE;

    sai_attribute_t attr2;
    attr2.id = SAI_FDB_ENTRY_ATTR_BRIDGE_PORT_ID;
    sai_attribute_t list[2] = { attr1, attr2 };

    status = meta_sai_get_fdb_entry(&fdb_entry, 2, list, &fdb_entry_get_port_id);
    META_ASSERT_SUCCESS(status);
}

void test_fdb_entry_flow()
{
    SWSS_LOG_ENTER();

    SWSS_LOG_TIMER("fdb flow");

    clear_local();
    meta_init_db();

    sai_object_id_t switch_id = create_switch();
    sai_status_t    status;
    sai_attribute_t attr;

    sai_fdb_entry_t fdb_entry;

    sai_mac_t mac = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66};

    memcpy(fdb_entry.mac_address, mac, sizeof(mac));
    fdb_entry.switch_id = switch_id;

    sai_object_id_t bridge_id = create_dummy_object_id(SAI_OBJECT_TYPE_BRIDGE, switch_id);
    object_reference_insert(bridge_id);
    fdb_entry.bv_id= bridge_id;

    sai_object_id_t lag = create_dummy_object_id(SAI_OBJECT_TYPE_BRIDGE_PORT,switch_id);
    object_reference_insert(lag);

    sai_attribute_t list[4] = { };

    sai_attribute_t &attr1 = list[0];
    sai_attribute_t &attr2 = list[1];
    sai_attribute_t &attr3 = list[2];
    sai_attribute_t &attr4 = list[3];

    attr1.id = SAI_FDB_ENTRY_ATTR_TYPE;
    attr1.value.s32 = SAI_FDB_ENTRY_TYPE_STATIC;

    attr2.id = SAI_FDB_ENTRY_ATTR_BRIDGE_PORT_ID;
    attr2.value.oid = lag;

    attr3.id = SAI_FDB_ENTRY_ATTR_PACKET_ACTION;
    attr3.value.s32 = SAI_PACKET_ACTION_FORWARD;

    attr4.id = SAI_FDB_ENTRY_ATTR_META_DATA;
    attr4.value.u32 = 0x12345678;

    SWSS_LOG_NOTICE("create");
    status = meta_sai_create_fdb_entry(&fdb_entry, 4, list, dummy_success_sai_create_fdb_entry);
    META_ASSERT_SUCCESS(status);

    SWSS_LOG_NOTICE("create existing");
    status = meta_sai_create_fdb_entry(&fdb_entry, 4, list, dummy_success_sai_create_fdb_entry);
    META_ASSERT_FAIL(status);

    SWSS_LOG_NOTICE("set");
    attr.id = SAI_FDB_ENTRY_ATTR_TYPE;
    attr.value.s32 = SAI_FDB_ENTRY_TYPE_DYNAMIC;
    status = meta_sai_set_fdb_entry(&fdb_entry, &attr, &dummy_success_sai_set_fdb_entry);
    META_ASSERT_SUCCESS(status);

    SWSS_LOG_NOTICE("set");
    attr.id = SAI_FDB_ENTRY_ATTR_META_DATA;
    attr.value.u32 = SAI_FDB_ENTRY_TYPE_DYNAMIC;
    status = meta_sai_set_fdb_entry(&fdb_entry, &attr, &dummy_success_sai_set_fdb_entry);
    META_ASSERT_SUCCESS(status);

    SWSS_LOG_NOTICE("get");
    status = meta_sai_get_fdb_entry(&fdb_entry, 4, list, &dummy_success_sai_get_fdb_entry);
    META_ASSERT_SUCCESS(status);

    SWSS_LOG_NOTICE("remove");
    status = meta_sai_remove_fdb_entry(&fdb_entry, &dummy_success_sai_remove_fdb_entry);
    META_ASSERT_SUCCESS(status);

    SWSS_LOG_NOTICE("remove non existing");
    status = meta_sai_remove_fdb_entry(&fdb_entry, &dummy_success_sai_remove_fdb_entry);
    META_ASSERT_FAIL(status);
}

// NEIGHBOR TESTS

void test_neighbor_entry_create()
{
    SWSS_LOG_ENTER();

    clear_local();
    meta_init_db();

    sai_object_id_t switch_id = create_switch();
    sai_status_t    status;
    sai_attribute_t attr;

    sai_mac_t mac = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66};

    sai_neighbor_entry_t neighbor_entry;

    // TODO we should use create
    sai_object_id_t rif = create_dummy_object_id(SAI_OBJECT_TYPE_ROUTER_INTERFACE,switch_id);
    object_reference_insert(rif);
    sai_object_meta_key_t meta_key_rif = { .objecttype = SAI_OBJECT_TYPE_ROUTER_INTERFACE, .objectkey = { .key = { .object_id = rif } } };
    std::string rif_key = sai_serialize_object_meta_key(meta_key_rif);
    ObjectAttrHash[rif_key] = { };

    neighbor_entry.ip_address.addr_family = SAI_IP_ADDR_FAMILY_IPV4;
    neighbor_entry.ip_address.addr.ip4 = htonl(0x0a00000f);
    neighbor_entry.rif_id = rif;
    neighbor_entry.switch_id = switch_id;

    SWSS_LOG_NOTICE("create tests");

    SWSS_LOG_NOTICE("zero attribute count (but there are mandatory attributes)");
    status = meta_sai_create_neighbor_entry(&neighbor_entry, 0, &attr, &dummy_success_sai_create_neighbor_entry);
    META_ASSERT_FAIL(status);

    SWSS_LOG_NOTICE("attr is null");
    status = meta_sai_create_neighbor_entry(&neighbor_entry, 1, NULL, &dummy_success_sai_create_neighbor_entry);
    META_ASSERT_FAIL(status);

    SWSS_LOG_NOTICE("neighbor entry is null");
    status = meta_sai_create_neighbor_entry(NULL, 1, &attr, &dummy_success_sai_create_neighbor_entry);
    META_ASSERT_FAIL(status);

    sai_attribute_t list[3] = { };

    sai_attribute_t &attr1 = list[0];
    sai_attribute_t &attr2 = list[1];
    sai_attribute_t &attr3 = list[2];

    attr1.id = SAI_NEIGHBOR_ENTRY_ATTR_DST_MAC_ADDRESS;
    memcpy(attr1.value.mac, mac, 6);

    attr2.id = SAI_NEIGHBOR_ENTRY_ATTR_PACKET_ACTION;
    attr2.value.s32 = SAI_PACKET_ACTION_FORWARD;

    attr3.id = -1;

    SWSS_LOG_NOTICE("create function is null");
    status = meta_sai_create_neighbor_entry(&neighbor_entry, 2, list, NULL);
    META_ASSERT_FAIL(status);

    SWSS_LOG_NOTICE("invalid attribute id");
    status = meta_sai_create_neighbor_entry(&neighbor_entry, 3, list, &dummy_success_sai_create_neighbor_entry);
    META_ASSERT_FAIL(status);

    attr2.value.s32 = SAI_PACKET_ACTION_FORWARD + 0x100;
    SWSS_LOG_NOTICE("invalid attribute value on enum");
    status = meta_sai_create_neighbor_entry(&neighbor_entry, 2, list, &dummy_success_sai_create_neighbor_entry);
    META_ASSERT_FAIL(status);

    attr2.value.s32 = SAI_PACKET_ACTION_FORWARD;

    sai_attribute_t list2[4] = { attr1, attr2, attr2 };

    SWSS_LOG_NOTICE("repeated attribute id");
    status = meta_sai_create_neighbor_entry(&neighbor_entry, 3, list2, &dummy_success_sai_create_neighbor_entry);
    META_ASSERT_FAIL(status);

    SWSS_LOG_NOTICE("correct ipv4");
    status = meta_sai_create_neighbor_entry(&neighbor_entry, 2, list2, &dummy_success_sai_create_neighbor_entry);
    META_ASSERT_SUCCESS(status);

    neighbor_entry.ip_address.addr_family = SAI_IP_ADDR_FAMILY_IPV6;
    sai_ip6_t ip6 = {0x00, 0x11, 0x22, 0x33,0x44, 0x55, 0x66,0x77, 0x88, 0x99, 0xaa, 0xbb,0xcc,0xdd,0xee,0xff};
    memcpy(neighbor_entry.ip_address.addr.ip6, ip6, 16);

    SWSS_LOG_NOTICE("correct ipv6");
    status = meta_sai_create_neighbor_entry(&neighbor_entry, 2, list2, &dummy_success_sai_create_neighbor_entry);
    META_ASSERT_SUCCESS(status);

    SWSS_LOG_NOTICE("already exists");
    status = meta_sai_create_neighbor_entry(&neighbor_entry, 2, list2, &dummy_success_sai_create_neighbor_entry);
    META_ASSERT_FAIL(status);
}

void test_neighbor_entry_remove()
{
    SWSS_LOG_ENTER();

    clear_local();
    meta_init_db();

    sai_status_t    status;

    sai_object_id_t switch_id = create_switch();
    sai_neighbor_entry_t neighbor_entry;

    sai_mac_t mac = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66};

    sai_attribute_t list[2] = { };

    sai_attribute_t &attr1 = list[0];
    sai_attribute_t &attr2 = list[1];

    attr1.id = SAI_NEIGHBOR_ENTRY_ATTR_DST_MAC_ADDRESS;
    memcpy(attr1.value.mac, mac, 6);

    attr2.id = SAI_NEIGHBOR_ENTRY_ATTR_PACKET_ACTION;
    attr2.value.s32 = SAI_PACKET_ACTION_FORWARD;

    // TODO we should use create
    sai_object_id_t rif = create_dummy_object_id(SAI_OBJECT_TYPE_ROUTER_INTERFACE,switch_id);
    object_reference_insert(rif);
    sai_object_meta_key_t meta_key_rif = { .objecttype = SAI_OBJECT_TYPE_ROUTER_INTERFACE, .objectkey = { .key = { .object_id = rif } } };
    std::string rif_key = sai_serialize_object_meta_key(meta_key_rif);
    ObjectAttrHash[rif_key] = { };

    neighbor_entry.ip_address.addr_family = SAI_IP_ADDR_FAMILY_IPV4;
    neighbor_entry.ip_address.addr.ip4 = htonl(0x0a00000f);
    neighbor_entry.rif_id = rif;
    neighbor_entry.switch_id = switch_id;

    SWSS_LOG_NOTICE("create");

    SWSS_LOG_NOTICE("correct ipv4");
    status = meta_sai_create_neighbor_entry(&neighbor_entry, 2, list, &dummy_success_sai_create_neighbor_entry);
    META_ASSERT_SUCCESS(status);

    neighbor_entry.ip_address.addr_family = SAI_IP_ADDR_FAMILY_IPV6;
    sai_ip6_t ip6 = {0x00, 0x11, 0x22, 0x33,0x44, 0x55, 0x66,0x77, 0x88, 0x99, 0xaa, 0xbb,0xcc,0xdd,0xee,0xff};
    memcpy(neighbor_entry.ip_address.addr.ip6, ip6, 16);

    SWSS_LOG_NOTICE("correct ipv6");
    status = meta_sai_create_neighbor_entry(&neighbor_entry, 2, list, &dummy_success_sai_create_neighbor_entry);
    META_ASSERT_SUCCESS(status);

    SWSS_LOG_NOTICE("remove tests");

    SWSS_LOG_NOTICE("neighbor_entry is null");
    status = meta_sai_remove_neighbor_entry(NULL, &dummy_success_sai_remove_neighbor_entry);
    META_ASSERT_FAIL(status);

    neighbor_entry.rif_id = SAI_NULL_OBJECT_ID;

    SWSS_LOG_NOTICE("invalid object id null");
    status = meta_sai_remove_neighbor_entry(&neighbor_entry, &dummy_success_sai_remove_neighbor_entry);
    META_ASSERT_FAIL(status);

    neighbor_entry.rif_id = create_dummy_object_id(SAI_OBJECT_TYPE_HASH,switch_id);

    SWSS_LOG_NOTICE("invalid object id hash");
    status = meta_sai_remove_neighbor_entry(&neighbor_entry, &dummy_success_sai_remove_neighbor_entry);
    META_ASSERT_FAIL(status);

    neighbor_entry.rif_id = create_dummy_object_id(SAI_OBJECT_TYPE_ROUTER_INTERFACE,switch_id);

    SWSS_LOG_NOTICE("invalid object id router");
    status = meta_sai_remove_neighbor_entry(&neighbor_entry, &dummy_success_sai_remove_neighbor_entry);
    META_ASSERT_FAIL(status);

    neighbor_entry.rif_id = rif;

    sai_object_meta_key_t meta = { .objecttype = SAI_OBJECT_TYPE_NEIGHBOR_ENTRY, .objectkey = { .key = { .neighbor_entry = neighbor_entry } } };

    std::string key = sai_serialize_object_meta_key(meta);

    META_ASSERT_TRUE(object_exists(key));

    SWSS_LOG_NOTICE("success");
    status = meta_sai_remove_neighbor_entry(&neighbor_entry, &dummy_success_sai_remove_neighbor_entry);
    META_ASSERT_SUCCESS(status);

    META_ASSERT_TRUE(!object_exists(key));
}

void test_neighbor_entry_set()
{
    SWSS_LOG_ENTER();

    clear_local();
    meta_init_db();

    sai_status_t    status;

    sai_attribute_t attr;

    sai_object_id_t switch_id = create_switch();
    sai_neighbor_entry_t neighbor_entry;

    sai_mac_t mac = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66};

    sai_attribute_t list[2] = { };

    sai_attribute_t &attr1 = list[0];
    sai_attribute_t &attr2 = list[1];

    attr1.id = SAI_NEIGHBOR_ENTRY_ATTR_DST_MAC_ADDRESS;
    memcpy(attr1.value.mac, mac, 6);

    attr2.id = SAI_NEIGHBOR_ENTRY_ATTR_PACKET_ACTION;
    attr2.value.s32 = SAI_PACKET_ACTION_FORWARD;

    // TODO we should use create
    sai_object_id_t rif = create_dummy_object_id(SAI_OBJECT_TYPE_ROUTER_INTERFACE,switch_id);
    object_reference_insert(rif);
    sai_object_meta_key_t meta_key_rif = { .objecttype = SAI_OBJECT_TYPE_ROUTER_INTERFACE, .objectkey = { .key = { .object_id = rif } } };
    std::string rif_key = sai_serialize_object_meta_key(meta_key_rif);
    ObjectAttrHash[rif_key] = { };

    neighbor_entry.ip_address.addr_family = SAI_IP_ADDR_FAMILY_IPV4;
    neighbor_entry.ip_address.addr.ip4 = htonl(0x0a00000f);
    neighbor_entry.rif_id = rif;
    neighbor_entry.switch_id = switch_id;

    SWSS_LOG_NOTICE("create");

    SWSS_LOG_NOTICE("correct ipv4");
    status = meta_sai_create_neighbor_entry(&neighbor_entry, 2, list, &dummy_success_sai_create_neighbor_entry);
    META_ASSERT_SUCCESS(status);

    neighbor_entry.ip_address.addr_family = SAI_IP_ADDR_FAMILY_IPV6;
    sai_ip6_t ip6 = {0x00, 0x11, 0x22, 0x33,0x44, 0x55, 0x66,0x77, 0x88, 0x99, 0xaa, 0xbb,0xcc,0xdd,0xee,0xff};
    memcpy(neighbor_entry.ip_address.addr.ip6, ip6, 16);

    SWSS_LOG_NOTICE("correct ipv6");
    status = meta_sai_create_neighbor_entry(&neighbor_entry, 2, list, &dummy_success_sai_create_neighbor_entry);
    META_ASSERT_SUCCESS(status);

    SWSS_LOG_NOTICE("set tests");

    SWSS_LOG_NOTICE("attr is null");
    status = meta_sai_set_neighbor_entry(&neighbor_entry, NULL, &dummy_success_sai_set_neighbor_entry);
    META_ASSERT_FAIL(status);

    SWSS_LOG_NOTICE("neighbor entry is null");
    status = meta_sai_set_neighbor_entry(NULL, &attr, &dummy_success_sai_set_neighbor_entry);
    META_ASSERT_FAIL(status);

    SWSS_LOG_NOTICE("setting invalid attrib id");
    attr.id = -1;
    status = meta_sai_set_neighbor_entry(&neighbor_entry, &attr, &dummy_success_sai_set_neighbor_entry);
    META_ASSERT_FAIL(status);

    SWSS_LOG_NOTICE("value outside range");
    attr.id = SAI_NEIGHBOR_ENTRY_ATTR_PACKET_ACTION;
    attr.value.s32 = 0x100;
    status = meta_sai_set_neighbor_entry(&neighbor_entry, &attr, &dummy_success_sai_set_neighbor_entry);
    META_ASSERT_FAIL(status);

    // correct
    attr.id = SAI_NEIGHBOR_ENTRY_ATTR_PACKET_ACTION;
    attr.value.s32 = SAI_PACKET_ACTION_DROP;
    status = meta_sai_set_neighbor_entry(&neighbor_entry, &attr, &dummy_success_sai_set_neighbor_entry);
    META_ASSERT_SUCCESS(status);
}

void test_neighbor_entry_get()
{
    SWSS_LOG_ENTER();

    clear_local();
    meta_init_db();

    sai_status_t    status;

    sai_attribute_t attr;

    sai_object_id_t switch_id = create_switch();
    sai_neighbor_entry_t neighbor_entry;

    sai_mac_t mac = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66};

    sai_attribute_t list[2] = { };

    sai_attribute_t &attr1 = list[0];
    sai_attribute_t &attr2 = list[1];

    attr1.id = SAI_NEIGHBOR_ENTRY_ATTR_DST_MAC_ADDRESS;
    memcpy(attr1.value.mac, mac, 6);

    attr2.id = SAI_NEIGHBOR_ENTRY_ATTR_PACKET_ACTION;
    attr2.value.s32 = SAI_PACKET_ACTION_FORWARD;

    // TODO we should use create
    sai_object_id_t rif = create_dummy_object_id(SAI_OBJECT_TYPE_ROUTER_INTERFACE,switch_id);
    object_reference_insert(rif);
    sai_object_meta_key_t meta_key_rif = { .objecttype = SAI_OBJECT_TYPE_ROUTER_INTERFACE, .objectkey = { .key = { .object_id = rif } } };
    std::string rif_key = sai_serialize_object_meta_key(meta_key_rif);
    ObjectAttrHash[rif_key] = { };

    neighbor_entry.ip_address.addr_family = SAI_IP_ADDR_FAMILY_IPV4;
    neighbor_entry.ip_address.addr.ip4 = htonl(0x0a00000f);
    neighbor_entry.rif_id = rif;
    neighbor_entry.switch_id = switch_id;

    SWSS_LOG_NOTICE("create");

    SWSS_LOG_NOTICE("correct ipv4");
    status = meta_sai_create_neighbor_entry(&neighbor_entry, 2, list, &dummy_success_sai_create_neighbor_entry);
    META_ASSERT_SUCCESS(status);

    neighbor_entry.ip_address.addr_family = SAI_IP_ADDR_FAMILY_IPV6;
    sai_ip6_t ip6 = {0x00, 0x11, 0x22, 0x33,0x44, 0x55, 0x66,0x77, 0x88, 0x99, 0xaa, 0xbb,0xcc,0xdd,0xee,0xff};
    memcpy(neighbor_entry.ip_address.addr.ip6, ip6, 16);

    SWSS_LOG_NOTICE("correct ipv6");
    status = meta_sai_create_neighbor_entry(&neighbor_entry, 2, list, &dummy_success_sai_create_neighbor_entry);
    META_ASSERT_SUCCESS(status);

    SWSS_LOG_NOTICE("get test");

    SWSS_LOG_NOTICE("zero attribute count");
    attr.id = SAI_NEIGHBOR_ENTRY_ATTR_PACKET_ACTION;
    status = meta_sai_get_neighbor_entry(&neighbor_entry, 0, &attr, &dummy_success_sai_get_neighbor_entry);
    META_ASSERT_FAIL(status);

    SWSS_LOG_NOTICE("attr is null");
    status = meta_sai_get_neighbor_entry(&neighbor_entry, 1, NULL, &dummy_success_sai_get_neighbor_entry);
    META_ASSERT_FAIL(status);

    SWSS_LOG_NOTICE("neighbor entry is null");
    status = meta_sai_get_neighbor_entry(NULL, 1, &attr, &dummy_success_sai_get_neighbor_entry);
    META_ASSERT_FAIL(status);

    SWSS_LOG_NOTICE("get function is null");
    status = meta_sai_get_neighbor_entry(&neighbor_entry, 1, &attr, NULL);
    META_ASSERT_FAIL(status);

    SWSS_LOG_NOTICE("attr id out of range");
    attr.id = -1;
    status = meta_sai_get_neighbor_entry(&neighbor_entry, 1, &attr, &dummy_success_sai_get_neighbor_entry);
    META_ASSERT_FAIL(status);

    SWSS_LOG_NOTICE("correct single valid attribute");
    attr.id = SAI_NEIGHBOR_ENTRY_ATTR_PACKET_ACTION;
    status = meta_sai_get_neighbor_entry(&neighbor_entry, 1, &attr, &dummy_success_sai_get_neighbor_entry);
    META_ASSERT_SUCCESS(status);

    SWSS_LOG_NOTICE("correct 2 attributes");

    sai_attribute_t attr3;
    sai_attribute_t attr4;
    attr3.id = SAI_NEIGHBOR_ENTRY_ATTR_PACKET_ACTION;
    attr3.value.s32 = 1;
    attr4.id = SAI_NEIGHBOR_ENTRY_ATTR_NO_HOST_ROUTE;

    sai_attribute_t list2[2] = { attr3, attr4 };
    status = meta_sai_get_neighbor_entry(&neighbor_entry, 2, list2, &dummy_success_sai_get_neighbor_entry);
    META_ASSERT_SUCCESS(status);
}

void test_neighbor_entry_flow()
{
    SWSS_LOG_ENTER();

    clear_local();
    meta_init_db();

    sai_status_t    status;

    sai_object_id_t switch_id = create_switch();
    sai_neighbor_entry_t neighbor_entry;

    sai_mac_t mac = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66};

    sai_attribute_t list[4] = { };

    sai_attribute_t &attr1 = list[0];
    sai_attribute_t &attr2 = list[1];
    sai_attribute_t &attr3 = list[1];
    sai_attribute_t &attr4 = list[1];

    attr1.id = SAI_NEIGHBOR_ENTRY_ATTR_DST_MAC_ADDRESS;
    memcpy(attr1.value.mac, mac, 6);

    attr2.id = SAI_NEIGHBOR_ENTRY_ATTR_PACKET_ACTION;
    attr2.value.s32 = SAI_PACKET_ACTION_FORWARD;

    attr3.id = SAI_NEIGHBOR_ENTRY_ATTR_NO_HOST_ROUTE;
    attr3.value.booldata = true;

    attr4.id = SAI_NEIGHBOR_ENTRY_ATTR_META_DATA;
    attr4.value.u32 = 1;

    // TODO we should use create
    sai_object_id_t rif = create_dummy_object_id(SAI_OBJECT_TYPE_ROUTER_INTERFACE,switch_id);
    object_reference_insert(rif);
    sai_object_meta_key_t meta_key_rif = { .objecttype = SAI_OBJECT_TYPE_ROUTER_INTERFACE, .objectkey = { .key = { .object_id = rif } } };
    std::string rif_key = sai_serialize_object_meta_key(meta_key_rif);
    ObjectAttrHash[rif_key] = { };

    neighbor_entry.ip_address.addr_family = SAI_IP_ADDR_FAMILY_IPV4;
    neighbor_entry.ip_address.addr.ip4 = htonl(0x0a00000f);
    neighbor_entry.rif_id = rif;
    neighbor_entry.switch_id = switch_id;

    SWSS_LOG_NOTICE("create");

    SWSS_LOG_NOTICE("correct ipv4");
    status = meta_sai_create_neighbor_entry(&neighbor_entry, 2, list, &dummy_success_sai_create_neighbor_entry);
    META_ASSERT_SUCCESS(status);

    SWSS_LOG_NOTICE("correct ipv4 existing");
    status = meta_sai_create_neighbor_entry(&neighbor_entry, 2, list, &dummy_success_sai_create_neighbor_entry);
    META_ASSERT_FAIL(status);

    SWSS_LOG_NOTICE("set");
    status = meta_sai_set_neighbor_entry(&neighbor_entry, &attr1, &dummy_success_sai_set_neighbor_entry);
    META_ASSERT_SUCCESS(status);

    SWSS_LOG_NOTICE("set");
    status = meta_sai_set_neighbor_entry(&neighbor_entry, &attr2, &dummy_success_sai_set_neighbor_entry);
    META_ASSERT_SUCCESS(status);

    SWSS_LOG_NOTICE("set");
    status = meta_sai_set_neighbor_entry(&neighbor_entry, &attr3, &dummy_success_sai_set_neighbor_entry);
    META_ASSERT_SUCCESS(status);

    SWSS_LOG_NOTICE("set");
    status = meta_sai_set_neighbor_entry(&neighbor_entry, &attr4, &dummy_success_sai_set_neighbor_entry);
    META_ASSERT_SUCCESS(status);

    SWSS_LOG_NOTICE("remove");
    status = meta_sai_remove_neighbor_entry(&neighbor_entry, &dummy_success_sai_remove_neighbor_entry);
    META_ASSERT_SUCCESS(status);

    SWSS_LOG_NOTICE("remove existing");
    status = meta_sai_remove_neighbor_entry(&neighbor_entry, &dummy_success_sai_remove_neighbor_entry);
    META_ASSERT_FAIL(status);
}

// VLAN TESTS

void test_vlan_create()
{
    SWSS_LOG_ENTER();

    clear_local();
    meta_init_db();

    sai_status_t    status;

    sai_attribute_t vlan1_att;
    vlan1_att.id = SAI_VLAN_ATTR_VLAN_ID;
    vlan1_att.value.u16 = 1;

    sai_object_id_t vlan1_id;

    sai_object_id_t switch_id = create_switch();

    status = meta_sai_create_oid(SAI_OBJECT_TYPE_VLAN, &vlan1_id, switch_id, 1, &vlan1_att, &dummy_success_sai_create_oid);
    META_ASSERT_SUCCESS(status);

    sai_attribute_t vlan;
    vlan.id = SAI_VLAN_ATTR_VLAN_ID;
    vlan.value.u16 = 2;

    sai_object_id_t vlan_id;

    // TODO we should use create
    sai_object_id_t stp = create_dummy_object_id(SAI_OBJECT_TYPE_STP,switch_id);
    object_reference_insert(stp);
    sai_object_meta_key_t meta_key_stp = { .objecttype = SAI_OBJECT_TYPE_STP, .objectkey = { .key = { .object_id = stp } } };
    std::string stp_key = sai_serialize_object_meta_key(meta_key_stp);
    ObjectAttrHash[stp_key] = { };

    SWSS_LOG_NOTICE("create tests");

//    SWSS_LOG_NOTICE("existing vlan");
//    status = meta_sai_create_oid(SAI_OBJECT_TYPE_VLAN, &vlan_id, switch_id, 1, &vlan, &dummy_success_sai_create_oid);
//    META_ASSERT_FAIL(status);

    vlan.value.u16 = MAXIMUM_VLAN_NUMBER + 1;

//    SWSS_LOG_NOTICE("vlan outside range");
//    status = meta_sai_create_oid(SAI_OBJECT_TYPE_VLAN, &vlan_id, switch_id, 1, &vlan, &dummy_success_sai_create_oid);
//    META_ASSERT_FAIL(status);

    vlan.value.u16 = 2;

    SWSS_LOG_NOTICE("create is null");
    status = meta_sai_create_oid(SAI_OBJECT_TYPE_VLAN, &vlan_id, switch_id, 1, &vlan, NULL);
    META_ASSERT_FAIL(status);

    SWSS_LOG_NOTICE("correct");
    status = meta_sai_create_oid(SAI_OBJECT_TYPE_VLAN, &vlan_id, switch_id, 1, &vlan, &dummy_success_sai_create_oid);
    META_ASSERT_SUCCESS(status);

    SWSS_LOG_NOTICE("existing");
    status = meta_sai_create_oid(SAI_OBJECT_TYPE_VLAN, &vlan_id, switch_id, 1, &vlan, &dummy_success_sai_create_oid);
    META_ASSERT_FAIL(status);
}

void test_vlan_remove()
{
    SWSS_LOG_ENTER();

    clear_local();
    meta_init_db();

    sai_status_t status;

    sai_attribute_t vlan1_att;
    vlan1_att.id = SAI_VLAN_ATTR_VLAN_ID;
    vlan1_att.value.u16 = 1;

    sai_object_id_t vlan1_id;
    sai_object_id_t switch_id = create_switch();

    // TODO we should use create
    sai_object_id_t stp = create_dummy_object_id(SAI_OBJECT_TYPE_STP,switch_id);
    object_reference_insert(stp);
    sai_object_meta_key_t meta_key_stp = { .objecttype = SAI_OBJECT_TYPE_STP, .objectkey = { .key = { .object_id = stp } } };
    std::string stp_key = sai_serialize_object_meta_key(meta_key_stp);
    ObjectAttrHash[stp_key] = { };

    SWSS_LOG_NOTICE("create");

    status = meta_sai_create_oid(SAI_OBJECT_TYPE_VLAN, &vlan1_id, switch_id, 1, &vlan1_att, &dummy_success_sai_create_oid);
    META_ASSERT_SUCCESS(status);

    sai_attribute_t vlan;
    vlan.id = SAI_VLAN_ATTR_VLAN_ID;
    vlan.value.u16 = 2;

    sai_object_id_t vlan_id;

    SWSS_LOG_NOTICE("correct");
    status = meta_sai_create_oid(SAI_OBJECT_TYPE_VLAN, &vlan_id, switch_id, 1, &vlan, &dummy_success_sai_create_oid);
    META_ASSERT_SUCCESS(status);

    SWSS_LOG_NOTICE("remove tests");

    SWSS_LOG_NOTICE("invalid vlan");
    status = meta_sai_remove_oid(SAI_OBJECT_TYPE_VLAN, SAI_NULL_OBJECT_ID, &dummy_success_sai_remove_oid);
    META_ASSERT_FAIL(status);

//    SWSS_LOG_NOTICE("default vlan");
//    status = meta_sai_remove_oid(SAI_OBJECT_TYPE_VLAN, vlan1_id, &dummy_success_sai_remove_oid);
//    META_ASSERT_FAIL(status);

    SWSS_LOG_NOTICE("remove is null");
    status = meta_sai_remove_oid(SAI_OBJECT_TYPE_VLAN, vlan_id, NULL);
    META_ASSERT_FAIL(status);

    SWSS_LOG_NOTICE("success");
    status = meta_sai_remove_oid(SAI_OBJECT_TYPE_VLAN, vlan_id, &dummy_success_sai_remove_oid);
    META_ASSERT_SUCCESS(status);

    SWSS_LOG_NOTICE("non existing");
    status = meta_sai_remove_oid(SAI_OBJECT_TYPE_VLAN, vlan_id, &dummy_success_sai_remove_oid);
    META_ASSERT_FAIL(status);
}

void test_vlan_set()
{
    SWSS_LOG_ENTER();

    clear_local();
    meta_init_db();

    sai_status_t status;

    sai_attribute_t attr;

    sai_attribute_t vlan1_att;
    vlan1_att.id = SAI_VLAN_ATTR_VLAN_ID;
    vlan1_att.value.u16 = 1;

    sai_object_id_t vlan1_id;
    sai_object_id_t switch_id = create_switch();

    // TODO we should use create
    sai_object_id_t stp = create_dummy_object_id(SAI_OBJECT_TYPE_STP,switch_id);
    object_reference_insert(stp);
    sai_object_meta_key_t meta_key_stp = { .objecttype = SAI_OBJECT_TYPE_STP, .objectkey = { .key = { .object_id = stp } } };
    std::string stp_key = sai_serialize_object_meta_key(meta_key_stp);
    ObjectAttrHash[stp_key] = { };

    SWSS_LOG_NOTICE("create");

    status = meta_sai_create_oid(SAI_OBJECT_TYPE_VLAN, &vlan1_id, switch_id, 1, &vlan1_att, &dummy_success_sai_create_oid);
    META_ASSERT_SUCCESS(status);

    sai_attribute_t vlan;
    vlan.id = SAI_VLAN_ATTR_VLAN_ID;
    vlan.value.u16 = 2;

    sai_object_id_t vlan_id;

    SWSS_LOG_NOTICE("correct");
    status = meta_sai_create_oid(SAI_OBJECT_TYPE_VLAN, &vlan_id, switch_id, 1, &vlan, &dummy_success_sai_create_oid);
    META_ASSERT_SUCCESS(status);

    SWSS_LOG_NOTICE("set tests");

    SWSS_LOG_NOTICE("invalid vlan");
    status = meta_sai_set_oid(SAI_OBJECT_TYPE_VLAN, SAI_NULL_OBJECT_ID, &vlan, &dummy_success_sai_set_oid);
    META_ASSERT_FAIL(status);

    SWSS_LOG_NOTICE("set is null");
    status = meta_sai_set_oid(SAI_OBJECT_TYPE_VLAN, vlan_id, &attr, NULL);
    META_ASSERT_FAIL(status);

    SWSS_LOG_NOTICE("attr is null");
    status = meta_sai_set_oid(SAI_OBJECT_TYPE_VLAN, vlan_id, NULL, &dummy_success_sai_set_oid);
    META_ASSERT_FAIL(status);

    attr.id = -1;

    SWSS_LOG_NOTICE("invalid attribute");
    status = meta_sai_set_oid(SAI_OBJECT_TYPE_VLAN, vlan_id, &attr, &dummy_success_sai_set_oid);
    META_ASSERT_FAIL(status);

    attr.id = SAI_VLAN_ATTR_MEMBER_LIST;

    SWSS_LOG_NOTICE("read only");
    status = meta_sai_set_oid(SAI_OBJECT_TYPE_VLAN, vlan_id, &attr, &dummy_success_sai_set_oid);
    META_ASSERT_FAIL(status);

    SWSS_LOG_NOTICE("max learned addresses");
    attr.id = SAI_VLAN_ATTR_MAX_LEARNED_ADDRESSES;
    attr.value.u32 = 1;
    status = meta_sai_set_oid(SAI_OBJECT_TYPE_VLAN, vlan_id, &attr, &dummy_success_sai_set_oid);
    META_ASSERT_SUCCESS(status);

    SWSS_LOG_NOTICE("null stp instance");
    attr.id = SAI_VLAN_ATTR_STP_INSTANCE;
    attr.value.oid = SAI_NULL_OBJECT_ID;
    status = meta_sai_set_oid(SAI_OBJECT_TYPE_VLAN, vlan_id, &attr, &dummy_success_sai_set_oid);
    META_ASSERT_FAIL(status);

    SWSS_LOG_NOTICE("wrong type on stp instance");
    attr.id = SAI_VLAN_ATTR_STP_INSTANCE;
    attr.value.oid = create_dummy_object_id(SAI_OBJECT_TYPE_HASH,switch_id);
    status = meta_sai_set_oid(SAI_OBJECT_TYPE_VLAN, vlan_id, &attr, &dummy_success_sai_set_oid);
    META_ASSERT_FAIL(status);

    SWSS_LOG_NOTICE("wrong type on stp instance");
    attr.id = SAI_VLAN_ATTR_STP_INSTANCE;
    attr.value.oid = create_dummy_object_id(SAI_OBJECT_TYPE_STP,switch_id);
    status = meta_sai_set_oid(SAI_OBJECT_TYPE_VLAN, vlan_id, &attr, &dummy_success_sai_set_oid);
    META_ASSERT_FAIL(status);

    SWSS_LOG_NOTICE("good stp oid");
    attr.id = SAI_VLAN_ATTR_STP_INSTANCE;
    attr.value.oid = stp;
    status = meta_sai_set_oid(SAI_OBJECT_TYPE_VLAN, vlan_id, &attr, &dummy_success_sai_set_oid);
    META_ASSERT_SUCCESS(status);

    SWSS_LOG_NOTICE("learn disable");
    attr.id = SAI_VLAN_ATTR_LEARN_DISABLE;
    attr.value.booldata = false;
    status = meta_sai_set_oid(SAI_OBJECT_TYPE_VLAN, vlan_id, &attr, &dummy_success_sai_set_oid);
    META_ASSERT_SUCCESS(status);

    SWSS_LOG_NOTICE("metadat");
    attr.id = SAI_VLAN_ATTR_META_DATA;
    attr.value.u32 = 1;
    status = meta_sai_set_oid(SAI_OBJECT_TYPE_VLAN, vlan_id, &attr, &dummy_success_sai_set_oid);
    META_ASSERT_SUCCESS(status);
}

void test_vlan_get()
{
    SWSS_LOG_ENTER();

    clear_local();
    meta_init_db();

    sai_object_id_t switch_id = create_switch();
    sai_status_t status;

    sai_attribute_t attr;

    sai_attribute_t vlan1_att;
    vlan1_att.id = SAI_VLAN_ATTR_VLAN_ID;
    vlan1_att.value.u16 = 1;

    sai_object_id_t vlan1_id;

    // TODO we should use create
    sai_object_id_t stp = create_dummy_object_id(SAI_OBJECT_TYPE_STP,switch_id);
    object_reference_insert(stp);
    sai_object_meta_key_t meta_key_stp = { .objecttype = SAI_OBJECT_TYPE_STP, .objectkey = { .key = { .object_id = stp } } };
    std::string stp_key = sai_serialize_object_meta_key(meta_key_stp);
    ObjectAttrHash[stp_key] = { };

    SWSS_LOG_NOTICE("create");

    status = meta_sai_create_oid(SAI_OBJECT_TYPE_VLAN, &vlan1_id, switch_id, 1, &vlan1_att, &dummy_success_sai_create_oid);
    META_ASSERT_SUCCESS(status);

    sai_attribute_t vlan;
    vlan.id = SAI_VLAN_ATTR_VLAN_ID;
    vlan.value.u16 = 2;

    sai_object_id_t vlan_id;

    SWSS_LOG_NOTICE("correct");
    status = meta_sai_create_oid(SAI_OBJECT_TYPE_VLAN, &vlan_id, switch_id, 1, &vlan, &dummy_success_sai_create_oid);
    META_ASSERT_SUCCESS(status);

    SWSS_LOG_NOTICE("get tests");

    attr.id = SAI_VLAN_ATTR_MAX_LEARNED_ADDRESSES;

    SWSS_LOG_NOTICE("invalid vlan");
    status = meta_sai_get_oid(SAI_OBJECT_TYPE_VLAN, 0, 1, &attr, &dummy_success_sai_get_oid);
    META_ASSERT_FAIL(status);

//    SWSS_LOG_NOTICE("invalid vlan");
//    status = meta_sai_get_oid(SAI_OBJECT_TYPE_VLAN, 3, 1, &attr, &dummy_success_sai_get_oid);
//    META_ASSERT_FAIL(status);

    SWSS_LOG_NOTICE("get is null");
    status = meta_sai_get_oid(SAI_OBJECT_TYPE_VLAN, vlan_id, 1, &attr, NULL);
    META_ASSERT_FAIL(status);

    SWSS_LOG_NOTICE("attr is null");
    status = meta_sai_get_oid(SAI_OBJECT_TYPE_VLAN, vlan_id, 1, NULL, &dummy_success_sai_get_oid);
    META_ASSERT_FAIL(status);

    SWSS_LOG_NOTICE("zero attributes");
    status = meta_sai_get_oid(SAI_OBJECT_TYPE_VLAN, vlan_id, 0, &attr, &dummy_success_sai_get_oid);
    META_ASSERT_FAIL(status);

    attr.id = -1;

    SWSS_LOG_NOTICE("invalid attribute");
    status = meta_sai_get_oid(SAI_OBJECT_TYPE_VLAN, vlan_id, 1, &attr, &dummy_success_sai_get_oid);
    META_ASSERT_FAIL(status);

    attr.id = SAI_VLAN_ATTR_MEMBER_LIST;
    attr.value.objlist.count = 1;
    attr.value.objlist.list = NULL;

    SWSS_LOG_NOTICE("read only null list");
    status = meta_sai_get_oid(SAI_OBJECT_TYPE_VLAN, vlan_id, 1, &attr, &dummy_success_sai_get_oid);
    META_ASSERT_FAIL(status);

    sai_object_id_t list[5] = { };

    list[0] = SAI_NULL_OBJECT_ID;
    list[1] = create_dummy_object_id(SAI_OBJECT_TYPE_HASH,switch_id);
    list[2] = create_dummy_object_id(SAI_OBJECT_TYPE_VLAN_MEMBER,switch_id);
    list[3] = stp;

    attr.value.objlist.count = 0;
    attr.value.objlist.list = list;

    SWSS_LOG_NOTICE("readonly 0 count and not null");
    status = meta_sai_get_oid(SAI_OBJECT_TYPE_VLAN, vlan_id, 1, &attr, &dummy_success_sai_get_oid);
//    META_ASSERT_SUCCESS(status);

    attr.value.objlist.count = 5;

    SWSS_LOG_NOTICE("readonly count and not null");
    status = meta_sai_get_oid(SAI_OBJECT_TYPE_VLAN, vlan_id, 1, &attr, &dummy_success_sai_get_oid);
    META_ASSERT_SUCCESS(status);

    attr.value.objlist.count = 0;
    attr.value.objlist.list = NULL;

    SWSS_LOG_NOTICE("readonly count 0 and null");
    status = meta_sai_get_oid(SAI_OBJECT_TYPE_VLAN, vlan_id, 1, &attr, &dummy_success_sai_get_oid);
    META_ASSERT_SUCCESS(status);

    SWSS_LOG_NOTICE("max learned addresses");
    attr.id = SAI_VLAN_ATTR_MAX_LEARNED_ADDRESSES;
    status = meta_sai_get_oid(SAI_OBJECT_TYPE_VLAN, vlan_id, 1, &attr, &dummy_success_sai_get_oid);
    META_ASSERT_SUCCESS(status);

    SWSS_LOG_NOTICE("stp instance");
    attr.id = SAI_VLAN_ATTR_STP_INSTANCE;
    status = meta_sai_get_oid(SAI_OBJECT_TYPE_VLAN, vlan_id, 1, &attr, &dummy_success_sai_get_oid);
    META_ASSERT_SUCCESS(status);

    SWSS_LOG_NOTICE("learn disable");
    attr.id = SAI_VLAN_ATTR_LEARN_DISABLE;
    status = meta_sai_get_oid(SAI_OBJECT_TYPE_VLAN, vlan_id, 1, &attr, &dummy_success_sai_get_oid);
    META_ASSERT_SUCCESS(status);

    SWSS_LOG_NOTICE("metadata");
    attr.id = SAI_VLAN_ATTR_META_DATA;
    status = meta_sai_get_oid(SAI_OBJECT_TYPE_VLAN, vlan_id, 1, &attr, &dummy_success_sai_get_oid);
    META_ASSERT_SUCCESS(status);
}

void test_vlan_flow()
{
    SWSS_LOG_ENTER();

    clear_local();
    meta_init_db();

    sai_attribute_t attr;

    sai_status_t    status;

    sai_object_id_t vlan_id;

    sai_attribute_t at;
    at.id = SAI_VLAN_ATTR_VLAN_ID;
    at.value.u16 = 2;
    sai_object_id_t switch_id = create_switch();

    status = meta_sai_create_oid(SAI_OBJECT_TYPE_VLAN, &vlan_id, switch_id, 1, &at, &dummy_success_sai_create_oid);
    META_ASSERT_SUCCESS(status);

    // TODO we should use create
    sai_object_id_t stp = create_dummy_object_id(SAI_OBJECT_TYPE_STP,switch_id);
    object_reference_insert(stp);
    sai_object_meta_key_t meta_key_stp = { .objecttype = SAI_OBJECT_TYPE_STP, .objectkey = { .key = { .object_id = stp } } };
    std::string stp_key = sai_serialize_object_meta_key(meta_key_stp);
    ObjectAttrHash[stp_key] = { };

    SWSS_LOG_NOTICE("create");

    attr.id = SAI_VLAN_ATTR_VLAN_ID;
    at.value.u16 = 2;

    SWSS_LOG_NOTICE("correct");
    status = meta_sai_create_oid(SAI_OBJECT_TYPE_VLAN, &vlan_id, switch_id, 1, &attr, &dummy_success_sai_create_oid);
    META_ASSERT_SUCCESS(status);

    SWSS_LOG_NOTICE("existing");
    status = meta_sai_create_oid(SAI_OBJECT_TYPE_VLAN, &vlan_id, switch_id, 1, &attr, &dummy_success_sai_create_oid);
    META_ASSERT_FAIL(status);

    SWSS_LOG_NOTICE("set");

    SWSS_LOG_NOTICE("max learned addresses");
    attr.id = SAI_VLAN_ATTR_MAX_LEARNED_ADDRESSES;
    attr.value.u32 = 1;
    status = meta_sai_set_oid(SAI_OBJECT_TYPE_VLAN, vlan_id, &attr, &dummy_success_sai_set_oid);
    META_ASSERT_SUCCESS(status);

    SWSS_LOG_NOTICE("good stp oid");
    attr.id = SAI_VLAN_ATTR_STP_INSTANCE;
    attr.value.oid = stp;
    status = meta_sai_set_oid(SAI_OBJECT_TYPE_VLAN, vlan_id, &attr, &dummy_success_sai_set_oid);
    META_ASSERT_SUCCESS(status);

    SWSS_LOG_NOTICE("learn disable");
    attr.id = SAI_VLAN_ATTR_LEARN_DISABLE;
    attr.value.booldata = false;
    status = meta_sai_set_oid(SAI_OBJECT_TYPE_VLAN, vlan_id, &attr, &dummy_success_sai_set_oid);
    META_ASSERT_SUCCESS(status);

    SWSS_LOG_NOTICE("metadata");
    attr.id = SAI_VLAN_ATTR_META_DATA;
    attr.value.u32 = 1;
    status = meta_sai_set_oid(SAI_OBJECT_TYPE_VLAN, vlan_id, &attr, &dummy_success_sai_set_oid);
    META_ASSERT_SUCCESS(status);

    SWSS_LOG_NOTICE("get");

    sai_object_id_t list[5] = { };

    list[0] = SAI_NULL_OBJECT_ID;
    list[1] = create_dummy_object_id(SAI_OBJECT_TYPE_HASH,switch_id);
    list[2] = create_dummy_object_id(SAI_OBJECT_TYPE_VLAN_MEMBER,switch_id);
    list[3] = stp;

    attr.value.objlist.count = 0;
    attr.value.objlist.list = list;

    SWSS_LOG_NOTICE("readonly 0 count and not null");
    status = meta_sai_get_oid(SAI_OBJECT_TYPE_VLAN, vlan_id, 1, &attr, &dummy_success_sai_get_oid);
    META_ASSERT_SUCCESS(status);

    attr.value.objlist.count = 5;

    SWSS_LOG_NOTICE("readonly count and not null");
    status = meta_sai_get_oid(SAI_OBJECT_TYPE_VLAN, vlan_id, 1, &attr, &dummy_success_sai_get_oid);
    META_ASSERT_SUCCESS(status);

    attr.value.objlist.count = 0;
    attr.value.objlist.list = NULL;

    SWSS_LOG_NOTICE("readonly count 0 and null");
    status = meta_sai_get_oid(SAI_OBJECT_TYPE_VLAN, vlan_id, 1, &attr, &dummy_success_sai_get_oid);
    META_ASSERT_SUCCESS(status);

    SWSS_LOG_NOTICE("max learned addresses");
    attr.id = SAI_VLAN_ATTR_MAX_LEARNED_ADDRESSES;
    status = meta_sai_get_oid(SAI_OBJECT_TYPE_VLAN, vlan_id, 1, &attr, &dummy_success_sai_get_oid);
    META_ASSERT_SUCCESS(status);

    SWSS_LOG_NOTICE("stp instance");
    attr.id = SAI_VLAN_ATTR_STP_INSTANCE;
    status = meta_sai_get_oid(SAI_OBJECT_TYPE_VLAN, vlan_id, 1, &attr, &dummy_success_sai_get_oid);
    META_ASSERT_SUCCESS(status);

    SWSS_LOG_NOTICE("learn disable");
    attr.id = SAI_VLAN_ATTR_LEARN_DISABLE;
    status = meta_sai_get_oid(SAI_OBJECT_TYPE_VLAN, vlan_id, 1, &attr, &dummy_success_sai_get_oid);
    META_ASSERT_SUCCESS(status);

    SWSS_LOG_NOTICE("metadata");
    attr.id = SAI_VLAN_ATTR_META_DATA;
    status = meta_sai_get_oid(SAI_OBJECT_TYPE_VLAN, vlan_id, 1, &attr, &dummy_success_sai_get_oid);
    META_ASSERT_SUCCESS(status);

    SWSS_LOG_NOTICE("remove");

    SWSS_LOG_NOTICE("success");
    status = meta_sai_remove_oid(SAI_OBJECT_TYPE_VLAN, vlan_id, &dummy_success_sai_remove_oid);
    META_ASSERT_SUCCESS(status);

    SWSS_LOG_NOTICE("non existing");
    status = meta_sai_remove_oid(SAI_OBJECT_TYPE_VLAN, vlan_id, &dummy_success_sai_remove_oid);
    META_ASSERT_FAIL(status);

    SWSS_LOG_NOTICE("learn disable");
    attr.id = SAI_VLAN_ATTR_LEARN_DISABLE;
    status = meta_sai_get_oid(SAI_OBJECT_TYPE_VLAN, vlan_id, 1, &attr, &dummy_success_sai_get_oid);
    META_ASSERT_FAIL(status);
}

// ROUTE TESTS

void test_route_entry_create()
{
    SWSS_LOG_ENTER();

    clear_local();
    meta_init_db();

    sai_status_t    status;
    sai_attribute_t attr;

    sai_object_id_t switch_id = create_switch();
    sai_route_entry_t route_entry;

    // TODO we should use create
    sai_object_id_t vr = create_dummy_object_id(SAI_OBJECT_TYPE_VIRTUAL_ROUTER,switch_id);
    object_reference_insert(vr);
    sai_object_meta_key_t meta_key_vr = { .objecttype = SAI_OBJECT_TYPE_VIRTUAL_ROUTER, .objectkey = { .key = { .object_id = vr } } };
    std::string vr_key = sai_serialize_object_meta_key(meta_key_vr);
    ObjectAttrHash[vr_key] = { };

    sai_object_id_t hop = create_dummy_object_id(SAI_OBJECT_TYPE_NEXT_HOP,switch_id);
    object_reference_insert(hop);
    sai_object_meta_key_t meta_key_hop = { .objecttype = SAI_OBJECT_TYPE_NEXT_HOP, .objectkey = { .key = { .object_id = hop } } };
    std::string hop_key = sai_serialize_object_meta_key(meta_key_hop);
    ObjectAttrHash[hop_key] = { };

    route_entry.destination.addr_family = SAI_IP_ADDR_FAMILY_IPV4;
    route_entry.destination.addr.ip4 = htonl(0x0a00000f);
    route_entry.destination.mask.ip4 = htonl(0xffffff00);
    route_entry.vr_id = vr;
    route_entry.switch_id = switch_id;

    SWSS_LOG_NOTICE("create tests");

    // commented out as there is no mandatory attribute
    // SWSS_LOG_NOTICE("zero attribute count (but there are mandatory attributes)");
    // attr.id = SAI_ROUTE_ENTRY_ATTR_NEXT_HOP_ID;
    // status = meta_sai_create_route_entry(&route_entry, 0, &attr, &dummy_success_sai_create_route_entry);
    // META_ASSERT_FAIL(status);

    SWSS_LOG_NOTICE("attr is null");
    status = meta_sai_create_route_entry(&route_entry, 1, NULL, &dummy_success_sai_create_route_entry);
    META_ASSERT_FAIL(status);

    SWSS_LOG_NOTICE("route entry is null");
    status = meta_sai_create_route_entry(NULL, 1, &attr, &dummy_success_sai_create_route_entry);
    META_ASSERT_FAIL(status);

    sai_attribute_t list[3] = { };

    sai_attribute_t &attr1 = list[0];
    sai_attribute_t &attr2 = list[1];
    sai_attribute_t &attr3 = list[2];

    attr1.id = SAI_ROUTE_ENTRY_ATTR_NEXT_HOP_ID;
    attr1.value.oid = hop;

    attr2.id = SAI_ROUTE_ENTRY_ATTR_PACKET_ACTION;
    attr2.value.s32 = SAI_PACKET_ACTION_FORWARD;

    attr3.id = -1;

    SWSS_LOG_NOTICE("create function is null");
    status = meta_sai_create_route_entry(&route_entry, 2, list, NULL);
    META_ASSERT_FAIL(status);

    SWSS_LOG_NOTICE("invalid attribute id");
    status = meta_sai_create_route_entry(&route_entry, 3, list, &dummy_success_sai_create_route_entry);
    META_ASSERT_FAIL(status);

    attr2.value.s32 = SAI_PACKET_ACTION_FORWARD + 0x100;
    SWSS_LOG_NOTICE("invalid attribute value on enum");
    status = meta_sai_create_route_entry(&route_entry, 2, list, &dummy_success_sai_create_route_entry);
    META_ASSERT_FAIL(status);

    attr2.value.s32 = SAI_PACKET_ACTION_FORWARD;

    sai_attribute_t list2[4] = { attr1, attr2, attr2 };

    SWSS_LOG_NOTICE("repeated attribute id");
    status = meta_sai_create_route_entry(&route_entry, 3, list2, &dummy_success_sai_create_route_entry);
    META_ASSERT_FAIL(status);

    SWSS_LOG_NOTICE("wrong obejct type");
    attr1.value.oid = create_dummy_object_id(SAI_OBJECT_TYPE_HASH,switch_id);
    status = meta_sai_create_route_entry(&route_entry, 2, list, &dummy_success_sai_create_route_entry);
    META_ASSERT_FAIL(status);

    SWSS_LOG_NOTICE("non existing object");
    attr1.value.oid = create_dummy_object_id(SAI_OBJECT_TYPE_NEXT_HOP,switch_id);
    status = meta_sai_create_route_entry(&route_entry, 2, list, &dummy_success_sai_create_route_entry);
    META_ASSERT_FAIL(status);

    int fam = 10;
    attr1.value.oid = hop;
    route_entry.destination.addr_family = (sai_ip_addr_family_t)fam;

    SWSS_LOG_NOTICE("wrong address family");
    status = meta_sai_create_route_entry(&route_entry, 2, list, &dummy_success_sai_create_route_entry);
    META_ASSERT_FAIL(status);

    SWSS_LOG_NOTICE("correct ipv4");
    route_entry.destination.addr_family = SAI_IP_ADDR_FAMILY_IPV4;
    status = meta_sai_create_route_entry(&route_entry, 2, list, &dummy_success_sai_create_route_entry);
    META_ASSERT_SUCCESS(status);

    route_entry.destination.addr_family = SAI_IP_ADDR_FAMILY_IPV6;
    sai_ip6_t ip62 = {0x00, 0x11, 0x22, 0x33,0x44, 0x55, 0x66,0x77, 0x88, 0x99, 0xaa, 0xbb,0xcc,0xdd,0xee,0x99};
    memcpy(route_entry.destination.addr.ip6, ip62, 16);

    sai_ip6_t ip6mask2 = {0xff, 0xff, 0xff, 0xff,0xff, 0xff, 0xff,0xff, 0xff, 0xff, 0xff, 0xff,0xff,0xff,0xf7,0x00};
    memcpy(route_entry.destination.mask.ip6, ip6mask2, 16);

    SWSS_LOG_NOTICE("invalid mask");
    status = meta_sai_create_route_entry(&route_entry, 2, list, &dummy_success_sai_create_route_entry);
    META_ASSERT_FAIL(status);

    route_entry.destination.addr_family = SAI_IP_ADDR_FAMILY_IPV6;
    sai_ip6_t ip6 = {0x00, 0x11, 0x22, 0x33,0x44, 0x55, 0x66,0x77, 0x88, 0x99, 0xaa, 0xbb,0xcc,0xdd,0xee,0xff};
    memcpy(route_entry.destination.addr.ip6, ip6, 16);

    sai_ip6_t ip6mask = {0xff, 0xff, 0xff, 0xff,0xff, 0xff, 0xff,0xff, 0xff, 0xff, 0xff, 0xff,0xff,0xff,0xff,0x00};
    memcpy(route_entry.destination.mask.ip6, ip6mask, 16);

    SWSS_LOG_NOTICE("correct ipv6");
    status = meta_sai_create_route_entry(&route_entry, 2, list, &dummy_success_sai_create_route_entry);
    META_ASSERT_SUCCESS(status);

    SWSS_LOG_NOTICE("already exists");
    status = meta_sai_create_route_entry(&route_entry, 2, list, &dummy_success_sai_create_route_entry);
    META_ASSERT_FAIL(status);
}

void test_route_entry_remove()
{
    SWSS_LOG_ENTER();

    clear_local();
    meta_init_db();

    sai_status_t    status;
    sai_object_id_t switch_id = create_switch();

    sai_route_entry_t route_entry;

    // TODO we should use create
    sai_object_id_t vr = create_dummy_object_id(SAI_OBJECT_TYPE_VIRTUAL_ROUTER,switch_id);
    object_reference_insert(vr);
    sai_object_meta_key_t meta_key_vr = { .objecttype = SAI_OBJECT_TYPE_VIRTUAL_ROUTER, .objectkey = { .key = { .object_id = vr } } };
    std::string vr_key = sai_serialize_object_meta_key(meta_key_vr);
    ObjectAttrHash[vr_key] = { };

    sai_object_id_t hop = create_dummy_object_id(SAI_OBJECT_TYPE_NEXT_HOP,switch_id);
    object_reference_insert(hop);
    sai_object_meta_key_t meta_key_hop = { .objecttype = SAI_OBJECT_TYPE_NEXT_HOP, .objectkey = { .key = { .object_id = hop } } };
    std::string hop_key = sai_serialize_object_meta_key(meta_key_hop);
    ObjectAttrHash[hop_key] = { };

    route_entry.destination.addr_family = SAI_IP_ADDR_FAMILY_IPV4;
    route_entry.destination.addr.ip4 = htonl(0x0a00000f);
    route_entry.destination.mask.ip4 = htonl(0xffffff00);
    route_entry.vr_id = vr;
    route_entry.switch_id = switch_id;

    SWSS_LOG_NOTICE("create tests");

    sai_attribute_t list[3] = { };

    sai_attribute_t &attr1 = list[0];
    sai_attribute_t &attr2 = list[1];

    attr1.id = SAI_ROUTE_ENTRY_ATTR_NEXT_HOP_ID;
    attr1.value.oid = hop;

    attr2.id = SAI_ROUTE_ENTRY_ATTR_PACKET_ACTION;
    attr2.value.s32 = SAI_PACKET_ACTION_FORWARD;

    SWSS_LOG_NOTICE("correct ipv4");
    route_entry.destination.addr_family = SAI_IP_ADDR_FAMILY_IPV4;
    status = meta_sai_create_route_entry(&route_entry, 2, list, &dummy_success_sai_create_route_entry);
    META_ASSERT_SUCCESS(status);

    route_entry.destination.addr_family = SAI_IP_ADDR_FAMILY_IPV6;
    sai_ip6_t ip6 = {0x00, 0x11, 0x22, 0x33,0x44, 0x55, 0x66,0x77, 0x88, 0x99, 0xaa, 0xbb,0xcc,0xdd,0xee,0xff};
    memcpy(route_entry.destination.addr.ip6, ip6, 16);

    sai_ip6_t ip6mask = {0xff, 0xff, 0xff, 0xff,0xff, 0xff, 0xff,0xff, 0xff, 0xff, 0xff, 0xff,0xff,0xff,0xff,0x00};
    memcpy(route_entry.destination.mask.ip6, ip6mask, 16);

    SWSS_LOG_NOTICE("correct ipv6");
    status = meta_sai_create_route_entry(&route_entry, 2, list, &dummy_success_sai_create_route_entry);
    META_ASSERT_SUCCESS(status);

    SWSS_LOG_NOTICE("remove tests");

    SWSS_LOG_NOTICE("route_entry is null");
    status = meta_sai_remove_route_entry(NULL, &dummy_success_sai_remove_route_entry);
    META_ASSERT_FAIL(status);

    route_entry.vr_id = SAI_NULL_OBJECT_ID;

    SWSS_LOG_NOTICE("invalid object id null");
    status = meta_sai_remove_route_entry(&route_entry, &dummy_success_sai_remove_route_entry);
    META_ASSERT_FAIL(status);

    route_entry.vr_id = create_dummy_object_id(SAI_OBJECT_TYPE_HASH,switch_id);

    SWSS_LOG_NOTICE("invalid object id hash");
    status = meta_sai_remove_route_entry(&route_entry, &dummy_success_sai_remove_route_entry);
    META_ASSERT_FAIL(status);

    route_entry.vr_id = create_dummy_object_id(SAI_OBJECT_TYPE_VIRTUAL_ROUTER,switch_id);

    SWSS_LOG_NOTICE("invalid object id router");
    status = meta_sai_remove_route_entry(&route_entry, &dummy_success_sai_remove_route_entry);
    META_ASSERT_FAIL(status);

    route_entry.vr_id = vr;

    sai_object_meta_key_t meta = { .objecttype = SAI_OBJECT_TYPE_ROUTE_ENTRY, .objectkey = { .key = { .route_entry = route_entry } } };

    std::string key = sai_serialize_object_meta_key(meta);

    META_ASSERT_TRUE(object_exists(key));

    SWSS_LOG_NOTICE("success");
    status = meta_sai_remove_route_entry(&route_entry, &dummy_success_sai_remove_route_entry);
    META_ASSERT_SUCCESS(status);

    META_ASSERT_TRUE(!object_exists(key));

    route_entry.destination.addr_family = SAI_IP_ADDR_FAMILY_IPV4;
    route_entry.destination.addr.ip4 = htonl(0x0a00000f);
    route_entry.destination.mask.ip4 = htonl(0xffffff00);

    SWSS_LOG_NOTICE("success");
    status = meta_sai_remove_route_entry(&route_entry, &dummy_success_sai_remove_route_entry);
    META_ASSERT_SUCCESS(status);
}

void test_route_entry_set()
{
    SWSS_LOG_ENTER();

    clear_local();
    meta_init_db();

    sai_status_t    status;
    sai_attribute_t attr;
    sai_object_id_t switch_id = create_switch();

    sai_route_entry_t route_entry;

    // TODO we should use create
    sai_object_id_t vr = create_dummy_object_id(SAI_OBJECT_TYPE_VIRTUAL_ROUTER,switch_id);
    object_reference_insert(vr);
    sai_object_meta_key_t meta_key_vr = { .objecttype = SAI_OBJECT_TYPE_VIRTUAL_ROUTER, .objectkey = { .key = { .object_id = vr } } };
    std::string vr_key = sai_serialize_object_meta_key(meta_key_vr);
    ObjectAttrHash[vr_key] = { };

    sai_object_id_t hop = create_dummy_object_id(SAI_OBJECT_TYPE_NEXT_HOP,switch_id);
    object_reference_insert(hop);
    sai_object_meta_key_t meta_key_hop = { .objecttype = SAI_OBJECT_TYPE_NEXT_HOP, .objectkey = { .key = { .object_id = hop } } };
    std::string hop_key = sai_serialize_object_meta_key(meta_key_hop);
    ObjectAttrHash[hop_key] = { };

    route_entry.destination.addr_family = SAI_IP_ADDR_FAMILY_IPV4;
    route_entry.destination.addr.ip4 = htonl(0x0a00000f);
    route_entry.destination.mask.ip4 = htonl(0xffffff00);
    route_entry.vr_id = vr;
    route_entry.switch_id = switch_id;

    SWSS_LOG_NOTICE("create tests");

    sai_attribute_t list[3] = { };

    sai_attribute_t &attr1 = list[0];
    sai_attribute_t &attr2 = list[1];

    attr1.id = SAI_ROUTE_ENTRY_ATTR_NEXT_HOP_ID;
    attr1.value.oid = hop;

    attr2.id = SAI_ROUTE_ENTRY_ATTR_PACKET_ACTION;
    attr2.value.s32 = SAI_PACKET_ACTION_FORWARD;

    SWSS_LOG_NOTICE("correct ipv4");
    route_entry.destination.addr_family = SAI_IP_ADDR_FAMILY_IPV4;
    status = meta_sai_create_route_entry(&route_entry, 2, list, &dummy_success_sai_create_route_entry);
    META_ASSERT_SUCCESS(status);

    route_entry.destination.addr_family = SAI_IP_ADDR_FAMILY_IPV6;
    sai_ip6_t ip6 = {0x00, 0x11, 0x22, 0x33,0x44, 0x55, 0x66,0x77, 0x88, 0x99, 0xaa, 0xbb,0xcc,0xdd,0xee,0xff};
    memcpy(route_entry.destination.addr.ip6, ip6, 16);

    sai_ip6_t ip6mask = {0xff, 0xff, 0xff, 0xff,0xff, 0xff, 0xff,0xff, 0xff, 0xff, 0xff, 0xff,0xff,0xff,0xff,0x00};
    memcpy(route_entry.destination.mask.ip6, ip6mask, 16);

    SWSS_LOG_NOTICE("correct ipv6");
    status = meta_sai_create_route_entry(&route_entry, 2, list, &dummy_success_sai_create_route_entry);
    META_ASSERT_SUCCESS(status);

    SWSS_LOG_NOTICE("set tests");

    SWSS_LOG_NOTICE("attr is null");
    status = meta_sai_set_route_entry(&route_entry, NULL, &dummy_success_sai_set_route_entry);
    META_ASSERT_FAIL(status);

    SWSS_LOG_NOTICE("route entry is null");
    status = meta_sai_set_route_entry(NULL, &attr, &dummy_success_sai_set_route_entry);
    META_ASSERT_FAIL(status);

    SWSS_LOG_NOTICE("setting invalid attrib id");
    attr.id = -1;
    status = meta_sai_set_route_entry(&route_entry, &attr, &dummy_success_sai_set_route_entry);
    META_ASSERT_FAIL(status);

    SWSS_LOG_NOTICE("value outside range");
    attr.id = SAI_ROUTE_ENTRY_ATTR_PACKET_ACTION;
    attr.value.s32 = 0x100;
    status = meta_sai_set_route_entry(&route_entry, &attr, &dummy_success_sai_set_route_entry);
    META_ASSERT_FAIL(status);

    SWSS_LOG_NOTICE("correct packet action");
    attr.id = SAI_ROUTE_ENTRY_ATTR_PACKET_ACTION;
    attr.value.s32 = SAI_PACKET_ACTION_DROP;
    status = meta_sai_set_route_entry(&route_entry, &attr, &dummy_success_sai_set_route_entry);
    META_ASSERT_SUCCESS(status);

    SWSS_LOG_NOTICE("correct next hop");
    attr.id = SAI_ROUTE_ENTRY_ATTR_NEXT_HOP_ID;
    attr.value.oid = hop;
    status = meta_sai_set_route_entry(&route_entry, &attr, &dummy_success_sai_set_route_entry);
    META_ASSERT_SUCCESS(status);

    SWSS_LOG_NOTICE("correct metadata");
    attr.id = SAI_ROUTE_ENTRY_ATTR_META_DATA;
    attr.value.u32 = 0x12345678;
    status = meta_sai_set_route_entry(&route_entry, &attr, &dummy_success_sai_set_route_entry);
    META_ASSERT_SUCCESS(status);
}

void test_route_entry_get()
{
    SWSS_LOG_ENTER();

    clear_local();
    meta_init_db();

    sai_status_t    status;
    sai_attribute_t attr;
    sai_object_id_t switch_id = create_switch();

    sai_route_entry_t route_entry;

    // TODO we should use create
    sai_object_id_t vr = create_dummy_object_id(SAI_OBJECT_TYPE_VIRTUAL_ROUTER,switch_id);
    object_reference_insert(vr);
    sai_object_meta_key_t meta_key_vr = { .objecttype = SAI_OBJECT_TYPE_VIRTUAL_ROUTER, .objectkey = { .key = { .object_id = vr } } };
    std::string vr_key = sai_serialize_object_meta_key(meta_key_vr);
    ObjectAttrHash[vr_key] = { };

    sai_object_id_t hop = create_dummy_object_id(SAI_OBJECT_TYPE_NEXT_HOP,switch_id);
    object_reference_insert(hop);
    sai_object_meta_key_t meta_key_hop = { .objecttype = SAI_OBJECT_TYPE_NEXT_HOP, .objectkey = { .key = { .object_id = hop } } };
    std::string hop_key = sai_serialize_object_meta_key(meta_key_hop);
    ObjectAttrHash[hop_key] = { };

    route_entry.destination.addr_family = SAI_IP_ADDR_FAMILY_IPV4;
    route_entry.destination.addr.ip4 = htonl(0x0a00000f);
    route_entry.destination.mask.ip4 = htonl(0xffffff00);
    route_entry.vr_id = vr;
    route_entry.switch_id = switch_id;

    SWSS_LOG_NOTICE("create tests");

    sai_attribute_t list[3] = { };

    sai_attribute_t &attr1 = list[0];
    sai_attribute_t &attr2 = list[1];

    attr1.id = SAI_ROUTE_ENTRY_ATTR_NEXT_HOP_ID;
    attr1.value.oid = hop;

    attr2.id = SAI_ROUTE_ENTRY_ATTR_PACKET_ACTION;
    attr2.value.s32 = SAI_PACKET_ACTION_FORWARD;

    SWSS_LOG_NOTICE("correct ipv4");
    route_entry.destination.addr_family = SAI_IP_ADDR_FAMILY_IPV4;
    status = meta_sai_create_route_entry(&route_entry, 2, list, &dummy_success_sai_create_route_entry);
    META_ASSERT_SUCCESS(status);

    route_entry.destination.addr_family = SAI_IP_ADDR_FAMILY_IPV6;
    sai_ip6_t ip6 = {0x00, 0x11, 0x22, 0x33,0x44, 0x55, 0x66,0x77, 0x88, 0x99, 0xaa, 0xbb,0xcc,0xdd,0xee,0xff};
    memcpy(route_entry.destination.addr.ip6, ip6, 16);

    sai_ip6_t ip6mask = {0xff, 0xff, 0xff, 0xff,0xff, 0xff, 0xff,0xff, 0xff, 0xff, 0xff, 0xff,0xff,0xff,0xff,0x00};
    memcpy(route_entry.destination.mask.ip6, ip6mask, 16);

    SWSS_LOG_NOTICE("correct ipv6");
    status = meta_sai_create_route_entry(&route_entry, 2, list, &dummy_success_sai_create_route_entry);
    META_ASSERT_SUCCESS(status);

    SWSS_LOG_NOTICE("zero attribute count");
    attr.id = SAI_ROUTE_ENTRY_ATTR_PACKET_ACTION;
    status = meta_sai_get_route_entry(&route_entry, 0, &attr, &dummy_success_sai_get_route_entry);
    META_ASSERT_FAIL(status);

    SWSS_LOG_NOTICE("attr is null");
    status = meta_sai_get_route_entry(&route_entry, 1, NULL, &dummy_success_sai_get_route_entry);
    META_ASSERT_FAIL(status);

    SWSS_LOG_NOTICE("route entry is null");
    status = meta_sai_get_route_entry(NULL, 1, &attr, &dummy_success_sai_get_route_entry);
    META_ASSERT_FAIL(status);

    SWSS_LOG_NOTICE("get function is null");
    status = meta_sai_get_route_entry(&route_entry, 1, &attr, NULL);
    META_ASSERT_FAIL(status);

    SWSS_LOG_NOTICE("attr id out of range");
    attr.id = -1;
    status = meta_sai_get_route_entry(&route_entry, 1, &attr, &dummy_success_sai_get_route_entry);
    META_ASSERT_FAIL(status);

    SWSS_LOG_NOTICE("correct packet action");
    attr.id = SAI_ROUTE_ENTRY_ATTR_PACKET_ACTION;
    status = meta_sai_get_route_entry(&route_entry, 1, &attr, &dummy_success_sai_get_route_entry);
    META_ASSERT_SUCCESS(status);

    SWSS_LOG_NOTICE("correct next hop");
    attr.id = SAI_ROUTE_ENTRY_ATTR_NEXT_HOP_ID;
    status = meta_sai_get_route_entry(&route_entry, 1, &attr, &dummy_success_sai_get_route_entry);
    META_ASSERT_SUCCESS(status);

    SWSS_LOG_NOTICE("correct meta");
    attr.id = SAI_ROUTE_ENTRY_ATTR_META_DATA;
    status = meta_sai_get_route_entry(&route_entry, 1, &attr, &dummy_success_sai_get_route_entry);
    META_ASSERT_SUCCESS(status);
}

void test_route_entry_flow()
{
    SWSS_LOG_ENTER();

    clear_local();
    meta_init_db();

    sai_status_t    status;
    sai_attribute_t attr;

    sai_route_entry_t route_entry;
    sai_object_id_t switch_id = create_switch();

    // TODO we should use create
    sai_object_id_t vr = create_dummy_object_id(SAI_OBJECT_TYPE_VIRTUAL_ROUTER,switch_id);
    object_reference_insert(vr);
    sai_object_meta_key_t meta_key_vr = { .objecttype = SAI_OBJECT_TYPE_VIRTUAL_ROUTER, .objectkey = { .key = { .object_id = vr } } };
    std::string vr_key = sai_serialize_object_meta_key(meta_key_vr);
    ObjectAttrHash[vr_key] = { };

    sai_object_id_t hop = create_dummy_object_id(SAI_OBJECT_TYPE_NEXT_HOP,switch_id);
    object_reference_insert(hop);
    sai_object_meta_key_t meta_key_hop = { .objecttype = SAI_OBJECT_TYPE_NEXT_HOP, .objectkey = { .key = { .object_id = hop } } };
    std::string hop_key = sai_serialize_object_meta_key(meta_key_hop);
    ObjectAttrHash[hop_key] = { };

    route_entry.destination.addr_family = SAI_IP_ADDR_FAMILY_IPV4;
    route_entry.destination.addr.ip4 = htonl(0x0a00000f);
    route_entry.destination.mask.ip4 = htonl(0xffffff00);
    route_entry.vr_id = vr;
    route_entry.switch_id = switch_id;

    SWSS_LOG_NOTICE("create tests");

    sai_attribute_t list[3] = { };

    sai_attribute_t &attr1 = list[0];
    sai_attribute_t &attr2 = list[1];

    attr1.id = SAI_ROUTE_ENTRY_ATTR_NEXT_HOP_ID;
    attr1.value.oid = hop;

    attr2.id = SAI_ROUTE_ENTRY_ATTR_PACKET_ACTION;
    attr2.value.s32 = SAI_PACKET_ACTION_FORWARD;

    SWSS_LOG_NOTICE("correct ipv4");
    route_entry.destination.addr_family = SAI_IP_ADDR_FAMILY_IPV4;
    status = meta_sai_create_route_entry(&route_entry, 2, list, &dummy_success_sai_create_route_entry);
    META_ASSERT_SUCCESS(status);

    SWSS_LOG_NOTICE("already exists ipv4");
    route_entry.destination.addr_family = SAI_IP_ADDR_FAMILY_IPV4;
    status = meta_sai_create_route_entry(&route_entry, 2, list, &dummy_success_sai_create_route_entry);
    META_ASSERT_FAIL(status);

    route_entry.destination.addr_family = SAI_IP_ADDR_FAMILY_IPV6;
    sai_ip6_t ip62 = {0x00, 0x11, 0x22, 0x33,0x44, 0x55, 0x66,0x77, 0x88, 0x99, 0xaa, 0xbb,0xcc,0xdd,0xee,0x99};
    memcpy(route_entry.destination.addr.ip6, ip62, 16);

    sai_ip6_t ip6mask2 = {0xff, 0xff, 0xff, 0xff,0xff, 0xff, 0xff,0xff, 0xff, 0xff, 0xff, 0xff,0xff,0xff,0xf7,0x00};
    memcpy(route_entry.destination.mask.ip6, ip6mask2, 16);

    SWSS_LOG_NOTICE("invalid mask");
    status = meta_sai_create_route_entry(&route_entry, 2, list, &dummy_success_sai_create_route_entry);
    META_ASSERT_FAIL(status);

    route_entry.destination.addr_family = SAI_IP_ADDR_FAMILY_IPV6;
    sai_ip6_t ip6 = {0x00, 0x11, 0x22, 0x33,0x44, 0x55, 0x66,0x77, 0x88, 0x99, 0xaa, 0xbb,0xcc,0xdd,0xee,0xff};
    memcpy(route_entry.destination.addr.ip6, ip6, 16);

    sai_ip6_t ip6mask = {0xff, 0xff, 0xff, 0xff,0xff, 0xff, 0xff,0xff, 0xff, 0xff, 0xff, 0xff,0xff,0xff,0xff,0x00};
    memcpy(route_entry.destination.mask.ip6, ip6mask, 16);

    SWSS_LOG_NOTICE("correct ipv6");
    status = meta_sai_create_route_entry(&route_entry, 2, list, &dummy_success_sai_create_route_entry);
    META_ASSERT_SUCCESS(status);

    SWSS_LOG_NOTICE("already exists");
    status = meta_sai_create_route_entry(&route_entry, 2, list, &dummy_success_sai_create_route_entry);
    META_ASSERT_FAIL(status);

    SWSS_LOG_NOTICE("set tests");

    SWSS_LOG_NOTICE("correct packet action");
    attr.id = SAI_ROUTE_ENTRY_ATTR_PACKET_ACTION;
    attr.value.s32 = SAI_PACKET_ACTION_DROP;
    status = meta_sai_set_route_entry(&route_entry, &attr, &dummy_success_sai_set_route_entry);
    META_ASSERT_SUCCESS(status);

    SWSS_LOG_NOTICE("correct next hop");
    attr.id = SAI_ROUTE_ENTRY_ATTR_NEXT_HOP_ID;
    attr.value.oid = hop;
    status = meta_sai_set_route_entry(&route_entry, &attr, &dummy_success_sai_set_route_entry);
    META_ASSERT_SUCCESS(status);

    SWSS_LOG_NOTICE("correct metadata");
    attr.id = SAI_ROUTE_ENTRY_ATTR_META_DATA;
    attr.value.u32 = 0x12345678;
    status = meta_sai_set_route_entry(&route_entry, &attr, &dummy_success_sai_set_route_entry);
    META_ASSERT_SUCCESS(status);

    SWSS_LOG_NOTICE("get tests");

    SWSS_LOG_NOTICE("correct packet action");
    attr.id = SAI_ROUTE_ENTRY_ATTR_PACKET_ACTION;
    status = meta_sai_get_route_entry(&route_entry, 1, &attr, &dummy_success_sai_get_route_entry);
    META_ASSERT_SUCCESS(status);

    SWSS_LOG_NOTICE("correct next hop");
    attr.id = SAI_ROUTE_ENTRY_ATTR_NEXT_HOP_ID;
    status = meta_sai_get_route_entry(&route_entry, 1, &attr, &dummy_success_sai_get_route_entry);
    META_ASSERT_SUCCESS(status);

    SWSS_LOG_NOTICE("correct meta");
    attr.id = SAI_ROUTE_ENTRY_ATTR_META_DATA;
    status = meta_sai_get_route_entry(&route_entry, 1, &attr, &dummy_success_sai_get_route_entry);
    META_ASSERT_SUCCESS(status);

    SWSS_LOG_NOTICE("remove tests");

    SWSS_LOG_NOTICE("success");
    status = meta_sai_remove_route_entry(&route_entry, &dummy_success_sai_remove_route_entry);
    META_ASSERT_SUCCESS(status);

    SWSS_LOG_NOTICE("non existing");
    status = meta_sai_remove_route_entry(&route_entry, &dummy_success_sai_remove_route_entry);
    META_ASSERT_FAIL(status);

    route_entry.destination.addr_family = SAI_IP_ADDR_FAMILY_IPV4;
    route_entry.destination.addr.ip4 = htonl(0x0a00000f);
    route_entry.destination.mask.ip4 = htonl(0xffffff00);

    SWSS_LOG_NOTICE("success");
    status = meta_sai_remove_route_entry(&route_entry, &dummy_success_sai_remove_route_entry);
    META_ASSERT_SUCCESS(status);

    SWSS_LOG_NOTICE("non existing");
    status = meta_sai_remove_route_entry(&route_entry, &dummy_success_sai_remove_route_entry);
    META_ASSERT_FAIL(status);
}

// SERIALIZATION TYPES TESTS

void test_serialization_type_vlan_list()
{
    SWSS_LOG_ENTER();

    clear_local();
    meta_init_db();

    sai_status_t    status;

    sai_object_id_t stp;

    SWSS_LOG_NOTICE("create stp");
    sai_object_id_t switch_id = create_switch();

    status = meta_sai_create_oid(SAI_OBJECT_TYPE_STP, &stp, switch_id, 0, NULL, &dummy_success_sai_create_oid);
    META_ASSERT_SUCCESS(status);

    sai_vlan_id_t list[2] = { 1, 2 };

    sai_attribute_t attr;

    attr.id = SAI_STP_ATTR_VLAN_LIST;
    attr.value.vlanlist.count = 2;
    attr.value.vlanlist.list = list;

    SWSS_LOG_NOTICE("set vlan list");

    status = meta_sai_set_oid(SAI_OBJECT_TYPE_STP, stp, &attr, &dummy_success_sai_set_oid);
    META_ASSERT_FAIL(status);

    SWSS_LOG_NOTICE("get vlan list");

    status = meta_sai_get_oid(SAI_OBJECT_TYPE_STP, stp, 1, &attr, &dummy_success_sai_get_oid);
    META_ASSERT_SUCCESS(status);

    SWSS_LOG_NOTICE("remove stp");

    status = meta_sai_remove_oid(SAI_OBJECT_TYPE_STP, stp, &dummy_success_sai_remove_oid);
    META_ASSERT_SUCCESS(status);
}

void test_serialization_type_bool()
{
    SWSS_LOG_ENTER();

    clear_local();
    meta_init_db();

    sai_status_t    status;

    sai_object_id_t vr;

    SWSS_LOG_NOTICE("create stp");
    sai_object_id_t switch_id = create_switch();

    sai_attribute_t attr;

    attr.id = SAI_VIRTUAL_ROUTER_ATTR_ADMIN_V4_STATE;
    attr.value.booldata = true;

    status = meta_sai_create_oid(SAI_OBJECT_TYPE_VIRTUAL_ROUTER, &vr, switch_id, 1, &attr, &dummy_success_sai_create_oid);
    META_ASSERT_SUCCESS(status);

    SWSS_LOG_NOTICE("set bool");

    status = meta_sai_set_oid(SAI_OBJECT_TYPE_VIRTUAL_ROUTER, vr, &attr, &dummy_success_sai_set_oid);
    META_ASSERT_SUCCESS(status);

    SWSS_LOG_NOTICE("get bool");

    status = meta_sai_get_oid(SAI_OBJECT_TYPE_VIRTUAL_ROUTER, vr, 1, &attr, &dummy_success_sai_get_oid);
    META_ASSERT_SUCCESS(status);

    SWSS_LOG_NOTICE("remove vr");

    status = meta_sai_remove_oid(SAI_OBJECT_TYPE_VIRTUAL_ROUTER, vr, &dummy_success_sai_remove_oid);
    META_ASSERT_SUCCESS(status);
}

void test_serialization_type_char()
{
    SWSS_LOG_ENTER();

    clear_local();
    meta_init_db();

    sai_status_t    status;

    sai_object_id_t hostif;

    SWSS_LOG_NOTICE("create rif");
    sai_object_id_t switch_id = create_switch();

    // TODO we should use create
    sai_object_id_t rif = create_dummy_object_id(SAI_OBJECT_TYPE_PORT,switch_id);
    object_reference_insert(rif);
    sai_object_meta_key_t meta_key_rif = { .objecttype = SAI_OBJECT_TYPE_ROUTER_INTERFACE, .objectkey = { .key = { .object_id = rif } } };
    std::string rif_key = sai_serialize_object_meta_key(meta_key_rif);
    ObjectAttrHash[rif_key] = { };

    sai_attribute_t attr, attr2, attr3;

    attr.id = SAI_HOSTIF_ATTR_TYPE;
    attr.value.s32 = SAI_HOSTIF_TYPE_NETDEV;

    attr2.id = SAI_HOSTIF_ATTR_OBJ_ID;
    attr2.value.oid = rif;

    attr3.id = SAI_HOSTIF_ATTR_NAME;

    memcpy(attr3.value.chardata, "foo", sizeof("foo"));

    sai_attribute_t list[3] = { attr, attr2, attr3 };

    // TODO we need to support conditions here

    SWSS_LOG_NOTICE("create hostif");

    status = meta_sai_create_oid(SAI_OBJECT_TYPE_HOSTIF, &hostif, switch_id, 3, list, &dummy_success_sai_create_oid);
    META_ASSERT_SUCCESS(status);

    SWSS_LOG_NOTICE("set char");

    status = meta_sai_set_oid(SAI_OBJECT_TYPE_HOSTIF, hostif, &attr3, &dummy_success_sai_set_oid);
    META_ASSERT_FAIL(status);

    SWSS_LOG_NOTICE("get char");

    status = meta_sai_get_oid(SAI_OBJECT_TYPE_HOSTIF, hostif, 1, &attr3, &dummy_success_sai_get_oid);
    META_ASSERT_SUCCESS(status);

    SWSS_LOG_NOTICE("remove hostif");

    status = meta_sai_remove_oid(SAI_OBJECT_TYPE_HOSTIF, hostif, &dummy_success_sai_remove_oid);
    META_ASSERT_SUCCESS(status);

    attr.id = SAI_HOSTIF_ATTR_TYPE;
    attr.value.s32 = SAI_HOSTIF_TYPE_FD;

    sai_attribute_t list2[1] = { attr };

    SWSS_LOG_NOTICE("create hostif with non mandatory");
    status = meta_sai_create_oid(SAI_OBJECT_TYPE_HOSTIF, &hostif, switch_id, 1, list2, &dummy_success_sai_create_oid);
    META_ASSERT_SUCCESS(status);

// TODO this test should pass, we are doing query here for conditional
// attribute, where condition is not met so this attribute will not be used, so
// metadata should figure out that we can't query this attribute, but there is
// a problem with internal existing objects, since we don't have their values
// then we we can't tell whether attribute was passed or not, we need to get
// switch discovered objects and attributes and populate local db then we need
// to update metadata condition in meta_generic_validation_get method where we
// check if attribute is conditional
//
//    SWSS_LOG_NOTICE("get char");
//
//    status = meta_sai_get_oid(SAI_OBJECT_TYPE_HOSTIF, hostif, 1, &attr2, &dummy_success_sai_get_oid);
//    META_ASSERT_FAIL(status);

    attr.id = SAI_HOSTIF_ATTR_TYPE;
    attr.value.s32 = SAI_HOSTIF_TYPE_NETDEV;

    sai_attribute_t list3[1] = { attr };

    SWSS_LOG_NOTICE("create hostif with mandatory missing");
    status = meta_sai_create_oid(SAI_OBJECT_TYPE_HOSTIF, &hostif, switch_id, 1, list3, &dummy_success_sai_create_oid);
    META_ASSERT_FAIL(status);
}

void test_serialization_type_int32_list()
{
    SWSS_LOG_ENTER();

    clear_local();
    meta_init_db();

    sai_status_t    status;

    sai_object_id_t hash;

    sai_attribute_t attr;

    SWSS_LOG_NOTICE("create hash");
    sai_object_id_t switch_id = create_switch();

    int32_t list[2] =  { SAI_NATIVE_HASH_FIELD_SRC_IP, SAI_NATIVE_HASH_FIELD_VLAN_ID };

    attr.id = SAI_HASH_ATTR_NATIVE_HASH_FIELD_LIST;
    attr.value.s32list.count = 2;
    attr.value.s32list.list = list;

    status = meta_sai_create_oid(SAI_OBJECT_TYPE_HASH, &hash, switch_id, 1, &attr, &dummy_success_sai_create_oid);
    META_ASSERT_SUCCESS(status);

    SWSS_LOG_NOTICE("set hash");

    status = meta_sai_set_oid(SAI_OBJECT_TYPE_HASH, hash, &attr, &dummy_success_sai_set_oid);
    META_ASSERT_SUCCESS(status);

    SWSS_LOG_NOTICE("get hash");

    status = meta_sai_get_oid(SAI_OBJECT_TYPE_HASH, hash, 1, &attr, &dummy_success_sai_get_oid);
    META_ASSERT_SUCCESS(status);

    SWSS_LOG_NOTICE("remove hash");

    status = meta_sai_remove_oid(SAI_OBJECT_TYPE_HASH, hash, &dummy_success_sai_remove_oid);
    META_ASSERT_SUCCESS(status);
}

void test_serialization_type_uint32_list()
{
    SWSS_LOG_ENTER();

    clear_local();
    meta_init_db();

    sai_status_t    status;

    sai_object_id_t hash;

    sai_attribute_t attr;

    SWSS_LOG_NOTICE("create hash");
    sai_object_id_t switch_id = create_switch();

    int32_t list[2] =  { SAI_NATIVE_HASH_FIELD_SRC_IP, SAI_NATIVE_HASH_FIELD_VLAN_ID };

    attr.id = SAI_HASH_ATTR_NATIVE_HASH_FIELD_LIST;
    attr.value.s32list.count = 2;
    attr.value.s32list.list = list;

    status = meta_sai_create_oid(SAI_OBJECT_TYPE_HASH, &hash, switch_id, 1, &attr, &dummy_success_sai_create_oid);
    META_ASSERT_SUCCESS(status);

    SWSS_LOG_NOTICE("set hash");

    status = meta_sai_set_oid(SAI_OBJECT_TYPE_HASH, hash, &attr, &dummy_success_sai_set_oid);
    META_ASSERT_SUCCESS(status);

    SWSS_LOG_NOTICE("get hash");

    status = meta_sai_get_oid(SAI_OBJECT_TYPE_HASH, hash, 1, &attr, &dummy_success_sai_get_oid);
    META_ASSERT_SUCCESS(status);

    SWSS_LOG_NOTICE("remove hash");

    status = meta_sai_remove_oid(SAI_OBJECT_TYPE_HASH, hash, &dummy_success_sai_remove_oid);
    META_ASSERT_SUCCESS(status);
}

// OTHER

void test_mask()
{
    SWSS_LOG_ENTER();

    sai_ip6_t ip6mask1 = {0xff, 0xff, 0xff, 0xff,0xff, 0xff, 0xff,0xff, 0xff, 0xff, 0xff, 0xff,0xff,0xff,0xff,0x00};
    sai_ip6_t ip6mask2 = {0xff, 0xff, 0xff, 0xff,0xff, 0xff, 0xff,0xff, 0xff, 0xff, 0xff, 0xff,0xff,0xff,0xff,0xff};
    sai_ip6_t ip6mask3 = {0x00, 0x00, 0x00, 0x00,0x00, 0x00, 0x00,0x00, 0x00, 0x00, 0x00, 0x00,0x00,0x00,0x00,0x00};
    sai_ip6_t ip6mask4 = {0xff, 0xff, 0xff, 0xff,0xff, 0xff, 0xff,0xff, 0xff, 0xff, 0xff, 0xff,0xff,0xff,0xff,0xfe};
    sai_ip6_t ip6mask5 = {0x80, 0x00, 0x00, 0x00,0x00, 0x00, 0x00,0x00, 0x00, 0x00, 0x00, 0x00,0x00,0x00,0x00,0x00};

    sai_ip6_t ip6mask6 = {0x01, 0x00, 0x00, 0x00,0x00, 0x00, 0x00,0x00, 0x00, 0x00, 0x00, 0x00,0x00,0x00,0x00,0x00};
    sai_ip6_t ip6mask7 = {0xff, 0xff, 0xff, 0xff,0xff, 0xff, 0xff,0xff, 0xff, 0xff, 0xff, 0xff,0xff,0xff,0xff,0x8f};
    sai_ip6_t ip6mask8 = {0xff, 0xff, 0xff, 0xff,0xff, 0xff, 0xff,0xff, 0xff, 0xff, 0xff, 0xff,0xff,0xff,0xff,0x8f};
    sai_ip6_t ip6mask9 = {0xff, 0xff, 0xff, 0xff,0xff, 0xff, 0xff,0xf1, 0xff, 0xff, 0xff, 0xff,0xff,0xff,0xff,0xff};

    META_ASSERT_TRUE(is_ipv6_mask_valid(ip6mask1));
    META_ASSERT_TRUE(is_ipv6_mask_valid(ip6mask2));
    META_ASSERT_TRUE(is_ipv6_mask_valid(ip6mask3));
    META_ASSERT_TRUE(is_ipv6_mask_valid(ip6mask4));
    META_ASSERT_TRUE(is_ipv6_mask_valid(ip6mask5));

    META_ASSERT_TRUE(!is_ipv6_mask_valid(ip6mask6));
    META_ASSERT_TRUE(!is_ipv6_mask_valid(ip6mask7));
    META_ASSERT_TRUE(!is_ipv6_mask_valid(ip6mask8));
    META_ASSERT_TRUE(!is_ipv6_mask_valid(ip6mask9));
}

sai_object_id_t insert_dummy_object(
        _In_ sai_object_type_t ot,
        _In_ sai_object_id_t switch_id)
{
    SWSS_LOG_ENTER();

    // TODO we should use create
    sai_object_id_t oid = create_dummy_object_id(ot,switch_id);
    object_reference_insert(oid);
    sai_object_meta_key_t meta_key_oid = { .objecttype = ot, .objectkey = { .key = { .object_id = oid } } };
    std::string oid_key = sai_serialize_object_meta_key(meta_key_oid);
    ObjectAttrHash[oid_key] = { };

    return oid;
}

void test_acl_entry_field_and_action()
{
    SWSS_LOG_ENTER();

    clear_local();
    meta_init_db();

    sai_object_id_t switch_id = create_switch();
    sai_status_t    status;

    sai_object_id_t aclentry;

    int32_t ids[] = {
        SAI_ACL_ENTRY_ATTR_TABLE_ID,
        SAI_ACL_ENTRY_ATTR_PRIORITY,
        SAI_ACL_ENTRY_ATTR_FIELD_SRC_IPV6,
        SAI_ACL_ENTRY_ATTR_FIELD_DST_IPV6,
        SAI_ACL_ENTRY_ATTR_FIELD_SRC_MAC,
        SAI_ACL_ENTRY_ATTR_FIELD_DST_MAC,
        SAI_ACL_ENTRY_ATTR_FIELD_SRC_IP,
        SAI_ACL_ENTRY_ATTR_FIELD_DST_IP,
        SAI_ACL_ENTRY_ATTR_FIELD_IN_PORTS,
        SAI_ACL_ENTRY_ATTR_FIELD_OUT_PORTS,
        SAI_ACL_ENTRY_ATTR_FIELD_IN_PORT,
        SAI_ACL_ENTRY_ATTR_FIELD_OUT_PORT,
        SAI_ACL_ENTRY_ATTR_FIELD_SRC_PORT,
        SAI_ACL_ENTRY_ATTR_FIELD_OUTER_VLAN_ID,
        SAI_ACL_ENTRY_ATTR_FIELD_OUTER_VLAN_PRI,
        SAI_ACL_ENTRY_ATTR_FIELD_OUTER_VLAN_CFI,
        SAI_ACL_ENTRY_ATTR_FIELD_INNER_VLAN_ID,
        SAI_ACL_ENTRY_ATTR_FIELD_INNER_VLAN_PRI,
        SAI_ACL_ENTRY_ATTR_FIELD_INNER_VLAN_CFI,
        SAI_ACL_ENTRY_ATTR_FIELD_L4_SRC_PORT,
        SAI_ACL_ENTRY_ATTR_FIELD_L4_DST_PORT,
        SAI_ACL_ENTRY_ATTR_FIELD_ETHER_TYPE,
        SAI_ACL_ENTRY_ATTR_FIELD_IP_PROTOCOL,
        SAI_ACL_ENTRY_ATTR_FIELD_DSCP,
        SAI_ACL_ENTRY_ATTR_FIELD_ECN,
        SAI_ACL_ENTRY_ATTR_FIELD_TTL,
        SAI_ACL_ENTRY_ATTR_FIELD_TOS,
        SAI_ACL_ENTRY_ATTR_FIELD_IP_FLAGS,
        SAI_ACL_ENTRY_ATTR_FIELD_TCP_FLAGS,
        SAI_ACL_ENTRY_ATTR_FIELD_ACL_IP_FRAG,
        SAI_ACL_ENTRY_ATTR_FIELD_IPV6_FLOW_LABEL,
        SAI_ACL_ENTRY_ATTR_FIELD_TC,
        SAI_ACL_ENTRY_ATTR_FIELD_ICMP_TYPE,
        SAI_ACL_ENTRY_ATTR_FIELD_ICMP_CODE,
        SAI_ACL_ENTRY_ATTR_FIELD_FDB_DST_USER_META,
        SAI_ACL_ENTRY_ATTR_FIELD_ROUTE_DST_USER_META,
        SAI_ACL_ENTRY_ATTR_FIELD_NEIGHBOR_DST_USER_META,
        SAI_ACL_ENTRY_ATTR_FIELD_PORT_USER_META,
        SAI_ACL_ENTRY_ATTR_FIELD_VLAN_USER_META,
        SAI_ACL_ENTRY_ATTR_FIELD_ACL_USER_META,
        SAI_ACL_ENTRY_ATTR_FIELD_FDB_NPU_META_DST_HIT,
        SAI_ACL_ENTRY_ATTR_FIELD_NEIGHBOR_NPU_META_DST_HIT,
        SAI_ACL_ENTRY_ATTR_FIELD_ROUTE_NPU_META_DST_HIT,
        SAI_ACL_ENTRY_ATTR_ACTION_REDIRECT,
        //SAI_ACL_ENTRY_ATTR_ACTION_REDIRECT_LIST,
        SAI_ACL_ENTRY_ATTR_ACTION_PACKET_ACTION,
        SAI_ACL_ENTRY_ATTR_ACTION_FLOOD,
        SAI_ACL_ENTRY_ATTR_ACTION_COUNTER,
        SAI_ACL_ENTRY_ATTR_ACTION_MIRROR_INGRESS,
        SAI_ACL_ENTRY_ATTR_ACTION_MIRROR_EGRESS,
        SAI_ACL_ENTRY_ATTR_ACTION_SET_POLICER,
        SAI_ACL_ENTRY_ATTR_ACTION_DECREMENT_TTL,
        SAI_ACL_ENTRY_ATTR_ACTION_SET_TC,
        SAI_ACL_ENTRY_ATTR_ACTION_SET_PACKET_COLOR,
        SAI_ACL_ENTRY_ATTR_ACTION_SET_INNER_VLAN_ID,
        SAI_ACL_ENTRY_ATTR_ACTION_SET_INNER_VLAN_PRI,
        SAI_ACL_ENTRY_ATTR_ACTION_SET_OUTER_VLAN_ID,
        SAI_ACL_ENTRY_ATTR_ACTION_SET_OUTER_VLAN_PRI,
        SAI_ACL_ENTRY_ATTR_ACTION_SET_SRC_MAC,
        SAI_ACL_ENTRY_ATTR_ACTION_SET_DST_MAC,
        SAI_ACL_ENTRY_ATTR_ACTION_SET_SRC_IP,
        SAI_ACL_ENTRY_ATTR_ACTION_SET_DST_IP,
        SAI_ACL_ENTRY_ATTR_ACTION_SET_SRC_IPV6,
        SAI_ACL_ENTRY_ATTR_ACTION_SET_DST_IPV6,
        SAI_ACL_ENTRY_ATTR_ACTION_SET_DSCP,
        SAI_ACL_ENTRY_ATTR_ACTION_SET_ECN,
        SAI_ACL_ENTRY_ATTR_ACTION_SET_L4_SRC_PORT,
        SAI_ACL_ENTRY_ATTR_ACTION_SET_L4_DST_PORT,
        SAI_ACL_ENTRY_ATTR_ACTION_INGRESS_SAMPLEPACKET_ENABLE,
        SAI_ACL_ENTRY_ATTR_ACTION_EGRESS_SAMPLEPACKET_ENABLE,
        SAI_ACL_ENTRY_ATTR_ACTION_SET_ACL_META_DATA,
        SAI_ACL_ENTRY_ATTR_ACTION_EGRESS_BLOCK_PORT_LIST,
        SAI_ACL_ENTRY_ATTR_ACTION_SET_USER_TRAP_ID,
    };

    std::vector<sai_attribute_t> vattrs;

    // all lists are empty we need info if that is possible

    for (uint32_t i = 0; i < sizeof(ids)/sizeof(int32_t); ++i)
    {
        sai_attribute_t attr;

        memset(&attr,0,sizeof(attr));

        attr.value.aclfield.enable = true;
        attr.value.aclaction.enable = true;

        attr.id = ids[i];

        if (attr.id == SAI_ACL_ENTRY_ATTR_TABLE_ID)
            attr.value.oid = insert_dummy_object(SAI_OBJECT_TYPE_ACL_TABLE,switch_id);

        if (attr.id == SAI_ACL_ENTRY_ATTR_FIELD_IN_PORT)
            attr.value.aclfield.data.oid = insert_dummy_object(SAI_OBJECT_TYPE_PORT,switch_id);

        if (attr.id == SAI_ACL_ENTRY_ATTR_FIELD_OUT_PORT)
            attr.value.aclfield.data.oid = insert_dummy_object(SAI_OBJECT_TYPE_PORT,switch_id);

        if (attr.id == SAI_ACL_ENTRY_ATTR_FIELD_SRC_PORT)
            attr.value.aclfield.data.oid = insert_dummy_object(SAI_OBJECT_TYPE_PORT,switch_id);

        if (attr.id == SAI_ACL_ENTRY_ATTR_ACTION_REDIRECT)
            attr.value.aclaction.parameter.oid = insert_dummy_object(SAI_OBJECT_TYPE_PORT,switch_id);

        if (attr.id == SAI_ACL_ENTRY_ATTR_ACTION_COUNTER)
            attr.value.aclaction.parameter.oid = insert_dummy_object(SAI_OBJECT_TYPE_ACL_COUNTER,switch_id);

        if (attr.id == SAI_ACL_ENTRY_ATTR_ACTION_SET_POLICER)
            attr.value.aclaction.parameter.oid = insert_dummy_object(SAI_OBJECT_TYPE_POLICER,switch_id);

        if (attr.id == SAI_ACL_ENTRY_ATTR_ACTION_INGRESS_SAMPLEPACKET_ENABLE)
            attr.value.aclaction.parameter.oid = insert_dummy_object(SAI_OBJECT_TYPE_SAMPLEPACKET,switch_id);

        if (attr.id == SAI_ACL_ENTRY_ATTR_ACTION_EGRESS_SAMPLEPACKET_ENABLE)
            attr.value.aclaction.parameter.oid = insert_dummy_object(SAI_OBJECT_TYPE_SAMPLEPACKET,switch_id);

        if (attr.id == SAI_ACL_ENTRY_ATTR_ACTION_SET_USER_TRAP_ID)
            attr.value.aclaction.parameter.oid = insert_dummy_object(SAI_OBJECT_TYPE_HOSTIF_USER_DEFINED_TRAP,switch_id);

        if (attr.id == SAI_ACL_ENTRY_ATTR_ACTION_REDIRECT_LIST)
        {
            sai_object_id_t list[1];

            list[0] = insert_dummy_object(SAI_OBJECT_TYPE_QUEUE,switch_id);

            SWSS_LOG_NOTICE("0x%lx", list[0]);

            attr.value.aclaction.parameter.objlist.count = 1;
            attr.value.aclaction.parameter.objlist.list = list;
        }

        vattrs.push_back(attr);
    }

    SWSS_LOG_NOTICE("create acl entry");

    status = meta_sai_create_oid(SAI_OBJECT_TYPE_ACL_ENTRY, &aclentry, switch_id, (uint32_t)vattrs.size(), vattrs.data(), &dummy_success_sai_create_oid);
    META_ASSERT_SUCCESS(status);

    for (uint32_t i = 0; i < sizeof(ids)/sizeof(int32_t); ++i)
    {
        if (vattrs[i].id == SAI_ACL_ENTRY_ATTR_TABLE_ID)
            continue;

        auto m = sai_metadata_get_attr_metadata(SAI_OBJECT_TYPE_ACL_ENTRY, vattrs[i].id);

        SWSS_LOG_NOTICE("set aclentry %u %s", i, m->attridname);

        status = meta_sai_set_oid(SAI_OBJECT_TYPE_ACL_ENTRY, aclentry, &vattrs[i], &dummy_success_sai_set_oid);
        META_ASSERT_SUCCESS(status);
    }

    SWSS_LOG_NOTICE("get aclentry");

    status = meta_sai_get_oid(SAI_OBJECT_TYPE_ACL_ENTRY, aclentry, (uint32_t)vattrs.size(), vattrs.data(), &dummy_success_sai_get_oid);
    META_ASSERT_SUCCESS(status);

    SWSS_LOG_NOTICE("remove aclentry");

    status = meta_sai_remove_oid(SAI_OBJECT_TYPE_ACL_ENTRY, aclentry, &dummy_success_sai_remove_oid);
    META_ASSERT_SUCCESS(status);
}

void test_construct_key()
{
    SWSS_LOG_ENTER();

    clear_local();
    meta_init_db();

    sai_attribute_t attr;

    uint32_t list[4] = {1,2,3,4};

    attr.id = SAI_PORT_ATTR_HW_LANE_LIST;
    attr.value.u32list.count = 4;
    attr.value.u32list.list = list;

    sai_object_meta_key_t meta_key;

    meta_key.objecttype = SAI_OBJECT_TYPE_PORT;

    std::string key = construct_key(meta_key, 1, &attr);

    SWSS_LOG_NOTICE("constructed key: %s", key.c_str());

    META_ASSERT_TRUE(key == "SAI_PORT_ATTR_HW_LANE_LIST:1,2,3,4;");
}

void test_queue_create()
{
    SWSS_LOG_ENTER();

    clear_local();
    meta_init_db();

    sai_object_id_t switch_id = create_switch();

    sai_status_t    status;
    sai_object_id_t queue;

    sai_attribute_t attr1;
    sai_attribute_t attr2;
    sai_attribute_t attr3;
    sai_attribute_t attr4;

    attr1.id = SAI_QUEUE_ATTR_TYPE;
    attr1.value.s32 = SAI_QUEUE_TYPE_UNICAST;

    attr2.id = SAI_QUEUE_ATTR_INDEX;
    attr2.value.u8 = 7;

    attr3.id = SAI_QUEUE_ATTR_PORT;
    attr3.value.oid = create_dummy_object_id(SAI_OBJECT_TYPE_PORT, switch_id);

    object_reference_insert(attr3.value.oid);

    attr4.id = SAI_QUEUE_ATTR_PARENT_SCHEDULER_NODE;
    attr4.value.oid = create_dummy_object_id(SAI_OBJECT_TYPE_SCHEDULER_GROUP, switch_id);

    object_reference_insert(attr4.value.oid);

    sai_attribute_t list[4] = { attr1, attr2, attr3, attr4 };

    SWSS_LOG_NOTICE("create tests");

    SWSS_LOG_NOTICE("create queue");
    status = meta_sai_create_oid(SAI_OBJECT_TYPE_QUEUE, &queue, switch_id, 4, list, &dummy_success_sai_create_oid);
    META_ASSERT_SUCCESS(status);

    SWSS_LOG_NOTICE("create queue but key exists");
    status = meta_sai_create_oid(SAI_OBJECT_TYPE_QUEUE, &queue, switch_id, 4, list, &dummy_success_sai_create_oid);
    META_ASSERT_FAIL(status);

    SWSS_LOG_NOTICE("remove queue");
    status = meta_sai_remove_oid(SAI_OBJECT_TYPE_QUEUE, queue, &dummy_success_sai_remove_oid);
    META_ASSERT_SUCCESS(status);

    SWSS_LOG_NOTICE("create queue");
    status = meta_sai_create_oid(SAI_OBJECT_TYPE_QUEUE, &queue, switch_id, 4, list, &dummy_success_sai_create_oid);
    META_ASSERT_SUCCESS(status);
}

void test_null_list()
{
    SWSS_LOG_ENTER();

    clear_local();
    meta_init_db();

    sai_status_t    status;

    sai_object_id_t hash;

    sai_attribute_t attr;

    int32_t list[2] =  { SAI_NATIVE_HASH_FIELD_SRC_IP, SAI_NATIVE_HASH_FIELD_VLAN_ID };

    attr.id = SAI_HASH_ATTR_NATIVE_HASH_FIELD_LIST;
    attr.value.s32list.count = 0;
    attr.value.s32list.list = list;
    sai_object_id_t switch_id = create_switch();

    SWSS_LOG_NOTICE("0 count, not null list");
    status = meta_sai_create_oid(SAI_OBJECT_TYPE_HASH, &hash, switch_id, 1, &attr, &dummy_success_sai_create_oid);
    META_ASSERT_FAIL(status);

    attr.value.s32list.list = NULL;

    SWSS_LOG_NOTICE("0 count, null list");
    status = meta_sai_create_oid(SAI_OBJECT_TYPE_HASH, &hash, switch_id, 1, &attr, &dummy_success_sai_create_oid);
    META_ASSERT_SUCCESS(status);
}

void test_priority_group()
{
    SWSS_LOG_ENTER();

    clear_local();
    meta_init_db();

    sai_object_id_t switch_id = create_switch();

    sai_status_t status;

    sai_attribute_t attr;

    sai_object_id_t pg = insert_dummy_object(SAI_OBJECT_TYPE_INGRESS_PRIORITY_GROUP, switch_id);

    SWSS_LOG_NOTICE("set SAI_INGRESS_PRIORITY_GROUP_ATTR_BUFFER_PROFILE attr");

    attr.id = SAI_INGRESS_PRIORITY_GROUP_ATTR_BUFFER_PROFILE;
    attr.value.oid = SAI_NULL_OBJECT_ID;
    status = meta_sai_set_oid(SAI_OBJECT_TYPE_INGRESS_PRIORITY_GROUP, pg, &attr, &dummy_success_sai_set_oid);
    META_ASSERT_SUCCESS(status);
}

#define ASSERT_TRUE(x,y)\
    if ((x) != y) { std::cout << "assert true failed: '" << x << "' != '" << y << "'" << std::endl; throw; }

#define ASSERT_FAIL(msg) \
    { std::cout << "assert failed: " << msg << std::endl; throw; }

void test_serialize_bool()
{
    SWSS_LOG_ENTER();

    clear_local();
    meta_init_db();

    sai_attribute_t attr;
    const sai_attr_metadata_t* meta;
    std::string s;

    // test bool

    attr.id = SAI_SWITCH_ATTR_ON_LINK_ROUTE_SUPPORTED;
    attr.value.booldata = true;

    meta = sai_metadata_get_attr_metadata(SAI_OBJECT_TYPE_SWITCH, attr.id);

    s = sai_serialize_attr_value(*meta, attr);

    ASSERT_TRUE(s, "true");

    attr.id = SAI_SWITCH_ATTR_ON_LINK_ROUTE_SUPPORTED;
    attr.value.booldata = false;

    s = sai_serialize_attr_value(*meta, attr);

    ASSERT_TRUE(s, "false");

    // deserialize

    attr.id = SAI_SWITCH_ATTR_ON_LINK_ROUTE_SUPPORTED;

    sai_deserialize_attr_value("true", *meta, attr);
    ASSERT_TRUE(true, attr.value.booldata);

    sai_deserialize_attr_value("false", *meta, attr);
    ASSERT_TRUE(false, attr.value.booldata);

    try
    {
        sai_deserialize_attr_value("xx", *meta, attr);
        ASSERT_FAIL("invalid bool deserialize failed to throw exception");
    }
    catch (const std::runtime_error& e)
    {
        // ok
    }
}

void test_serialize_chardata()
{
    SWSS_LOG_ENTER();

    clear_local();
    meta_init_db();

    sai_attribute_t attr;
    const sai_attr_metadata_t* meta;
    std::string s;

    memset(attr.value.chardata, 0, 32);

    attr.id = SAI_HOSTIF_ATTR_NAME;
    memcpy(attr.value.chardata, "foo", sizeof("foo"));

    meta = sai_metadata_get_attr_metadata(SAI_OBJECT_TYPE_HOSTIF, attr.id);

    s = sai_serialize_attr_value(*meta, attr);

    ASSERT_TRUE(s, "foo");

    attr.id = SAI_HOSTIF_ATTR_NAME;
    memcpy(attr.value.chardata, "f\\oo\x12", sizeof("f\\oo\x12"));

    meta = sai_metadata_get_attr_metadata(SAI_OBJECT_TYPE_HOSTIF, attr.id);

    s = sai_serialize_attr_value(*meta, attr);

    ASSERT_TRUE(s, "f\\\\oo\\x12");

    attr.id = SAI_HOSTIF_ATTR_NAME;
    memcpy(attr.value.chardata, "\x80\xff", sizeof("\x80\xff"));

    meta = sai_metadata_get_attr_metadata(SAI_OBJECT_TYPE_HOSTIF, attr.id);

    s = sai_serialize_attr_value(*meta, attr);

    ASSERT_TRUE(s, "\\x80\\xFF");

    // deserialize

    sai_deserialize_attr_value("f\\\\oo\\x12", *meta, attr);

    SWSS_LOG_NOTICE("des: %s", attr.value.chardata);

    ASSERT_TRUE(0, strcmp(attr.value.chardata, "f\\oo\x12"));

    sai_deserialize_attr_value("foo", *meta, attr);

    ASSERT_TRUE(0, strcmp(attr.value.chardata, "foo"));

    try
    {
        sai_deserialize_attr_value("\\x2g", *meta, attr);
        ASSERT_FAIL("invalid chardata deserialize failed to throw exception");
    }
    catch (const std::runtime_error& e)
    {
        // ok
    }

    try
    {
        sai_deserialize_attr_value("\\x2", *meta, attr);
        ASSERT_FAIL("invalid chardata deserialize failed to throw exception");
    }
    catch (const std::runtime_error& e)
    {
        // ok
    }

    try
    {
        sai_deserialize_attr_value("\\s45", *meta, attr);
        ASSERT_FAIL("invalid chardata deserialize failed to throw exception");
    }
    catch (const std::runtime_error& e)
    {
        // ok
    }

    try
    {
        sai_deserialize_attr_value("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", *meta, attr);
        ASSERT_FAIL("invalid chardata deserialize failed to throw exception");
    }
    catch (const std::runtime_error& e)
    {
        // ok
    }
}

void test_serialize_uint64()
{
    SWSS_LOG_ENTER();

    clear_local();
    meta_init_db();

    sai_attribute_t attr;
    const sai_attr_metadata_t* meta;
    std::string s;

    attr.id = SAI_SWITCH_ATTR_NV_STORAGE_SIZE;
    attr.value.u64 = 42;

    meta = sai_metadata_get_attr_metadata(SAI_OBJECT_TYPE_SWITCH, attr.id);

    s = sai_serialize_attr_value(*meta, attr);

    ASSERT_TRUE(s, "42");
    attr.value.u64 = 0x87654321aabbccdd;

    attr.id = SAI_SWITCH_ATTR_NV_STORAGE_SIZE;

    meta = sai_metadata_get_attr_metadata(SAI_OBJECT_TYPE_SWITCH, attr.id);

    s = sai_serialize_attr_value(*meta, attr);

    char buf[32];
    sprintf(buf, "%lu", attr.value.u64);

    ASSERT_TRUE(s, std::string(buf));

    // deserialize

    sai_deserialize_attr_value("12345", *meta, attr);

    ASSERT_TRUE(12345, attr.value.u64);

    try
    {
        sai_deserialize_attr_value("22345235345345345435", *meta, attr);
        ASSERT_FAIL("invalid u64 deserialize failed to throw exception");
    }
    catch (const std::runtime_error& e)
    {
        // ok
    }

    try
    {
        sai_deserialize_attr_value("2a", *meta, attr);
        ASSERT_FAIL("invalid u64 deserialize failed to throw exception");
    }
    catch (const std::runtime_error& e)
    {
        // ok
    }
}

void test_serialize_enum()
{
    SWSS_LOG_ENTER();

    clear_local();
    meta_init_db();

    sai_attribute_t attr;
    const sai_attr_metadata_t* meta;
    std::string s;

    attr.id = SAI_SWITCH_ATTR_SWITCHING_MODE;
    attr.value.s32 = SAI_SWITCH_SWITCHING_MODE_STORE_AND_FORWARD;

    meta = sai_metadata_get_attr_metadata(SAI_OBJECT_TYPE_SWITCH, attr.id);

    s = sai_serialize_attr_value(*meta, attr);

    ASSERT_TRUE(s, "SAI_SWITCH_SWITCHING_MODE_STORE_AND_FORWARD");

    attr.value.s32 = -1;

    attr.id = SAI_SWITCH_ATTR_SWITCHING_MODE;

    meta = sai_metadata_get_attr_metadata(SAI_OBJECT_TYPE_SWITCH, attr.id);

    s = sai_serialize_attr_value(*meta, attr);

    ASSERT_TRUE(s, "-1");

    attr.value.s32 = 100;

    attr.id = SAI_SWITCH_ATTR_SWITCHING_MODE;

    meta = sai_metadata_get_attr_metadata(SAI_OBJECT_TYPE_SWITCH, attr.id);

    s = sai_serialize_attr_value(*meta, attr);

    ASSERT_TRUE(s, "100");

    // deserialize

    sai_deserialize_attr_value("12345", *meta, attr);

    ASSERT_TRUE(12345, attr.value.s32);

    sai_deserialize_attr_value("-1", *meta, attr);

    ASSERT_TRUE(-1, attr.value.s32);

    sai_deserialize_attr_value("SAI_SWITCH_SWITCHING_MODE_STORE_AND_FORWARD", *meta, attr);

    ASSERT_TRUE(SAI_SWITCH_SWITCHING_MODE_STORE_AND_FORWARD, attr.value.s32);

    try
    {
        sai_deserialize_attr_value("foo", *meta, attr);
        ASSERT_FAIL("invalid s32 deserialize failed to throw exception");
    }
    catch (const std::runtime_error& e)
    {
        // ok
    }
}

void test_serialize_mac()
{
    SWSS_LOG_ENTER();

    clear_local();
    meta_init_db();

    sai_attribute_t attr;
    const sai_attr_metadata_t* meta;
    std::string s;

    attr.id = SAI_SWITCH_ATTR_SRC_MAC_ADDRESS;
    memcpy(attr.value.mac, "\x01\x22\x33\xaa\xbb\xcc", 6);

    meta = sai_metadata_get_attr_metadata(SAI_OBJECT_TYPE_SWITCH, attr.id);

    s = sai_serialize_attr_value(*meta, attr);

    ASSERT_TRUE(s, "01:22:33:AA:BB:CC");

    // deserialize

    sai_deserialize_attr_value("ff:ee:dd:33:44:55", *meta, attr);

    ASSERT_TRUE(0, memcmp("\xff\xee\xdd\x33\x44\x55", attr.value.mac, 6));

    try
    {
        sai_deserialize_attr_value("foo", *meta, attr);
        ASSERT_FAIL("invalid s32 deserialize failed to throw exception");
    }
    catch (const std::runtime_error& e)
    {
        // ok
    }
}

void test_serialize_ip_address()
{
    SWSS_LOG_ENTER();

    clear_local();
    meta_init_db();

    sai_attribute_t attr;
    const sai_attr_metadata_t* meta;
    std::string s;

    attr.id = SAI_TUNNEL_ATTR_ENCAP_SRC_IP;
    attr.value.ipaddr.addr_family = SAI_IP_ADDR_FAMILY_IPV4;
    attr.value.ipaddr.addr.ip4 = htonl(0x0a000015);

    meta = sai_metadata_get_attr_metadata(SAI_OBJECT_TYPE_TUNNEL, attr.id);

    s = sai_serialize_attr_value(*meta, attr);

    ASSERT_TRUE(s, "10.0.0.21");

    attr.id = SAI_TUNNEL_ATTR_ENCAP_SRC_IP;
    attr.value.ipaddr.addr_family = SAI_IP_ADDR_FAMILY_IPV6;

    uint16_t ip6[] = { 0x1111, 0x2222, 0x3333, 0x4444, 0x5555, 0x6666, 0xaaaa, 0xbbbb };

    memcpy(attr.value.ipaddr.addr.ip6, ip6, 16);

    meta = sai_metadata_get_attr_metadata(SAI_OBJECT_TYPE_TUNNEL, attr.id);

    s = sai_serialize_attr_value(*meta, attr);

    ASSERT_TRUE(s, "1111:2222:3333:4444:5555:6666:aaaa:bbbb");

    uint16_t ip6a[] = { 0x0100, 0 ,0 ,0 ,0 ,0 ,0 ,0xff00 };

    memcpy(attr.value.ipaddr.addr.ip6, ip6a, 16);

    meta = sai_metadata_get_attr_metadata(SAI_OBJECT_TYPE_TUNNEL, attr.id);

    s = sai_serialize_attr_value(*meta, attr);

    ASSERT_TRUE(s, "1::ff");

    uint16_t ip6b[] = { 0, 0 ,0 ,0 ,0 ,0 ,0 ,0x100 };

    memcpy(attr.value.ipaddr.addr.ip6, ip6b, 16);

    meta = sai_metadata_get_attr_metadata(SAI_OBJECT_TYPE_TUNNEL, attr.id);

    s = sai_serialize_attr_value(*meta, attr);

    ASSERT_TRUE(s, "::1");

    try
    {
        // invalid address family
        int k = 100;
        attr.value.ipaddr.addr_family = (sai_ip_addr_family_t)k;

        s = sai_serialize_attr_value(*meta, attr);

        ASSERT_FAIL("invalid family not throw exception");
    }
    catch (const std::runtime_error &e)
    {
        // ok
    }

    // deserialize

    sai_deserialize_attr_value("10.0.0.23", *meta, attr);

    ASSERT_TRUE(attr.value.ipaddr.addr.ip4, htonl(0x0a000017));
    ASSERT_TRUE(attr.value.ipaddr.addr_family, SAI_IP_ADDR_FAMILY_IPV4);

    sai_deserialize_attr_value("1::ff", *meta, attr);

    ASSERT_TRUE(0, memcmp(attr.value.ipaddr.addr.ip6, ip6a, 16));
    ASSERT_TRUE(attr.value.ipaddr.addr_family, SAI_IP_ADDR_FAMILY_IPV6);

    try
    {
        sai_deserialize_attr_value("foo", *meta, attr);
        ASSERT_FAIL("invalid s32 deserialize failed to throw exception");
    }
    catch (const std::runtime_error& e)
    {
        // ok
    }
}

void test_serialize_uint32_list()
{
    SWSS_LOG_ENTER();

    clear_local();
    meta_init_db();

    sai_attribute_t attr;
    const sai_attr_metadata_t* meta;
    std::string s;

    attr.id = SAI_PORT_ATTR_SUPPORTED_SPEED; //SAI_PORT_ATTR_SUPPORTED_HALF_DUPLEX_SPEED;

    uint32_t list[] = {1,2,3,4,5,6,7};

    attr.value.u32list.count = 7;
    attr.value.u32list.list = NULL;

    meta = sai_metadata_get_attr_metadata(SAI_OBJECT_TYPE_PORT, attr.id);

    s = sai_serialize_attr_value(*meta, attr);

    ASSERT_TRUE(s, "7:null");

    attr.value.u32list.list = list;

    meta = sai_metadata_get_attr_metadata(SAI_OBJECT_TYPE_PORT, attr.id);

    s = sai_serialize_attr_value(*meta, attr);

    ASSERT_TRUE(s, "7:1,2,3,4,5,6,7");

    attr.value.u32list.count = 0;
    attr.value.u32list.list = list;

    meta = sai_metadata_get_attr_metadata(SAI_OBJECT_TYPE_PORT, attr.id);

    s = sai_serialize_attr_value(*meta, attr);

    ASSERT_TRUE(s, "0:null");

    attr.value.u32list.count = 0;
    attr.value.u32list.list = NULL;

    meta = sai_metadata_get_attr_metadata(SAI_OBJECT_TYPE_PORT, attr.id);

    s = sai_serialize_attr_value(*meta, attr);

    ASSERT_TRUE(s, "0:null");

    memset(&attr, 0, sizeof(attr));

    sai_deserialize_attr_value("7:1,2,3,4,5,6,7", *meta, attr, false);

    ASSERT_TRUE(attr.value.u32list.count, 7);
    ASSERT_TRUE(attr.value.u32list.list[0], 1);
    ASSERT_TRUE(attr.value.u32list.list[1], 2);
    ASSERT_TRUE(attr.value.u32list.list[2], 3);
    ASSERT_TRUE(attr.value.u32list.list[3], 4);
}

void test_serialize_enum_list()
{
    SWSS_LOG_ENTER();

    clear_local();
    meta_init_db();

    sai_attribute_t attr;
    const sai_attr_metadata_t* meta;
    std::string s;

    attr.id = SAI_HASH_ATTR_NATIVE_HASH_FIELD_LIST;

    int32_t list[] = {
             SAI_NATIVE_HASH_FIELD_SRC_IP,
             SAI_NATIVE_HASH_FIELD_DST_IP,
             SAI_NATIVE_HASH_FIELD_VLAN_ID,
             77
    };

    attr.value.s32list.count = 4;
    attr.value.s32list.list = NULL;

    meta = sai_metadata_get_attr_metadata(SAI_OBJECT_TYPE_HASH, attr.id);

    s = sai_serialize_attr_value(*meta, attr);

    ASSERT_TRUE(s, "4:null");

    attr.value.s32list.list = list;

    meta = sai_metadata_get_attr_metadata(SAI_OBJECT_TYPE_HASH, attr.id);

    s = sai_serialize_attr_value(*meta, attr);

    // or for enum: 4:SAI_NATIVE_HASH_FIELD[SRC_IP,DST_IP,VLAN_ID,77]

    ASSERT_TRUE(s, "4:SAI_NATIVE_HASH_FIELD_SRC_IP,SAI_NATIVE_HASH_FIELD_DST_IP,SAI_NATIVE_HASH_FIELD_VLAN_ID,77");

    attr.value.s32list.count = 0;
    attr.value.s32list.list = list;

    meta = sai_metadata_get_attr_metadata(SAI_OBJECT_TYPE_HASH, attr.id);

    s = sai_serialize_attr_value(*meta, attr);

    ASSERT_TRUE(s, "0:null");

    attr.value.s32list.count = 0;
    attr.value.s32list.list = NULL;

    meta = sai_metadata_get_attr_metadata(SAI_OBJECT_TYPE_HASH, attr.id);

    s = sai_serialize_attr_value(*meta, attr);

    ASSERT_TRUE(s, "0:null");
}

void test_serialize_oid()
{
    SWSS_LOG_ENTER();

    clear_local();
    meta_init_db();

    sai_attribute_t attr;
    const sai_attr_metadata_t* meta;
    std::string s;

    attr.id = SAI_SWITCH_ATTR_DEFAULT_VIRTUAL_ROUTER_ID;

    attr.value.oid = 0x1234567890abcdef;

    meta = sai_metadata_get_attr_metadata(SAI_OBJECT_TYPE_SWITCH, attr.id);

    s = sai_serialize_attr_value(*meta, attr);

    ASSERT_TRUE(s, "oid:0x1234567890abcdef");

    // deserialize

    sai_deserialize_attr_value("oid:0x1234567890abcdef", *meta, attr);

    ASSERT_TRUE(0x1234567890abcdef, attr.value.oid);

    try
    {
        sai_deserialize_attr_value("foo", *meta, attr);
        ASSERT_FAIL("invalid oid deserialize failed to throw exception");
    }
    catch (const std::runtime_error& e)
    {
        // ok
    }
}

void test_serialize_oid_list()
{
    SWSS_LOG_ENTER();

    clear_local();
    meta_init_db();

    sai_attribute_t attr;
    const sai_attr_metadata_t* meta;
    std::string s;

    attr.id = SAI_SWITCH_ATTR_PORT_LIST;

    sai_object_id_t list[] = {
        1,0x42, 0x77
    };

    attr.value.objlist.count = 3;
    attr.value.objlist.list = NULL;

    meta = sai_metadata_get_attr_metadata(SAI_OBJECT_TYPE_SWITCH, attr.id);

    s = sai_serialize_attr_value(*meta, attr);

    ASSERT_TRUE(s, "3:null");

    attr.value.objlist.list = list;

    meta = sai_metadata_get_attr_metadata(SAI_OBJECT_TYPE_SWITCH, attr.id);

    s = sai_serialize_attr_value(*meta, attr);

    // or: 4:[ROUTE:0x1,PORT:0x3,oid:0x77] if we have query function

    ASSERT_TRUE(s, "3:oid:0x1,oid:0x42,oid:0x77");

    attr.value.objlist.count = 0;
    attr.value.objlist.list = list;

    meta = sai_metadata_get_attr_metadata(SAI_OBJECT_TYPE_SWITCH, attr.id);

    s = sai_serialize_attr_value(*meta, attr);

    ASSERT_TRUE(s, "0:null");

    attr.value.objlist.count = 0;
    attr.value.objlist.list = NULL;

    meta = sai_metadata_get_attr_metadata(SAI_OBJECT_TYPE_SWITCH, attr.id);

    s = sai_serialize_attr_value(*meta, attr);

    ASSERT_TRUE(s, "0:null");

    memset(&attr, 0, sizeof(attr));

    // deserialize

    sai_deserialize_attr_value("3:oid:0x1,oid:0x42,oid:0x77", *meta, attr, false);

    ASSERT_TRUE(attr.value.objlist.count, 3);
    ASSERT_TRUE(attr.value.objlist.list[0], 0x1);
    ASSERT_TRUE(attr.value.objlist.list[1], 0x42);
    ASSERT_TRUE(attr.value.objlist.list[2], 0x77);
}

void test_serialize_acl_action()
{
    SWSS_LOG_ENTER();

    clear_local();
    meta_init_db();

    sai_attribute_t attr;
    const sai_attr_metadata_t* meta;
    std::string s;

    attr.id = SAI_ACL_ENTRY_ATTR_ACTION_REDIRECT;

    attr.value.aclaction.enable = true;
    attr.value.aclaction.parameter.oid = (sai_object_id_t)2;

    meta = sai_metadata_get_attr_metadata(SAI_OBJECT_TYPE_ACL_ENTRY, attr.id);

    s = sai_serialize_attr_value(*meta, attr);

    ASSERT_TRUE(s, "oid:0x2");

    attr.value.aclaction.enable = false;
    attr.value.aclaction.parameter.oid = (sai_object_id_t)2;

    meta = sai_metadata_get_attr_metadata(SAI_OBJECT_TYPE_ACL_ENTRY, attr.id);

    s = sai_serialize_attr_value(*meta, attr);

    ASSERT_TRUE(s, "disabled");

    attr.id = SAI_ACL_ENTRY_ATTR_ACTION_PACKET_ACTION;

    attr.value.aclaction.enable = true;
    attr.value.aclaction.parameter.s32 = SAI_PACKET_ACTION_TRAP;

    meta = sai_metadata_get_attr_metadata(SAI_OBJECT_TYPE_ACL_ENTRY, attr.id);

    s = sai_serialize_attr_value(*meta, attr);

    ASSERT_TRUE(s, "SAI_PACKET_ACTION_TRAP");

    attr.value.aclaction.enable = true;
    attr.value.aclaction.parameter.s32 = 77;

    meta = sai_metadata_get_attr_metadata(SAI_OBJECT_TYPE_ACL_ENTRY, attr.id);

    s = sai_serialize_attr_value(*meta, attr);

    ASSERT_TRUE(s, "77");
}

void test_serialize_qos_map()
{
    SWSS_LOG_ENTER();

    clear_local();
    meta_init_db();

    sai_attribute_t attr;
    const sai_attr_metadata_t* meta;
    std::string s;

    attr.id = SAI_QOS_MAP_ATTR_MAP_TO_VALUE_LIST;

    sai_qos_map_t qm = {
        .key   = { .tc = 1, .dscp = 2, .dot1p = 3, .prio = 4, .pg = 5, .queue_index = 6, .color = SAI_PACKET_COLOR_RED },
        .value = { .tc = 11, .dscp = 22, .dot1p = 33, .prio = 44, .pg = 55, .queue_index = 66, .color = SAI_PACKET_COLOR_GREEN } };

    attr.value.qosmap.count = 1;
    attr.value.qosmap.list = &qm;

    meta = sai_metadata_get_attr_metadata(SAI_OBJECT_TYPE_QOS_MAP, attr.id);

    s = sai_serialize_attr_value(*meta, attr);

    std::string ret = "{\"count\":1,\"list\":[{\"key\":{\"color\":\"SAI_PACKET_COLOR_RED\",\"dot1p\":3,\"dscp\":2,\"pg\":5,\"prio\":4,\"qidx\":6,\"tc\":1},\"value\":{\"color\":\"SAI_PACKET_COLOR_GREEN\",\"dot1p\":33,\"dscp\":22,\"pg\":55,\"prio\":44,\"qidx\":66,\"tc\":11}}]}";

    ASSERT_TRUE(s, ret);

    s = sai_serialize_attr_value(*meta, attr, true);

    std::string ret2 = "{\"count\":1,\"list\":null}";
    ASSERT_TRUE(s, ret2);

    // deserialize

    memset(&attr, 0, sizeof(attr));

    sai_deserialize_attr_value(ret, *meta, attr);

    ASSERT_TRUE(attr.value.qosmap.count, 1);

    auto &l = attr.value.qosmap.list[0];
    ASSERT_TRUE(l.key.tc, 1);
    ASSERT_TRUE(l.key.dscp, 2);
    ASSERT_TRUE(l.key.dot1p, 3);
    ASSERT_TRUE(l.key.prio, 4);
    ASSERT_TRUE(l.key.pg, 5);
    ASSERT_TRUE(l.key.queue_index, 6);
    ASSERT_TRUE(l.key.color, SAI_PACKET_COLOR_RED);

    ASSERT_TRUE(l.value.tc, 11);
    ASSERT_TRUE(l.value.dscp, 22);
    ASSERT_TRUE(l.value.dot1p, 33);
    ASSERT_TRUE(l.value.prio, 44);
    ASSERT_TRUE(l.value.pg, 55);
    ASSERT_TRUE(l.value.queue_index, 66);
    ASSERT_TRUE(l.value.color, SAI_PACKET_COLOR_GREEN);
}

template<typename T>
void deserialize_number(
        _In_ const std::string& s,
        _Out_ T& number,
        _In_ bool hex = false)
{
    SWSS_LOG_ENTER();

    errno = 0;

    char *endptr = NULL;

    number = (T)strtoull(s.c_str(), &endptr, hex ? 16 : 10);

    if (errno != 0 || endptr != s.c_str() + s.length())
    {
        SWSS_LOG_THROW("invalid number %s", s.c_str());
    }
}

template <typename T>
std::string serialize_number(
        _In_ const T& number,
        _In_ bool hex = false)
{
    SWSS_LOG_ENTER();

    if (hex)
    {
        char buf[32];

        snprintf(buf, sizeof(buf), "0x%lx", (uint64_t)number);

        return buf;
    }

    return std::to_string(number);
}

void test_numbers()
{
    SWSS_LOG_ENTER();

    int64_t sp =  0x12345678;
    int64_t sn = -0x12345678;
    int64_t u  =  0x12345678;

    auto ssp = serialize_number(sp);
    auto ssn = serialize_number(sn);
    auto su  = serialize_number(u);

    ASSERT_TRUE(ssp, std::to_string(sp));
    ASSERT_TRUE(ssn, std::to_string(sn));
    ASSERT_TRUE(su,  std::to_string(u));

    auto shsp = serialize_number(sp, true);
    auto shsn = serialize_number(sn, true);
    auto shu  = serialize_number(u,  true);

    ASSERT_TRUE(shsp, "0x12345678");
    ASSERT_TRUE(shsn, "0xffffffffedcba988");
    ASSERT_TRUE(shu,  "0x12345678");

    sp = 0;
    sn = 0;
    u  = 0;

    deserialize_number(ssp, sp);
    deserialize_number(ssn, sn);
    deserialize_number(su,  u);

    ASSERT_TRUE(sp,  0x12345678);
    ASSERT_TRUE(sn, -0x12345678);
    ASSERT_TRUE(u,   0x12345678);

    deserialize_number(shsp, sp, true);
    deserialize_number(shsn, sn, true);
    deserialize_number(shu,  u,  true);

    ASSERT_TRUE(sp,  0x12345678);
    ASSERT_TRUE(sn, -0x12345678);
    ASSERT_TRUE(u,   0x12345678);
}

int main()
{
    swss::Logger::getInstance().setMinPrio(swss::Logger::SWSS_DEBUG);

    SWSS_LOG_ENTER();

    // serialize tests

    test_serialize_bool();
    test_serialize_chardata();
    test_serialize_uint64();
    test_serialize_enum();
    test_serialize_mac();
    test_serialize_ip_address();
    test_serialize_uint32_list();
    test_serialize_enum_list();
    test_serialize_oid();
    test_serialize_oid_list();
    test_serialize_acl_action();
    test_serialize_qos_map();

    // attributes tests

    test_switch_set();
    test_switch_get();

    test_fdb_entry_create();
    test_fdb_entry_remove();
    test_fdb_entry_set();
    test_fdb_entry_get();
    test_fdb_entry_flow();

    test_neighbor_entry_create();
    test_neighbor_entry_remove();
    test_neighbor_entry_set();
    test_neighbor_entry_get();
    test_neighbor_entry_flow();

    test_vlan_create();
    test_vlan_remove();
    test_vlan_set();
    test_vlan_get();
    test_vlan_flow();

    test_route_entry_create();
    test_route_entry_remove();
    test_route_entry_set();
    test_route_entry_get();
    test_route_entry_flow();

    test_serialization_type_vlan_list();
    test_serialization_type_bool();
    test_serialization_type_char();
    test_serialization_type_int32_list();
    test_serialization_type_uint32_list();

    test_mask();
    test_acl_entry_field_and_action();
    test_construct_key();
    test_queue_create();
    test_null_list();

    test_numbers();

    test_priority_group();

    std::cout << "SUCCESS" << std::endl;
}
