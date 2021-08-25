#include "TestLegacy.h"
#include "AttrKeyMap.h"

#include "sai_serialize.h"

#include <arpa/inet.h>
#include <inttypes.h>

#include <gtest/gtest.h>

#include <memory>

using namespace TestLegacy;
using namespace saimeta;

// STATIC HELPERS

static sai_object_id_t create_acl_table(
        _In_ sai_object_id_t switch_id)
{
    SWSS_LOG_ENTER();

    sai_object_id_t at;

    sai_attribute_t attrs[9] = { };

    attrs[0].id = SAI_ACL_TABLE_ATTR_ACL_STAGE;
    attrs[0].value.s32 = SAI_ACL_STAGE_INGRESS;

    auto status = g_meta->create(SAI_OBJECT_TYPE_ACL_TABLE, &at, switch_id, 1, attrs);
    EXPECT_EQ(SAI_STATUS_SUCCESS, status);

    return at;
}

static sai_object_id_t create_acl_counter(
        _In_ sai_object_id_t switch_id)
{
    SWSS_LOG_ENTER();

    sai_object_id_t ac;

    sai_attribute_t attrs[9] = { };

    attrs[0].id = SAI_ACL_COUNTER_ATTR_TABLE_ID;
    attrs[0].value.oid = create_acl_table(switch_id);

    auto status = g_meta->create(SAI_OBJECT_TYPE_ACL_COUNTER, &ac, switch_id, 1, attrs);
    EXPECT_EQ(SAI_STATUS_SUCCESS, status);

    return ac;
}

static sai_object_id_t create_policer(
        _In_ sai_object_id_t switch_id)
{
    SWSS_LOG_ENTER();

    sai_object_id_t policer;

    sai_attribute_t attrs[9] = { };

    attrs[0].id = SAI_POLICER_ATTR_METER_TYPE;
    attrs[0].value.s32 = SAI_METER_TYPE_PACKETS;
    attrs[1].id = SAI_POLICER_ATTR_MODE;
    attrs[1].value.s32 = SAI_POLICER_MODE_SR_TCM;

    auto status = g_meta->create(SAI_OBJECT_TYPE_POLICER, &policer, switch_id, 2, attrs);
    EXPECT_EQ(SAI_STATUS_SUCCESS, status);

    return policer;
}

static sai_object_id_t create_samplepacket(
        _In_ sai_object_id_t switch_id)
{
    SWSS_LOG_ENTER();

    sai_object_id_t sp;

    sai_attribute_t attrs[9] = { };

    attrs[0].id = SAI_SAMPLEPACKET_ATTR_SAMPLE_RATE;
    attrs[0].value.u32 = 1;

    auto status = g_meta->create(SAI_OBJECT_TYPE_SAMPLEPACKET, &sp, switch_id, 1, attrs);
    EXPECT_EQ(SAI_STATUS_SUCCESS, status);

    return sp;
}

static sai_object_id_t create_hostif_udt(
        _In_ sai_object_id_t switch_id)
{
    SWSS_LOG_ENTER();

    sai_object_id_t udt;

    sai_attribute_t attrs[9] = { };

    attrs[0].id = SAI_HOSTIF_USER_DEFINED_TRAP_ATTR_TYPE;
    attrs[0].value.s32 = SAI_HOSTIF_USER_DEFINED_TRAP_TYPE_ROUTER;

    auto status = g_meta->create(SAI_OBJECT_TYPE_HOSTIF_USER_DEFINED_TRAP, &udt, switch_id, 1, attrs);
    EXPECT_EQ(SAI_STATUS_SUCCESS, status);

    return udt;
}

static sai_object_id_t create_ipg(
        _In_ sai_object_id_t switch_id)
{
    SWSS_LOG_ENTER();

    sai_object_id_t ipg;

    sai_attribute_t attrs[9] = { };

    attrs[0].id = SAI_INGRESS_PRIORITY_GROUP_ATTR_PORT;
    attrs[0].value.oid = create_port(switch_id);
    attrs[1].id = SAI_INGRESS_PRIORITY_GROUP_ATTR_INDEX;
    attrs[1].value.u8 = 0;

    auto status = g_meta->create(SAI_OBJECT_TYPE_INGRESS_PRIORITY_GROUP, &ipg, switch_id, 2, attrs);
    EXPECT_EQ(SAI_STATUS_SUCCESS, status);

    return ipg;
}

