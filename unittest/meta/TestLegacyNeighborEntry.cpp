#include "TestLegacy.h"

#include <arpa/inet.h>

#include <gtest/gtest.h>

#include <memory>

using namespace TestLegacy;

// STATIC HELPERS

//static sai_neighbor_entry_t create_neighbor_entry()
//{
//    SWSS_LOG_ENTER();
//
//    sai_object_id_t switch_id = create_switch();
//    sai_status_t    status;
//
//    sai_mac_t mac = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66};
//
//    sai_neighbor_entry_t neighbor_entry;
//
//    sai_object_id_t rif = create_rif(switch_id);
//
//    neighbor_entry.ip_address.addr_family = SAI_IP_ADDR_FAMILY_IPV4;
//    neighbor_entry.ip_address.addr.ip4 = htonl(0x0a00000f);
//    neighbor_entry.rif_id = rif;
//    neighbor_entry.switch_id = switch_id;
//
//    sai_attribute_t list[3] = { };
//
//    sai_attribute_t &attr1 = list[0];
//    sai_attribute_t &attr2 = list[1];
//    sai_attribute_t &attr3 = list[2];
//
//    attr1.id = SAI_NEIGHBOR_ENTRY_ATTR_DST_MAC_ADDRESS;
//    memcpy(attr1.value.mac, mac, 6);
//
//    attr2.id = SAI_NEIGHBOR_ENTRY_ATTR_PACKET_ACTION;
//    attr2.value.s32 = SAI_PACKET_ACTION_FORWARD;
//
//    attr3.id = -1;
//
//    attr2.value.s32 = SAI_PACKET_ACTION_FORWARD;
//
//    sai_attribute_t list2[4] = { attr1, attr2, attr2 };
//
//    status = g_meta->create(&neighbor_entry, 2, list2);
//    EXPECT_EQ(SAI_STATUS_SUCCESS, status);
//
//    return neighbor_entry;
//}

static sai_object_id_t create_hash(
        _In_ sai_object_id_t switch_id)
{
    SWSS_LOG_ENTER();

    sai_object_id_t hash;

    auto status = g_meta->create(SAI_OBJECT_TYPE_HASH, &hash, switch_id, 0, NULL);
    EXPECT_EQ(SAI_STATUS_SUCCESS, status);

    return hash;
}

// ACTUAL TESTS

TEST(LegacyNeighborEntry, neighbor_entry_create)
{
    SWSS_LOG_ENTER();

    clear_local();

    sai_object_id_t switch_id = create_switch();
    sai_status_t    status;
    sai_attribute_t attr;

    sai_mac_t mac = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66};

    sai_neighbor_entry_t neighbor_entry;

    sai_object_id_t rif = create_rif(switch_id);

    neighbor_entry.ip_address.addr_family = SAI_IP_ADDR_FAMILY_IPV4;
    neighbor_entry.ip_address.addr.ip4 = htonl(0x0a00000f);
    neighbor_entry.rif_id = rif;
    neighbor_entry.switch_id = switch_id;

    SWSS_LOG_NOTICE("create tests");

    SWSS_LOG_NOTICE("zero attribute count (but there are mandatory attributes)");
    status = g_meta->create(&neighbor_entry, 0, &attr);
    EXPECT_NE(SAI_STATUS_SUCCESS, status);

    SWSS_LOG_NOTICE("attr is null");
    status = g_meta->create(&neighbor_entry, 1, NULL);
    EXPECT_NE(SAI_STATUS_SUCCESS, status);

    SWSS_LOG_NOTICE("neighbor entry is null");
    status = g_meta->create((sai_neighbor_entry_t*)NULL, 1, &attr);
    EXPECT_NE(SAI_STATUS_SUCCESS, status);

    sai_attribute_t list[3] = { };

    sai_attribute_t &attr1 = list[0];
    sai_attribute_t &attr2 = list[1];
    sai_attribute_t &attr3 = list[2];

    attr1.id = SAI_NEIGHBOR_ENTRY_ATTR_DST_MAC_ADDRESS;
    memcpy(attr1.value.mac, mac, 6);

    attr2.id = SAI_NEIGHBOR_ENTRY_ATTR_PACKET_ACTION;
    attr2.value.s32 = SAI_PACKET_ACTION_FORWARD;

    attr3.id = -1;

    SWSS_LOG_NOTICE("invalid attribute id");
    status = g_meta->create(&neighbor_entry, 3, list);
    EXPECT_NE(SAI_STATUS_SUCCESS, status);

    attr2.value.s32 = SAI_PACKET_ACTION_FORWARD + 0x100;
    SWSS_LOG_NOTICE("invalid attribute value on enum");
    status = g_meta->create(&neighbor_entry, 2, list);
    EXPECT_NE(SAI_STATUS_SUCCESS, status);

    attr2.value.s32 = SAI_PACKET_ACTION_FORWARD;

    sai_attribute_t list2[4] = { attr1, attr2, attr2 };

    SWSS_LOG_NOTICE("repeated attribute id");
    status = g_meta->create(&neighbor_entry, 3, list2);
    EXPECT_NE(SAI_STATUS_SUCCESS, status);

    SWSS_LOG_NOTICE("correct ipv4");
    status = g_meta->create(&neighbor_entry, 2, list2);
    EXPECT_EQ(SAI_STATUS_SUCCESS, status);

    neighbor_entry.ip_address.addr_family = SAI_IP_ADDR_FAMILY_IPV6;
    sai_ip6_t ip6 = {0x00, 0x11, 0x22, 0x33,0x44, 0x55, 0x66,0x77, 0x88, 0x99, 0xaa, 0xbb,0xcc,0xdd,0xee,0xff};
    memcpy(neighbor_entry.ip_address.addr.ip6, ip6, 16);

    SWSS_LOG_NOTICE("correct ipv6");
    status = g_meta->create(&neighbor_entry, 2, list2);
    EXPECT_EQ(SAI_STATUS_SUCCESS, status);

    SWSS_LOG_NOTICE("already exists");
    status = g_meta->create(&neighbor_entry, 2, list2);
    EXPECT_NE(SAI_STATUS_SUCCESS, status);

    remove_switch(switch_id);
}

