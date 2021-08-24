#include "TestLegacy.h"

#include <gtest/gtest.h>

#include <memory>

using namespace TestLegacy;

// STATIC HELPERS

static sai_fdb_entry_t create_fdb_entry()
{
    SWSS_LOG_ENTER();

    sai_fdb_entry_t fdb_entry;

    sai_mac_t mac = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66};

    memcpy(fdb_entry.mac_address, mac, sizeof(mac));

    auto switch_id = create_switch();

    fdb_entry.switch_id = switch_id;
    sai_object_id_t bridge_id = create_bridge(switch_id);
    fdb_entry.bv_id = bridge_id;

    sai_object_id_t port = create_bridge_port(switch_id, bridge_id);

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

    sai_attribute_t list2[4] = { attr1, attr2, attr3, attr3 };

    auto status = g_meta->create(&fdb_entry, 3, list2);
    EXPECT_EQ(SAI_STATUS_SUCCESS, status);

    return fdb_entry;
}

// TESTS

TEST(LegacyFdbEntry, fdb_entry_create)
{
    SWSS_LOG_ENTER();

    clear_local();

    sai_status_t    status;
    sai_attribute_t attr;

    sai_object_id_t switch_id = create_switch();

    sai_fdb_entry_t fdb_entry;

    sai_mac_t mac = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66};

    memcpy(fdb_entry.mac_address, mac, sizeof(mac));

    fdb_entry.switch_id = switch_id;
    sai_object_id_t bridge_id = create_bridge(switch_id);
    fdb_entry.bv_id = bridge_id;

    sai_object_id_t port = create_bridge_port(switch_id, bridge_id);

    SWSS_LOG_NOTICE("create tests");

    SWSS_LOG_NOTICE("zero attribute count (but there are mandatory attributes)");
    attr.id = SAI_FDB_ENTRY_ATTR_TYPE;
    status = g_meta->create(&fdb_entry, 0, &attr);
    EXPECT_NE(SAI_STATUS_SUCCESS, status);

    SWSS_LOG_NOTICE("attr is null");
    status = g_meta->create(&fdb_entry, 1, NULL);
    EXPECT_NE(SAI_STATUS_SUCCESS, status);

    SWSS_LOG_NOTICE("fdb entry is null");
    status = g_meta->create((const sai_fdb_entry_t*)NULL, 1, &attr);
    EXPECT_NE(SAI_STATUS_SUCCESS, status);

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

    SWSS_LOG_NOTICE("invalid attribute id");
    status = g_meta->create(&fdb_entry, 4, list1);
    EXPECT_NE(SAI_STATUS_SUCCESS, status);

//    // packet action is now optional
//    SWSS_LOG_NOTICE("passing optional attribute");
//    status = g_meta->create(&fdb_entry, 1, list1);
//    EXPECT_EQ(SAI_STATUS_SUCCESS, status);

    attr2.value.oid = create_dummy_object_id(SAI_OBJECT_TYPE_HASH, switch_id);

    SWSS_LOG_NOTICE("invalid attribute value on oid");
    status = g_meta->create(&fdb_entry, 3, list1);
    EXPECT_NE(SAI_STATUS_SUCCESS, status);

    attr2.value.oid = create_dummy_object_id(SAI_OBJECT_TYPE_PORT, switch_id);

    SWSS_LOG_NOTICE("non existing object on oid");
    status = g_meta->create(&fdb_entry, 3, list1);
    EXPECT_NE(SAI_STATUS_SUCCESS, status);

    attr2.value.oid = port;
    attr3.value.s32 = 0x100;

    SWSS_LOG_NOTICE("invalid attribute value on enum");
    status = g_meta->create(&fdb_entry, 3, list1);
    EXPECT_NE(SAI_STATUS_SUCCESS, status);

    attr3.value.s32 = SAI_PACKET_ACTION_FORWARD;

    sai_attribute_t list2[4] = { attr1, attr2, attr3, attr3 };

    SWSS_LOG_NOTICE("repeated attribute id");
    status = g_meta->create(&fdb_entry, 4, list2);
    EXPECT_NE(SAI_STATUS_SUCCESS, status);

    SWSS_LOG_NOTICE("correct");
    status = g_meta->create(&fdb_entry, 3, list2);
    EXPECT_EQ(SAI_STATUS_SUCCESS, status);

    SWSS_LOG_NOTICE("already exists");
    status = g_meta->create(&fdb_entry, 3, list2);
    EXPECT_NE(SAI_STATUS_SUCCESS, status);

    remove_switch(switch_id);
}