static sai_object_id_t insert_dummy_object(
        _In_ sai_object_type_t ot,
        _In_ sai_object_id_t switch_id)
{
    SWSS_LOG_ENTER();

    switch (ot)
    {
        case SAI_OBJECT_TYPE_PORT:
            return create_port(switch_id);

        case SAI_OBJECT_TYPE_ACL_TABLE:
            return create_acl_table(switch_id);

        case SAI_OBJECT_TYPE_ACL_COUNTER:
            return create_acl_counter(switch_id);

        case SAI_OBJECT_TYPE_POLICER:
            return create_policer(switch_id);

        case SAI_OBJECT_TYPE_SAMPLEPACKET:
            return create_samplepacket(switch_id);

        case SAI_OBJECT_TYPE_HOSTIF_USER_DEFINED_TRAP:
            return create_hostif_udt(switch_id);

        case SAI_OBJECT_TYPE_INGRESS_PRIORITY_GROUP:
            return create_ipg(switch_id);

        default:

            SWSS_LOG_THROW("not implemented: %s, FIXME", sai_serialize_object_type(ot).c_str());
    }
}

static sai_object_id_t create_scheduler_group(
        _In_ sai_object_id_t switch_id)
{
    SWSS_LOG_ENTER();

    sai_object_id_t sg;

    sai_attribute_t attrs[9] = { };

    attrs[0].id = SAI_SCHEDULER_GROUP_ATTR_PORT_ID;
    attrs[0].value.oid = create_port(switch_id);
    attrs[1].id = SAI_SCHEDULER_GROUP_ATTR_LEVEL;
    attrs[1].value.u8 = 0;
    attrs[2].id = SAI_SCHEDULER_GROUP_ATTR_MAX_CHILDS;
    attrs[2].value.u8 = 1;
    attrs[3].id = SAI_SCHEDULER_GROUP_ATTR_PARENT_NODE;
    attrs[3].value.oid = attrs[0].value.oid;

    auto status = g_meta->create(SAI_OBJECT_TYPE_SCHEDULER_GROUP, &sg, switch_id, 4, attrs);
    EXPECT_EQ(SAI_STATUS_SUCCESS, status);

    return sg;
}



// ACTUAL TESTS