TEST(LegacyNeighborEntry, neighbor_entry_remove)
{
    clear_local();

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

    sai_object_id_t rif = create_rif(switch_id);

    neighbor_entry.ip_address.addr_family = SAI_IP_ADDR_FAMILY_IPV4;
    neighbor_entry.ip_address.addr.ip4 = htonl(0x0a00000f);
    neighbor_entry.rif_id = rif;
    neighbor_entry.switch_id = switch_id;

    SWSS_LOG_NOTICE("create");

    SWSS_LOG_NOTICE("correct ipv4");
    status = g_meta->create(&neighbor_entry, 2, list);
    EXPECT_EQ(SAI_STATUS_SUCCESS, status);

    neighbor_entry.ip_address.addr_family = SAI_IP_ADDR_FAMILY_IPV6;
    sai_ip6_t ip6 = {0x00, 0x11, 0x22, 0x33,0x44, 0x55, 0x66,0x77, 0x88, 0x99, 0xaa, 0xbb,0xcc,0xdd,0xee,0xff};
    memcpy(neighbor_entry.ip_address.addr.ip6, ip6, 16);

    SWSS_LOG_NOTICE("correct ipv6");
    status = g_meta->create(&neighbor_entry, 2, list);
    EXPECT_EQ(SAI_STATUS_SUCCESS, status);

    SWSS_LOG_NOTICE("remove tests");

    SWSS_LOG_NOTICE("neighbor_entry is null");
    status = g_meta->remove((sai_neighbor_entry_t*)NULL);
    EXPECT_NE(SAI_STATUS_SUCCESS, status);

    neighbor_entry.rif_id = SAI_NULL_OBJECT_ID;

    SWSS_LOG_NOTICE("invalid object id null");
    status = g_meta->remove(&neighbor_entry);
    EXPECT_NE(SAI_STATUS_SUCCESS, status);

    neighbor_entry.rif_id = create_hash(switch_id);

    SWSS_LOG_NOTICE("invalid object id hash");
    status = g_meta->remove(&neighbor_entry);
    EXPECT_NE(SAI_STATUS_SUCCESS, status);

    neighbor_entry.rif_id = create_rif(switch_id);

    SWSS_LOG_NOTICE("invalid object id router");
    status = g_meta->remove(&neighbor_entry);
    EXPECT_NE(SAI_STATUS_SUCCESS, status);

    neighbor_entry.rif_id = rif;

    sai_object_meta_key_t key = { .objecttype = SAI_OBJECT_TYPE_NEIGHBOR_ENTRY, .objectkey = { .key = { .neighbor_entry = neighbor_entry } } };

    EXPECT_TRUE(g_meta->objectExists(key));

    SWSS_LOG_NOTICE("success");
    status = g_meta->remove(&neighbor_entry);
    EXPECT_EQ(SAI_STATUS_SUCCESS, status);

    EXPECT_TRUE(!g_meta->objectExists(key));

    remove_switch(switch_id);
}