TEST(LegacyFdbEntry, fdb_entry_remove)
{
    clear_local();

    sai_status_t    status;

    sai_object_id_t switch_id = create_switch();

    sai_fdb_entry_t fdb_entry;

    sai_mac_t mac = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66};

    memcpy(fdb_entry.mac_address, mac, sizeof(mac));
    fdb_entry.switch_id = switch_id;
    sai_object_id_t bridge_id = create_bridge(switch_id);
    fdb_entry.bv_id= bridge_id;

    sai_object_id_t port = create_bridge_port(switch_id, bridge_id);

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
    status = g_meta->create(&fdb_entry, 3, list1);
    EXPECT_EQ(SAI_STATUS_SUCCESS, status);

    SWSS_LOG_NOTICE("remove tests");

    SWSS_LOG_NOTICE("fdb_entry is null");
    status = g_meta->remove((const sai_fdb_entry_t*)NULL);
    EXPECT_NE(SAI_STATUS_SUCCESS, status);

    //SWSS_LOG_NOTICE("invalid vlan");
    //status = g_meta->remove(&fdb_entry);
    //EXPECT_NE(SAI_STATUS_SUCCESS, status);

    fdb_entry.mac_address[0] = 1;

    SWSS_LOG_NOTICE("invalid mac");
    status = g_meta->remove(&fdb_entry);
    EXPECT_NE(SAI_STATUS_SUCCESS, status);

    fdb_entry.mac_address[0] = 0x11;

    sai_object_meta_key_t key = { .objecttype = SAI_OBJECT_TYPE_FDB_ENTRY, .objectkey = { .key = { .fdb_entry = fdb_entry } } };

    EXPECT_TRUE(g_meta->objectExists(key));

    SWSS_LOG_NOTICE("success");
    status = g_meta->remove(&fdb_entry);
    EXPECT_EQ(SAI_STATUS_SUCCESS, status);

    EXPECT_TRUE(!g_meta->objectExists(key));

    remove_switch(switch_id);
}

TEST(LegacyFdbEntry, fdb_entry_set)
{
    clear_local();

    sai_status_t    status;
    sai_attribute_t attr;

    sai_fdb_entry_t fdb_entry = create_fdb_entry();

    //status = g_meta->create(&fdb_entry, 0, 0);
    //EXPECT_EQ(SAI_STATUS_SUCCESS, status);

    SWSS_LOG_NOTICE("attr is null");
    status = g_meta->set(&fdb_entry, NULL);
    EXPECT_NE(SAI_STATUS_SUCCESS, status);

    SWSS_LOG_NOTICE("fdb entry is null");
    status = g_meta->set((const sai_fdb_entry_t*)NULL, &attr);
    EXPECT_NE(SAI_STATUS_SUCCESS, status);

    SWSS_LOG_NOTICE("setting read only object");
    attr.id = SAI_FDB_ENTRY_ATTR_BRIDGE_PORT_ID;
    attr.value.oid = create_dummy_object_id(SAI_OBJECT_TYPE_BRIDGE_PORT, fdb_entry.switch_id);

    status = g_meta->set(&fdb_entry, &attr);
    EXPECT_NE(SAI_STATUS_SUCCESS, status);

    SWSS_LOG_NOTICE("setting invalid attrib id");
    attr.id = -1;
    status = g_meta->set(&fdb_entry, &attr);
    EXPECT_NE(SAI_STATUS_SUCCESS, status);

    //SWSS_LOG_NOTICE("invalid vlan");
    //attr.id = SAI_FDB_ENTRY_ATTR_TYPE;
    //attr.value.s32 = SAI_FDB_ENTRY_TYPE_STATIC;
    //status = g_meta->set(&fdb_entry, &attr);
    //EXPECT_NE(SAI_STATUS_SUCCESS, status);

    //SWSS_LOG_NOTICE("vlan outside range");
    //attr.id = SAI_FDB_ENTRY_ATTR_TYPE;
    //attr.value.s32 = SAI_FDB_ENTRY_TYPE_STATIC;
    //status = g_meta->set(&fdb_entry, &attr);
    //EXPECT_NE(SAI_STATUS_SUCCESS, status);

    // correct
    attr.id = SAI_FDB_ENTRY_ATTR_TYPE;
    attr.value.s32 = SAI_FDB_ENTRY_TYPE_STATIC;
    status = g_meta->set(&fdb_entry, &attr);
    EXPECT_EQ(SAI_STATUS_SUCCESS, status);

    // TODO check references ?

    remove_switch(fdb_entry.switch_id);
}