TEST(Legacy, switch_set)
{
    SWSS_LOG_ENTER();

    clear_local();

    sai_status_t    status;
    sai_attribute_t attr;

    sai_object_id_t switch_id = create_switch();

    status = g_meta->set(SAI_OBJECT_TYPE_SWITCH, switch_id, NULL);
    EXPECT_NE(SAI_STATUS_SUCCESS, status);

    status = g_meta->set(SAI_OBJECT_TYPE_SWITCH, switch_id, &attr);
    EXPECT_NE(SAI_STATUS_SUCCESS, status);

    // id outside range
    attr.id = -1;
    status = g_meta->set(SAI_OBJECT_TYPE_SWITCH, switch_id, &attr);
    EXPECT_NE(SAI_STATUS_SUCCESS, status);

    g_sai->setStatus(SAI_STATUS_FAILURE);
    attr.id = SAI_SWITCH_ATTR_PORT_NUMBER;
    status = g_meta->set(SAI_OBJECT_TYPE_SWITCH, switch_id, &attr);
    EXPECT_NE(SAI_STATUS_SUCCESS, status);
    g_sai->setStatus(SAI_STATUS_SUCCESS);

    attr.id = SAI_SWITCH_ATTR_PORT_NUMBER;
    status = g_meta->set(SAI_OBJECT_TYPE_SWITCH, switch_id, &attr);
    EXPECT_NE(SAI_STATUS_SUCCESS, status);

    attr.id = SAI_SWITCH_ATTR_SWITCHING_MODE;
    attr.value.s32 = 0x1000;
    status = g_meta->set(SAI_OBJECT_TYPE_SWITCH, switch_id, &attr);
    EXPECT_NE(SAI_STATUS_SUCCESS, status);

    // enum
    attr.id = SAI_SWITCH_ATTR_SWITCHING_MODE;
    attr.value.s32 = SAI_SWITCH_SWITCHING_MODE_STORE_AND_FORWARD;
    status = g_meta->set(SAI_OBJECT_TYPE_SWITCH, switch_id, &attr);
    EXPECT_EQ(SAI_STATUS_SUCCESS, status);

    // bool
    attr.id = SAI_SWITCH_ATTR_BCAST_CPU_FLOOD_ENABLE;
    attr.value.booldata = SAI_SWITCH_SWITCHING_MODE_STORE_AND_FORWARD;
    status = g_meta->set(SAI_OBJECT_TYPE_SWITCH, switch_id, &attr);
    EXPECT_EQ(SAI_STATUS_SUCCESS, status);

    // mac
    sai_mac_t mac = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66};

    attr.id = SAI_SWITCH_ATTR_SRC_MAC_ADDRESS;
    memcpy(attr.value.mac, mac, sizeof(mac));
    status = g_meta->set(SAI_OBJECT_TYPE_SWITCH, switch_id, &attr);
    EXPECT_EQ(SAI_STATUS_SUCCESS, status);

    // uint8
    attr.id = SAI_SWITCH_ATTR_QOS_DEFAULT_TC;
    attr.value.u8 = 0x11;
    status = g_meta->set(SAI_OBJECT_TYPE_SWITCH, switch_id, &attr);
    EXPECT_EQ(SAI_STATUS_SUCCESS, status);

    // object id with not allowed null

    // currently hash is read only
    //
    //    // null oid

    //    attr.id = SAI_SWITCH_ATTR_LAG_HASH_IPV6;
    //    attr.value.oid = SAI_NULL_OBJECT_ID;
    //    status = g_meta->set(SAI_OBJECT_TYPE_SWITCH, switch_id, &attr);
    //    EXPECT_NE(SAI_STATUS_SUCCESS, status);
    //
    //    // wrong object type
    //    attr.id = SAI_SWITCH_ATTR_LAG_HASH_IPV6;
    //    attr.value.oid = create_dummy_object_id(SAI_OBJECT_TYPE_LAG);
    //    status = g_meta->set(SAI_OBJECT_TYPE_SWITCH, switch_id, &attr);
    //    EXPECT_NE(SAI_STATUS_SUCCESS, status);
    //
    //    // valid object (object must exist)
    //    attr.id = SAI_SWITCH_ATTR_LAG_HASH_IPV6;
    //    attr.value.oid = create_hash(switch_id);
    //
    //    status = g_meta->set(SAI_OBJECT_TYPE_SWITCH, switch_id, &attr);
    //    EXPECT_EQ(SAI_STATUS_SUCCESS, status);
    //
    //    EXPECT_TRUE(g_meta->getObjectReferenceCount(attr.value.oid) == 1);

    // object id with allowed null

    // null oid
    attr.id = SAI_SWITCH_ATTR_QOS_DOT1P_TO_TC_MAP;
    attr.value.oid = SAI_NULL_OBJECT_ID;
    status = g_meta->set(SAI_OBJECT_TYPE_SWITCH, switch_id, &attr);
    EXPECT_EQ(SAI_STATUS_SUCCESS, status);

    // wrong object
    attr.id = SAI_SWITCH_ATTR_QOS_DOT1P_TO_TC_MAP;
    status = g_meta->create(SAI_OBJECT_TYPE_LAG, &attr.value.oid, switch_id, 0, NULL);
    EXPECT_EQ(SAI_STATUS_SUCCESS, status);

    status = g_meta->set(SAI_OBJECT_TYPE_SWITCH, switch_id, &attr);
    EXPECT_NE(SAI_STATUS_SUCCESS, status);

    // good object
    attr.id = SAI_SWITCH_ATTR_QOS_DOT1P_TO_TC_MAP;

    sai_attribute_t a[2] = { };
    a[0].id = SAI_QOS_MAP_ATTR_TYPE;
    a[0].value.s32 = 1;
    a[1].id = SAI_QOS_MAP_ATTR_MAP_TO_VALUE_LIST;
    status = g_meta->create(SAI_OBJECT_TYPE_QOS_MAP, &attr.value.oid, switch_id, 2, a);
    EXPECT_EQ(SAI_STATUS_SUCCESS, status);

    sai_object_id_t oid = attr.value.oid;

    status = g_meta->set(SAI_OBJECT_TYPE_SWITCH, switch_id, &attr);
    EXPECT_EQ(SAI_STATUS_SUCCESS, status);

    EXPECT_TRUE(g_meta->getObjectReferenceCount(attr.value.oid) == 1);

    attr.id = SAI_SWITCH_ATTR_QOS_DOT1P_TO_TC_MAP;
    attr.value.oid = SAI_NULL_OBJECT_ID;
    status = g_meta->set(SAI_OBJECT_TYPE_SWITCH, switch_id, &attr);
    EXPECT_EQ(SAI_STATUS_SUCCESS, status);

    // check if it was decreased
    EXPECT_TRUE(g_meta->getObjectReferenceCount(oid) == 0);

    remove_switch(switch_id);
}