TEST(LegacyNeighborEntry, neighbor_entry_set)
{
    clear_local();

    sai_status_t    status;

    sai_attribute_t attr;

    memset(&attr, 0, sizeof(attr));

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

    sai_object_id_t rif = create_rif(switch_id);

    neighbor_entry.ip_address.addr_family = SAI_IP_ADDR_FAMILY_IPV4;
    neighbor_entry.ip_address.addr.ip4 = htonl(0x0a00000f);
    neighbor_entry.rif_id = rif;
    neighbor_entry.switch_id = switch_id;

    SWSS_LOG_NOTICE("create");

    SWSS_LOG_NOTICE("correct ipv4");
    status = g_meta->create(&neighbor_entry, 2, list);
    EXPECT_EQ(SAI_STATUS_SUCCESS, status);

    neighbor_entry.ip_address.addr_family = SAI_IP_ADDR_FAMILY_IPV6;
    sai_ip6_t ip6 = {0x00, 0x11, 0x22, 0x33,0x44, 0x55, 0x66,0x77, 0x88, 0x99, 0xaa, 0xbb,0xcc,0xdd,0xee,0xff};
    memcpy(neighbor_entry.ip_address.addr.ip6, ip6, 16);

    SWSS_LOG_NOTICE("correct ipv6");
    status = g_meta->create(&neighbor_entry, 2, list);
    EXPECT_EQ(SAI_STATUS_SUCCESS, status);

    SWSS_LOG_NOTICE("set tests");

    SWSS_LOG_NOTICE("attr is null");
    status = g_meta->set(&neighbor_entry, NULL);
    EXPECT_NE(SAI_STATUS_SUCCESS, status);

    SWSS_LOG_NOTICE("neighbor entry is null");
    status = g_meta->set((sai_neighbor_entry_t*)NULL, &attr);
    EXPECT_NE(SAI_STATUS_SUCCESS, status);

    SWSS_LOG_NOTICE("setting invalid attrib id");
    attr.id = -1;
    status = g_meta->set(&neighbor_entry, &attr);
    EXPECT_NE(SAI_STATUS_SUCCESS, status);

    SWSS_LOG_NOTICE("value outside range");
    attr.id = SAI_NEIGHBOR_ENTRY_ATTR_PACKET_ACTION;
    attr.value.s32 = 0x100;
    status = g_meta->set(&neighbor_entry, &attr);
    EXPECT_NE(SAI_STATUS_SUCCESS, status);

    // correct
    attr.id = SAI_NEIGHBOR_ENTRY_ATTR_PACKET_ACTION;
    attr.value.s32 = SAI_PACKET_ACTION_DROP;
    status = g_meta->set(&neighbor_entry, &attr);
    EXPECT_EQ(SAI_STATUS_SUCCESS, status);

    remove_switch(switch_id);
}