TEST(LegacyFdbEntry, fdb_entry_get)
{
    clear_local();

    sai_status_t    status;
    sai_attribute_t attr;

    sai_fdb_entry_t fdb_entry = create_fdb_entry();

    attr.id = SAI_FDB_ENTRY_ATTR_TYPE;
    attr.value.s32 = SAI_FDB_ENTRY_TYPE_STATIC;
    status = g_meta->set(&fdb_entry, &attr);
    EXPECT_EQ(SAI_STATUS_SUCCESS, status);

    SWSS_LOG_NOTICE("get test");

    // zero attribute count
    attr.id = SAI_FDB_ENTRY_ATTR_TYPE;
    status = g_meta->get(&fdb_entry, 0, &attr);
    EXPECT_NE(SAI_STATUS_SUCCESS, status);

    // attr is null
    status = g_meta->get(&fdb_entry, 1, NULL);
    EXPECT_NE(SAI_STATUS_SUCCESS, status);

    // fdb entry is null
    status = g_meta->get((sai_fdb_entry_t*)NULL, 1, &attr);
    EXPECT_NE(SAI_STATUS_SUCCESS, status);

    // attr id out of range
    attr.id = -1;
    status = g_meta->get(&fdb_entry, 1, &attr);
    EXPECT_NE(SAI_STATUS_SUCCESS, status);

    // correct single valid attribute
    attr.id = SAI_FDB_ENTRY_ATTR_TYPE;
    status = g_meta->get(&fdb_entry, 1, &attr);
    EXPECT_EQ(SAI_STATUS_SUCCESS, status);

    // correct 2 attributes
    sai_attribute_t attr1;
    attr1.id = SAI_FDB_ENTRY_ATTR_TYPE;

    sai_attribute_t attr2;
    attr2.id = SAI_FDB_ENTRY_ATTR_BRIDGE_PORT_ID;
    sai_attribute_t list[2] = { attr1, attr2 };

    status = g_meta->get(&fdb_entry, 2, list);
    EXPECT_EQ(SAI_STATUS_SUCCESS, status);

    remove_switch(fdb_entry.switch_id);
}

TEST(LegacyFdbEntry, fdb_entry_flow)
{
    SWSS_LOG_TIMER("fdb flow");

    clear_local();

    sai_object_id_t switch_id = create_switch();
    sai_status_t    status;
    sai_attribute_t attr;

    sai_fdb_entry_t fdb_entry;

    sai_mac_t mac = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66};

    memcpy(fdb_entry.mac_address, mac, sizeof(mac));
    fdb_entry.switch_id = switch_id;

    sai_object_id_t bridge_id = create_bridge(switch_id);
    fdb_entry.bv_id= bridge_id;

    sai_object_id_t lag = create_bridge_port(switch_id, bridge_id);

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
    status = g_meta->create(&fdb_entry, 4, list);
    EXPECT_EQ(SAI_STATUS_SUCCESS, status);

    SWSS_LOG_NOTICE("create existing");
    status = g_meta->create(&fdb_entry, 4, list);
    EXPECT_NE(SAI_STATUS_SUCCESS, status);

    SWSS_LOG_NOTICE("set");
    attr.id = SAI_FDB_ENTRY_ATTR_TYPE;
    attr.value.s32 = SAI_FDB_ENTRY_TYPE_DYNAMIC;
    status = g_meta->set(&fdb_entry, &attr);
    EXPECT_EQ(SAI_STATUS_SUCCESS, status);

    SWSS_LOG_NOTICE("set");
    attr.id = SAI_FDB_ENTRY_ATTR_META_DATA;
    attr.value.u32 = SAI_FDB_ENTRY_TYPE_DYNAMIC;
    status = g_meta->set(&fdb_entry, &attr);
    EXPECT_EQ(SAI_STATUS_SUCCESS, status);

    SWSS_LOG_NOTICE("get");
    status = g_meta->get(&fdb_entry, 4, list);
    EXPECT_EQ(SAI_STATUS_SUCCESS, status);

    SWSS_LOG_NOTICE("remove");
    status = g_meta->remove(&fdb_entry);
    EXPECT_EQ(SAI_STATUS_SUCCESS, status);

    SWSS_LOG_NOTICE("remove non existing");
    status = g_meta->remove(&fdb_entry);
    EXPECT_NE(SAI_STATUS_SUCCESS, status);

    remove_switch(switch_id);
}