TEST(Legacy, switch_get)
{
    SWSS_LOG_ENTER();

    clear_local();

    sai_status_t    status;
    sai_attribute_t attr;

    sai_object_id_t switch_id = create_switch();

    attr.id = SAI_SWITCH_ATTR_PORT_NUMBER;
    status = g_meta->get(SAI_OBJECT_TYPE_SWITCH, switch_id, 0, &attr);
    EXPECT_NE(SAI_STATUS_SUCCESS, status);

    status = g_meta->get(SAI_OBJECT_TYPE_SWITCH, switch_id, 1000, &attr);
    EXPECT_NE(SAI_STATUS_SUCCESS, status);

    status = g_meta->get(SAI_OBJECT_TYPE_SWITCH, switch_id, 1, NULL);
    EXPECT_NE(SAI_STATUS_SUCCESS, status);

    status = g_meta->get(SAI_OBJECT_TYPE_SWITCH, switch_id, 1, NULL);
    EXPECT_NE(SAI_STATUS_SUCCESS, status);

    status = g_meta->get(SAI_OBJECT_TYPE_SWITCH, switch_id, 1, &attr);
    EXPECT_EQ(SAI_STATUS_SUCCESS, status);

    attr.id = -1;
    status = g_meta->get(SAI_OBJECT_TYPE_SWITCH, switch_id, 1, &attr);
    EXPECT_NE(SAI_STATUS_SUCCESS, status);

    attr.id = SAI_SWITCH_ATTR_PORT_NUMBER;
    attr.value.u32 = 0;
    status = g_meta->get(SAI_OBJECT_TYPE_SWITCH, switch_id, 1, &attr);
    EXPECT_EQ(SAI_STATUS_SUCCESS, status);

    sai_attribute_t attr1;
    attr1.id = SAI_SWITCH_ATTR_PORT_NUMBER;

    sai_attribute_t attr2;
    attr2.id = SAI_SWITCH_ATTR_DEFAULT_VIRTUAL_ROUTER_ID;
    sai_attribute_t list[2] = { attr1, attr2 };

    status = g_meta->get(SAI_OBJECT_TYPE_SWITCH, switch_id, 2, list);
    EXPECT_EQ(SAI_STATUS_SUCCESS, status);

    remove_switch(switch_id);
}

TEST(Legacy, serialization_type_vlan_list)
{
    SWSS_LOG_ENTER();

    clear_local();

    sai_status_t    status;

    SWSS_LOG_NOTICE("create stp");
    sai_object_id_t switch_id = create_switch();

    sai_object_id_t stp = create_stp(switch_id);

    sai_vlan_id_t list[2] = { 1, 2 };

    sai_attribute_t attr;

    attr.id = SAI_STP_ATTR_VLAN_LIST;
    attr.value.vlanlist.count = 2;
    attr.value.vlanlist.list = list;

    SWSS_LOG_NOTICE("set vlan list");

    status = g_meta->set(SAI_OBJECT_TYPE_STP, stp, &attr);
    EXPECT_NE(SAI_STATUS_SUCCESS, status);

    SWSS_LOG_NOTICE("get vlan list");

    status = g_meta->get(SAI_OBJECT_TYPE_STP, stp, 1, &attr);
    EXPECT_EQ(SAI_STATUS_SUCCESS, status);

    SWSS_LOG_NOTICE("remove stp");

    status = g_meta->remove(SAI_OBJECT_TYPE_STP, stp);
    EXPECT_EQ(SAI_STATUS_SUCCESS, status);

    remove_switch(switch_id);
}