TEST(LegacyNeighborEntry, neighbor_entry_get)
{
    clear_local();

    sai_status_t    status;

    sai_attribute_t attr;

    memset(&attr, 0, sizeof(attr));

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

    sai_object_id_t rif = create_rif(switch_id);

    neighbor_entry.ip_address.addr_family = SAI_IP_ADDR_FAMILY_IPV4;
    neighbor_entry.ip_address.addr.ip4 = htonl(0x0a00000f);
    neighbor_entry.rif_id = rif;
    neighbor_entry.switch_id = switch_id;

    SWSS_LOG_NOTICE("create");

    SWSS_LOG_NOTICE("correct ipv4");
    status = g_meta->create(&neighbor_entry, 2, list);
    EXPECT_EQ(SAI_STATUS_SUCCESS, status);

    neighbor_entry.ip_address.addr_family = SAI_IP_ADDR_FAMILY_IPV6;
    sai_ip6_t ip6 = {0x00, 0x11, 0x22, 0x33,0x44, 0x55, 0x66,0x77, 0x88, 0x99, 0xaa, 0xbb,0xcc,0xdd,0xee,0xff};
    memcpy(neighbor_entry.ip_address.addr.ip6, ip6, 16);

    SWSS_LOG_NOTICE("correct ipv6");
    status = g_meta->create(&neighbor_entry, 2, list);
    EXPECT_EQ(SAI_STATUS_SUCCESS, status);

    SWSS_LOG_NOTICE("get test");

    SWSS_LOG_NOTICE("zero attribute count");
    attr.id = SAI_NEIGHBOR_ENTRY_ATTR_PACKET_ACTION;
    status = g_meta->get(&neighbor_entry, 0, &attr);
    EXPECT_NE(SAI_STATUS_SUCCESS, status);

    SWSS_LOG_NOTICE("attr is null");
    status = g_meta->get(&neighbor_entry, 1, NULL);
    EXPECT_NE(SAI_STATUS_SUCCESS, status);

    SWSS_LOG_NOTICE("neighbor entry is null");
    status = g_meta->get((sai_neighbor_entry_t*)NULL, 1, &attr);
    EXPECT_NE(SAI_STATUS_SUCCESS, status);

    SWSS_LOG_NOTICE("attr id out of range");
    attr.id = -1;
    status = g_meta->get(&neighbor_entry, 1, &attr);
    EXPECT_NE(SAI_STATUS_SUCCESS, status);

    SWSS_LOG_NOTICE("correct single valid attribute");
    attr.id = SAI_NEIGHBOR_ENTRY_ATTR_PACKET_ACTION;
    status = g_meta->get(&neighbor_entry, 1, &attr);
    EXPECT_EQ(SAI_STATUS_SUCCESS, status);

    SWSS_LOG_NOTICE("correct 2 attributes");

    sai_attribute_t attr3;
    sai_attribute_t attr4;
    attr3.id = SAI_NEIGHBOR_ENTRY_ATTR_PACKET_ACTION;
    attr3.value.s32 = 1;
    attr4.id = SAI_NEIGHBOR_ENTRY_ATTR_NO_HOST_ROUTE;

    sai_attribute_t list2[2] = { attr3, attr4 };
    status = g_meta->get(&neighbor_entry, 2, list2);
    EXPECT_EQ(SAI_STATUS_SUCCESS, status);

    remove_switch(switch_id);
}

TEST(LegacyNeighborEntry, neighbor_entry_flow)
{
    SWSS_LOG_ENTER();

    clear_local();

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

    sai_object_id_t rif = create_rif(switch_id);

    neighbor_entry.ip_address.addr_family = SAI_IP_ADDR_FAMILY_IPV4;
    neighbor_entry.ip_address.addr.ip4 = htonl(0x0a00000f);
    neighbor_entry.rif_id = rif;
    neighbor_entry.switch_id = switch_id;

    SWSS_LOG_NOTICE("create");

    SWSS_LOG_NOTICE("correct ipv4");
    status = g_meta->create(&neighbor_entry, 2, list);
    EXPECT_EQ(SAI_STATUS_SUCCESS, status);

    SWSS_LOG_NOTICE("correct ipv4 existing");
    status = g_meta->create(&neighbor_entry, 2, list);
    EXPECT_NE(SAI_STATUS_SUCCESS, status);

    SWSS_LOG_NOTICE("set");
    status = g_meta->set(&neighbor_entry, &attr1);
    EXPECT_EQ(SAI_STATUS_SUCCESS, status);

    SWSS_LOG_NOTICE("set");
    status = g_meta->set(&neighbor_entry, &attr2);
    EXPECT_EQ(SAI_STATUS_SUCCESS, status);

    SWSS_LOG_NOTICE("set");
    status = g_meta->set(&neighbor_entry, &attr3);
    EXPECT_EQ(SAI_STATUS_SUCCESS, status);

    SWSS_LOG_NOTICE("set");
    status = g_meta->set(&neighbor_entry, &attr4);
    EXPECT_EQ(SAI_STATUS_SUCCESS, status);

    SWSS_LOG_NOTICE("remove");
    status = g_meta->remove(&neighbor_entry);
    EXPECT_EQ(SAI_STATUS_SUCCESS, status);

    SWSS_LOG_NOTICE("remove existing");
    status = g_meta->remove(&neighbor_entry);
    EXPECT_NE(SAI_STATUS_SUCCESS, status);

    remove_switch(switch_id);
}