TEST(Legacy, serialization_type_bool)
{
    SWSS_LOG_ENTER();

    clear_local();

    sai_status_t    status;

    sai_object_id_t vr;

    SWSS_LOG_NOTICE("create stp");
    sai_object_id_t switch_id = create_switch();

    sai_attribute_t attr;

    attr.id = SAI_VIRTUAL_ROUTER_ATTR_ADMIN_V4_STATE;
    attr.value.booldata = true;

    status = g_meta->create(SAI_OBJECT_TYPE_VIRTUAL_ROUTER, &vr, switch_id, 1, &attr);
    EXPECT_EQ(SAI_STATUS_SUCCESS, status);

    SWSS_LOG_NOTICE("set bool");

    status = g_meta->set(SAI_OBJECT_TYPE_VIRTUAL_ROUTER, vr, &attr);
    EXPECT_EQ(SAI_STATUS_SUCCESS, status);

    SWSS_LOG_NOTICE("get bool");

    status = g_meta->get(SAI_OBJECT_TYPE_VIRTUAL_ROUTER, vr, 1, &attr);
    EXPECT_EQ(SAI_STATUS_SUCCESS, status);

    SWSS_LOG_NOTICE("remove vr");

    status = g_meta->remove(SAI_OBJECT_TYPE_VIRTUAL_ROUTER, vr);
    EXPECT_EQ(SAI_STATUS_SUCCESS, status);

    remove_switch(switch_id);
}

TEST(Legacy, serialization_type_char)
{
    SWSS_LOG_ENTER();

    clear_local();

    sai_status_t    status;

    sai_object_id_t hostif;

    SWSS_LOG_NOTICE("create port");
    sai_object_id_t switch_id = create_switch();

    sai_object_id_t port = create_port(switch_id);

    sai_attribute_t attr, attr2, attr3;

    attr.id = SAI_HOSTIF_ATTR_TYPE;
    attr.value.s32 = SAI_HOSTIF_TYPE_NETDEV;

    attr2.id = SAI_HOSTIF_ATTR_OBJ_ID;
    attr2.value.oid = port;

    attr3.id = SAI_HOSTIF_ATTR_NAME;

    memcpy(attr3.value.chardata, "foo", sizeof("foo"));

    sai_attribute_t list[3] = { attr, attr2, attr3 };

    // TODO we need to support conditions here

    SWSS_LOG_NOTICE("create hostif");

    status = g_meta->create(SAI_OBJECT_TYPE_HOSTIF, &hostif, switch_id, 3, list);
    EXPECT_EQ(SAI_STATUS_SUCCESS, status);

    SWSS_LOG_NOTICE("set char");

    status = g_meta->set(SAI_OBJECT_TYPE_HOSTIF, hostif, &attr3);
    EXPECT_NE(SAI_STATUS_SUCCESS, status);

    SWSS_LOG_NOTICE("get char");

    status = g_meta->get(SAI_OBJECT_TYPE_HOSTIF, hostif, 1, &attr3);
    EXPECT_EQ(SAI_STATUS_SUCCESS, status);

    SWSS_LOG_NOTICE("remove hostif");

    status = g_meta->remove(SAI_OBJECT_TYPE_HOSTIF, hostif);
    EXPECT_EQ(SAI_STATUS_SUCCESS, status);

    attr.id = SAI_HOSTIF_ATTR_TYPE;
    attr.value.s32 = SAI_HOSTIF_TYPE_FD;

    sai_attribute_t list2[1] = { attr };

    SWSS_LOG_NOTICE("create hostif with non mandatory");
    status = g_meta->create(SAI_OBJECT_TYPE_HOSTIF, &hostif, switch_id, 1, list2);
    EXPECT_EQ(SAI_STATUS_SUCCESS, status);

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
    //    status = g_meta->get(SAI_OBJECT_TYPE_HOSTIF, hostif, 1, &attr2);
    //    EXPECT_NE(SAI_STATUS_SUCCESS, status);

    attr.id = SAI_HOSTIF_ATTR_TYPE;
    attr.value.s32 = SAI_HOSTIF_TYPE_NETDEV;

    sai_attribute_t list3[1] = { attr };

    SWSS_LOG_NOTICE("create hostif with mandatory missing");
    status = g_meta->create(SAI_OBJECT_TYPE_HOSTIF, &hostif, switch_id, 1, list3);
    EXPECT_NE(SAI_STATUS_SUCCESS, status);

    remove_switch(switch_id);
}

TEST(Legacy, serialization_type_int32_list)
{
    SWSS_LOG_ENTER();

    clear_local();

    sai_status_t    status;

    sai_object_id_t hash;

    sai_attribute_t attr;

    SWSS_LOG_NOTICE("create hash");
    sai_object_id_t switch_id = create_switch();

    int32_t list[2] =  { SAI_NATIVE_HASH_FIELD_SRC_IP, SAI_NATIVE_HASH_FIELD_VLAN_ID };

    attr.id = SAI_HASH_ATTR_NATIVE_HASH_FIELD_LIST;
    attr.value.s32list.count = 2;
    attr.value.s32list.list = list;

    status = g_meta->create(SAI_OBJECT_TYPE_HASH, &hash, switch_id, 1, &attr);
    EXPECT_EQ(SAI_STATUS_SUCCESS, status);

    SWSS_LOG_NOTICE("set hash");

    status = g_meta->set(SAI_OBJECT_TYPE_HASH, hash, &attr);
    EXPECT_EQ(SAI_STATUS_SUCCESS, status);

    SWSS_LOG_NOTICE("get hash");

    status = g_meta->get(SAI_OBJECT_TYPE_HASH, hash, 1, &attr);
    EXPECT_EQ(SAI_STATUS_SUCCESS, status);

    SWSS_LOG_NOTICE("remove hash");

    status = g_meta->remove(SAI_OBJECT_TYPE_HASH, hash);
    EXPECT_EQ(SAI_STATUS_SUCCESS, status);

    remove_switch(switch_id);
}

TEST(Legacy, test_serialization_type_uint32_list)
{
    SWSS_LOG_ENTER();

    clear_local();

    sai_status_t    status;

    sai_object_id_t hash;

    sai_attribute_t attr;

    SWSS_LOG_NOTICE("create hash");
    sai_object_id_t switch_id = create_switch();

    int32_t list[2] =  { SAI_NATIVE_HASH_FIELD_SRC_IP, SAI_NATIVE_HASH_FIELD_VLAN_ID };

    attr.id = SAI_HASH_ATTR_NATIVE_HASH_FIELD_LIST;
    attr.value.s32list.count = 2;
    attr.value.s32list.list = list;

    status = g_meta->create(SAI_OBJECT_TYPE_HASH, &hash, switch_id, 1, &attr);
    EXPECT_EQ(SAI_STATUS_SUCCESS, status);

    SWSS_LOG_NOTICE("set hash");

    status = g_meta->set(SAI_OBJECT_TYPE_HASH, hash, &attr);
    EXPECT_EQ(SAI_STATUS_SUCCESS, status);

    SWSS_LOG_NOTICE("get hash");

    status = g_meta->get(SAI_OBJECT_TYPE_HASH, hash, 1, &attr);
    EXPECT_EQ(SAI_STATUS_SUCCESS, status);

    SWSS_LOG_NOTICE("remove hash");

    status = g_meta->remove(SAI_OBJECT_TYPE_HASH, hash);
    EXPECT_EQ(SAI_STATUS_SUCCESS, status);

    remove_switch(switch_id);
}

TEST(Legacy, mask)
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

    EXPECT_TRUE(Meta::is_ipv6_mask_valid(ip6mask1));
    EXPECT_TRUE(Meta::is_ipv6_mask_valid(ip6mask2));
    EXPECT_TRUE(Meta::is_ipv6_mask_valid(ip6mask3));
    EXPECT_TRUE(Meta::is_ipv6_mask_valid(ip6mask4));
    EXPECT_TRUE(Meta::is_ipv6_mask_valid(ip6mask5));

    EXPECT_TRUE(!Meta::is_ipv6_mask_valid(ip6mask6));
    EXPECT_TRUE(!Meta::is_ipv6_mask_valid(ip6mask7));
    EXPECT_TRUE(!Meta::is_ipv6_mask_valid(ip6mask8));
    EXPECT_TRUE(!Meta::is_ipv6_mask_valid(ip6mask9));
}

TEST(Legacy, acl_entry_field_and_action)
{
    SWSS_LOG_ENTER();

    clear_local();

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

            SWSS_LOG_NOTICE("0x%" PRIx64, list[0]);

            attr.value.aclaction.parameter.objlist.count = 1;
            attr.value.aclaction.parameter.objlist.list = list;
        }

        vattrs.push_back(attr);
    }

    SWSS_LOG_NOTICE("create acl entry");

    status = g_meta->create(SAI_OBJECT_TYPE_ACL_ENTRY, &aclentry, switch_id, (uint32_t)vattrs.size(), vattrs.data());
    EXPECT_EQ(SAI_STATUS_SUCCESS, status);

    for (uint32_t i = 0; i < sizeof(ids)/sizeof(int32_t); ++i)
    {
        if (vattrs[i].id == SAI_ACL_ENTRY_ATTR_TABLE_ID)
            continue;

        auto m = sai_metadata_get_attr_metadata(SAI_OBJECT_TYPE_ACL_ENTRY, vattrs[i].id);

        SWSS_LOG_NOTICE("set aclentry %u %s", i, m->attridname);

        status = g_meta->set(SAI_OBJECT_TYPE_ACL_ENTRY, aclentry, &vattrs[i]);
        EXPECT_EQ(SAI_STATUS_SUCCESS, status);
    }

    SWSS_LOG_NOTICE("get aclentry");

    status = g_meta->get(SAI_OBJECT_TYPE_ACL_ENTRY, aclentry, (uint32_t)vattrs.size(), vattrs.data());
    EXPECT_EQ(SAI_STATUS_SUCCESS, status);

    SWSS_LOG_NOTICE("remove aclentry");

    status = g_meta->remove(SAI_OBJECT_TYPE_ACL_ENTRY, aclentry);
    EXPECT_EQ(SAI_STATUS_SUCCESS, status);

    remove_switch(switch_id);
}

TEST(Legacy, construct_key)
{
    SWSS_LOG_ENTER();

    clear_local();

    sai_attribute_t attr;

    uint32_t list[4] = {1,2,3,4};

    attr.id = SAI_PORT_ATTR_HW_LANE_LIST;
    attr.value.u32list.count = 4;
    attr.value.u32list.list = list;

    sai_object_meta_key_t meta_key;

    meta_key.objecttype = SAI_OBJECT_TYPE_PORT;

    sai_object_id_t switchId = 0x21000000000000;

    std::string key = AttrKeyMap::constructKey(switchId, meta_key, 1, &attr);

    SWSS_LOG_NOTICE("constructed key: %s", key.c_str());

    EXPECT_TRUE(key == "oid:0x21000000000000;SAI_PORT_ATTR_HW_LANE_LIST:1,2,3,4;");
}

TEST(Legacy, queue_create)
{
    SWSS_LOG_ENTER();

    clear_local();

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
    attr3.value.oid = create_port(switch_id);

    attr4.id = SAI_QUEUE_ATTR_PARENT_SCHEDULER_NODE;
    attr4.value.oid = create_scheduler_group(switch_id);

    sai_attribute_t list[4] = { attr1, attr2, attr3, attr4 };

    SWSS_LOG_NOTICE("create tests");

    SWSS_LOG_NOTICE("create queue");
    status = g_meta->create(SAI_OBJECT_TYPE_QUEUE, &queue, switch_id, 4, list);
    EXPECT_EQ(SAI_STATUS_SUCCESS, status);

    SWSS_LOG_NOTICE("create queue but key exists");
    status = g_meta->create(SAI_OBJECT_TYPE_QUEUE, &queue, switch_id, 4, list);
    EXPECT_NE(SAI_STATUS_SUCCESS, status);

    SWSS_LOG_NOTICE("remove queue");
    status = g_meta->remove(SAI_OBJECT_TYPE_QUEUE, queue);
    EXPECT_EQ(SAI_STATUS_SUCCESS, status);

    SWSS_LOG_NOTICE("create queue");
    status = g_meta->create(SAI_OBJECT_TYPE_QUEUE, &queue, switch_id, 4, list);
    EXPECT_EQ(SAI_STATUS_SUCCESS, status);

    remove_switch(switch_id);
}

TEST(Legacy, null_list)
{
    SWSS_LOG_ENTER();

    clear_local();

    sai_status_t    status;

    sai_object_id_t hash;

    sai_attribute_t attr;

    int32_t list[2] =  { SAI_NATIVE_HASH_FIELD_SRC_IP, SAI_NATIVE_HASH_FIELD_VLAN_ID };

    attr.id = SAI_HASH_ATTR_NATIVE_HASH_FIELD_LIST;
    attr.value.s32list.count = 0;
    attr.value.s32list.list = list;
    sai_object_id_t switch_id = create_switch();

    SWSS_LOG_NOTICE("0 count, not null list");
    status = g_meta->create(SAI_OBJECT_TYPE_HASH, &hash, switch_id, 1, &attr);
    EXPECT_NE(SAI_STATUS_SUCCESS, status);

    attr.value.s32list.list = NULL;

    SWSS_LOG_NOTICE("0 count, null list");
    status = g_meta->create(SAI_OBJECT_TYPE_HASH, &hash, switch_id, 1, &attr);
    EXPECT_EQ(SAI_STATUS_SUCCESS, status);

    remove_switch(switch_id);
}

TEST(Legacy, priority_group)
{
    SWSS_LOG_ENTER();

    clear_local();

    sai_object_id_t switch_id = create_switch();

    sai_status_t status;

    sai_attribute_t attr;

    sai_object_id_t pg = insert_dummy_object(SAI_OBJECT_TYPE_INGRESS_PRIORITY_GROUP, switch_id);

    SWSS_LOG_NOTICE("set SAI_INGRESS_PRIORITY_GROUP_ATTR_BUFFER_PROFILE attr");

    attr.id = SAI_INGRESS_PRIORITY_GROUP_ATTR_BUFFER_PROFILE;
    attr.value.oid = SAI_NULL_OBJECT_ID;
    status = g_meta->set(SAI_OBJECT_TYPE_INGRESS_PRIORITY_GROUP, pg, &attr);
    EXPECT_EQ(SAI_STATUS_SUCCESS, status);

    remove_switch(switch_id);
}

TEST(Legacy, bulk_route_entry_create)
{
    SWSS_LOG_ENTER();

    clear_local();

    int object_count = 1000;

    std::vector<sai_route_entry_t> routes;
    std::vector<uint32_t> attr_counts;
    std::vector<const sai_attribute_t*> attr_lists;
    std::vector<sai_status_t> statuses(object_count);

    sai_object_id_t switch_id = create_switch();

    sai_object_id_t vr = create_virtual_router(switch_id);
    sai_object_id_t hop = create_next_hop(switch_id);

    sai_attribute_t attr;

    attr.id = SAI_ROUTE_ENTRY_ATTR_NEXT_HOP_ID;
    attr.value.oid = hop;

    int n = 100;

    for (int i = 0; i < object_count * n; i++)
    {
        sai_route_entry_t re;

        memset(re.destination.mask.ip6, 0xff, sizeof(re.destination.mask.ip6));
        re.destination.addr_family = SAI_IP_ADDR_FAMILY_IPV4;
        re.destination.addr.ip4 = htonl(0x0a00000f + i);
        re.destination.mask.ip4 = htonl(0xffffff00);
        re.vr_id = vr;
        re.switch_id = switch_id;

        routes.push_back(re);

        attr_counts.push_back(1);

        // since we use the same attribute every where we can get away with this

        attr_lists.push_back(&attr);
    }

    auto start = std::chrono::high_resolution_clock::now();

    if (getenv("TEST_NO_PERF"))
    {
        object_count = 10;

        std::cout << "disabling performance tests" << std::endl;
    }


    for (int i = 0; i < n; i++)
    {
        auto status = g_meta->bulkCreate(
                object_count,
                routes.data() + i * object_count,
                attr_counts.data(),
                attr_lists.data(),
                SAI_BULK_OP_ERROR_MODE_IGNORE_ERROR,
                statuses.data());

        EXPECT_EQ(status, SAI_STATUS_SUCCESS);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto time = end - start;
    auto us = std::chrono::duration_cast<std::chrono::microseconds>(time);

    std::cout << "ms: " << (double)us.count()/1000 << " / " << n << "/" << object_count << std::endl;
}

