#include "TestLegacy.h"

#include <arpa/inet.h>

#include <gtest/gtest.h>

#include <memory>

using namespace TestLegacy;

// STATIC HELPERS

// ACTUAL TESTS

TEST(LegacyRouteEntry, route_entry_create)
{
    SWSS_LOG_ENTER();

    clear_local();

    sai_status_t    status;
    sai_attribute_t attr;

    sai_object_id_t switch_id = create_switch();
    sai_route_entry_t route_entry;

    sai_object_id_t vr = create_virtual_router(switch_id);

    sai_object_id_t hop = create_next_hop(switch_id);

    route_entry.destination.addr_family = SAI_IP_ADDR_FAMILY_IPV4;
    route_entry.destination.addr.ip4 = htonl(0x0a00000f);
    route_entry.destination.mask.ip4 = htonl(0xffffff00);
    route_entry.vr_id = vr;
    route_entry.switch_id = switch_id;

    SWSS_LOG_NOTICE("create tests");

    // commented out as there is no mandatory attribute
    // SWSS_LOG_NOTICE("zero attribute count (but there are mandatory attributes)");
    // attr.id = SAI_ROUTE_ENTRY_ATTR_NEXT_HOP_ID;
    // status = g_meta->create(&route_entry, 0, &attr);
    // EXPECT_NE(SAI_STATUS_SUCCESS, status);

    SWSS_LOG_NOTICE("attr is null");
    status = g_meta->create(&route_entry, 1, NULL);
    EXPECT_NE(SAI_STATUS_SUCCESS, status);

    SWSS_LOG_NOTICE("route entry is null");
    status = g_meta->create((sai_route_entry_t*)NULL, 1, &attr);
    EXPECT_NE(SAI_STATUS_SUCCESS, status);

    sai_attribute_t list[3] = { };

    sai_attribute_t &attr1 = list[0];
    sai_attribute_t &attr2 = list[1];
    sai_attribute_t &attr3 = list[2];

    attr1.id = SAI_ROUTE_ENTRY_ATTR_NEXT_HOP_ID;
    attr1.value.oid = hop;

    attr2.id = SAI_ROUTE_ENTRY_ATTR_PACKET_ACTION;
    attr2.value.s32 = SAI_PACKET_ACTION_FORWARD;

    attr3.id = -1;

    SWSS_LOG_NOTICE("invalid attribute id");
    status = g_meta->create(&route_entry, 3, list);
    EXPECT_NE(SAI_STATUS_SUCCESS, status);

    attr2.value.s32 = SAI_PACKET_ACTION_FORWARD + 0x100;
    SWSS_LOG_NOTICE("invalid attribute value on enum");
    status = g_meta->create(&route_entry, 2, list);
    EXPECT_NE(SAI_STATUS_SUCCESS, status);

    attr2.value.s32 = SAI_PACKET_ACTION_FORWARD;

    sai_attribute_t list2[4] = { attr1, attr2, attr2 };

    SWSS_LOG_NOTICE("repeated attribute id");
    status = g_meta->create(&route_entry, 3, list2);
    EXPECT_NE(SAI_STATUS_SUCCESS, status);

    SWSS_LOG_NOTICE("wrong object type");
    attr1.value.oid = create_dummy_object_id(SAI_OBJECT_TYPE_HASH,switch_id);
    status = g_meta->create(&route_entry, 2, list);
    EXPECT_NE(SAI_STATUS_SUCCESS, status);

    SWSS_LOG_NOTICE("non existing object");
    attr1.value.oid = create_dummy_object_id(SAI_OBJECT_TYPE_NEXT_HOP,switch_id);
    status = g_meta->create(&route_entry, 2, list);
    EXPECT_NE(SAI_STATUS_SUCCESS, status);

    int fam = 10;
    attr1.value.oid = hop;
    route_entry.destination.addr_family = (sai_ip_addr_family_t)fam;

    SWSS_LOG_NOTICE("wrong address family");
    status = g_meta->create(&route_entry, 2, list);
    EXPECT_NE(SAI_STATUS_SUCCESS, status);

    SWSS_LOG_NOTICE("correct ipv4");
    route_entry.destination.addr_family = SAI_IP_ADDR_FAMILY_IPV4;
    status = g_meta->create(&route_entry, 2, list);
    EXPECT_EQ(SAI_STATUS_SUCCESS, status);

    route_entry.destination.addr_family = SAI_IP_ADDR_FAMILY_IPV6;
    sai_ip6_t ip62 = {0x00, 0x11, 0x22, 0x33,0x44, 0x55, 0x66,0x77, 0x88, 0x99, 0xaa, 0xbb,0xcc,0xdd,0xee,0x99};
    memcpy(route_entry.destination.addr.ip6, ip62, 16);

    sai_ip6_t ip6mask2 = {0xff, 0xff, 0xff, 0xff,0xff, 0xff, 0xff,0xff, 0xff, 0xff, 0xff, 0xff,0xff,0xff,0xf7,0x00};
    memcpy(route_entry.destination.mask.ip6, ip6mask2, 16);

    SWSS_LOG_NOTICE("invalid mask");
    status = g_meta->create(&route_entry, 2, list);
    EXPECT_NE(SAI_STATUS_SUCCESS, status);

    route_entry.destination.addr_family = SAI_IP_ADDR_FAMILY_IPV6;
    sai_ip6_t ip6 = {0x00, 0x11, 0x22, 0x33,0x44, 0x55, 0x66,0x77, 0x88, 0x99, 0xaa, 0xbb,0xcc,0xdd,0xee,0xff};
    memcpy(route_entry.destination.addr.ip6, ip6, 16);

    sai_ip6_t ip6mask = {0xff, 0xff, 0xff, 0xff,0xff, 0xff, 0xff,0xff, 0xff, 0xff, 0xff, 0xff,0xff,0xff,0xff,0x00};
    memcpy(route_entry.destination.mask.ip6, ip6mask, 16);

    SWSS_LOG_NOTICE("correct ipv6");
    status = g_meta->create(&route_entry, 2, list);
    EXPECT_EQ(SAI_STATUS_SUCCESS, status);

    SWSS_LOG_NOTICE("already exists");
    status = g_meta->create(&route_entry, 2, list);
    EXPECT_NE(SAI_STATUS_SUCCESS, status);

    remove_switch(switch_id);
}

TEST(LegacyRouteEntry, route_entry_remove)
{
    SWSS_LOG_ENTER();

    clear_local();

    sai_status_t    status;
    sai_object_id_t switch_id = create_switch();

    sai_route_entry_t route_entry;

    sai_object_id_t vr = create_virtual_router(switch_id);
    sai_object_id_t hop = create_next_hop(switch_id);

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
    status = g_meta->create(&route_entry, 2, list);
    EXPECT_EQ(SAI_STATUS_SUCCESS, status);

    route_entry.destination.addr_family = SAI_IP_ADDR_FAMILY_IPV6;
    sai_ip6_t ip6 = {0x00, 0x11, 0x22, 0x33,0x44, 0x55, 0x66,0x77, 0x88, 0x99, 0xaa, 0xbb,0xcc,0xdd,0xee,0xff};
    memcpy(route_entry.destination.addr.ip6, ip6, 16);

    sai_ip6_t ip6mask = {0xff, 0xff, 0xff, 0xff,0xff, 0xff, 0xff,0xff, 0xff, 0xff, 0xff, 0xff,0xff,0xff,0xff,0x00};
    memcpy(route_entry.destination.mask.ip6, ip6mask, 16);

    SWSS_LOG_NOTICE("correct ipv6");
    status = g_meta->create(&route_entry, 2, list);
    EXPECT_EQ(SAI_STATUS_SUCCESS, status);

    SWSS_LOG_NOTICE("remove tests");

    SWSS_LOG_NOTICE("route_entry is null");
    status = g_meta->remove((sai_route_entry_t*)NULL);
    EXPECT_NE(SAI_STATUS_SUCCESS, status);

    route_entry.vr_id = SAI_NULL_OBJECT_ID;

    SWSS_LOG_NOTICE("invalid object id null");
    status = g_meta->remove(&route_entry);
    EXPECT_NE(SAI_STATUS_SUCCESS, status);

    route_entry.vr_id = create_dummy_object_id(SAI_OBJECT_TYPE_HASH,switch_id);

    SWSS_LOG_NOTICE("invalid object id hash");
    status = g_meta->remove(&route_entry);
    EXPECT_NE(SAI_STATUS_SUCCESS, status);

    route_entry.vr_id = create_dummy_object_id(SAI_OBJECT_TYPE_VIRTUAL_ROUTER,switch_id);

    SWSS_LOG_NOTICE("invalid object id router");
    status = g_meta->remove(&route_entry);
    EXPECT_NE(SAI_STATUS_SUCCESS, status);

    route_entry.vr_id = vr;

    sai_object_meta_key_t key = { .objecttype = SAI_OBJECT_TYPE_ROUTE_ENTRY, .objectkey = { .key = { .route_entry = route_entry } } };

    EXPECT_TRUE(g_meta->objectExists(key));

    SWSS_LOG_NOTICE("success");
    status = g_meta->remove(&route_entry);
    EXPECT_EQ(SAI_STATUS_SUCCESS, status);

    EXPECT_TRUE(!g_meta->objectExists(key));

    route_entry.destination.addr_family = SAI_IP_ADDR_FAMILY_IPV4;
    route_entry.destination.addr.ip4 = htonl(0x0a00000f);
    route_entry.destination.mask.ip4 = htonl(0xffffff00);

    SWSS_LOG_NOTICE("success");
    status = g_meta->remove(&route_entry);
    EXPECT_EQ(SAI_STATUS_SUCCESS, status);

    remove_switch(switch_id);
}

TEST(LegacyRouteEntry, route_entry_set)
{
    SWSS_LOG_ENTER();

    clear_local();

    sai_status_t    status;
    sai_attribute_t attr;
    sai_object_id_t switch_id = create_switch();

    sai_route_entry_t route_entry;

    sai_object_id_t vr = create_virtual_router(switch_id);
    sai_object_id_t hop = create_next_hop(switch_id);

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
    status = g_meta->create(&route_entry, 2, list);
    EXPECT_EQ(SAI_STATUS_SUCCESS, status);

    route_entry.destination.addr_family = SAI_IP_ADDR_FAMILY_IPV6;
    sai_ip6_t ip6 = {0x00, 0x11, 0x22, 0x33,0x44, 0x55, 0x66,0x77, 0x88, 0x99, 0xaa, 0xbb,0xcc,0xdd,0xee,0xff};
    memcpy(route_entry.destination.addr.ip6, ip6, 16);

    sai_ip6_t ip6mask = {0xff, 0xff, 0xff, 0xff,0xff, 0xff, 0xff,0xff, 0xff, 0xff, 0xff, 0xff,0xff,0xff,0xff,0x00};
    memcpy(route_entry.destination.mask.ip6, ip6mask, 16);

    SWSS_LOG_NOTICE("correct ipv6");
    status = g_meta->create(&route_entry, 2, list);
    EXPECT_EQ(SAI_STATUS_SUCCESS, status);

    SWSS_LOG_NOTICE("set tests");

    SWSS_LOG_NOTICE("attr is null");
    status = g_meta->set(&route_entry, NULL);
    EXPECT_NE(SAI_STATUS_SUCCESS, status);

    SWSS_LOG_NOTICE("route entry is null");
    status = g_meta->set((sai_route_entry_t*)NULL, &attr);
    EXPECT_NE(SAI_STATUS_SUCCESS, status);

    SWSS_LOG_NOTICE("setting invalid attrib id");
    attr.id = -1;
    status = g_meta->set(&route_entry, &attr);
    EXPECT_NE(SAI_STATUS_SUCCESS, status);

    SWSS_LOG_NOTICE("value outside range");
    attr.id = SAI_ROUTE_ENTRY_ATTR_PACKET_ACTION;
    attr.value.s32 = 0x100;
    status = g_meta->set(&route_entry, &attr);
    EXPECT_NE(SAI_STATUS_SUCCESS, status);

    SWSS_LOG_NOTICE("correct packet action");
    attr.id = SAI_ROUTE_ENTRY_ATTR_PACKET_ACTION;
    attr.value.s32 = SAI_PACKET_ACTION_DROP;
    status = g_meta->set(&route_entry, &attr);
    EXPECT_EQ(SAI_STATUS_SUCCESS, status);

    SWSS_LOG_NOTICE("correct next hop");
    attr.id = SAI_ROUTE_ENTRY_ATTR_NEXT_HOP_ID;
    attr.value.oid = hop;
    status = g_meta->set(&route_entry, &attr);
    EXPECT_EQ(SAI_STATUS_SUCCESS, status);

    SWSS_LOG_NOTICE("correct metadata");
    attr.id = SAI_ROUTE_ENTRY_ATTR_META_DATA;
    attr.value.u32 = 0x12345678;
    status = g_meta->set(&route_entry, &attr);
    EXPECT_EQ(SAI_STATUS_SUCCESS, status);

    remove_switch(switch_id);
}

TEST(LegacyRouteEntry, route_entry_get)
{
    SWSS_LOG_ENTER();

    clear_local();

    sai_status_t    status;
    sai_attribute_t attr;
    sai_object_id_t switch_id = create_switch();

    sai_route_entry_t route_entry;

    sai_object_id_t vr = create_virtual_router(switch_id);
    sai_object_id_t hop = create_next_hop(switch_id);

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
    status = g_meta->create(&route_entry, 2, list);
    EXPECT_EQ(SAI_STATUS_SUCCESS, status);

    route_entry.destination.addr_family = SAI_IP_ADDR_FAMILY_IPV6;
    sai_ip6_t ip6 = {0x00, 0x11, 0x22, 0x33,0x44, 0x55, 0x66,0x77, 0x88, 0x99, 0xaa, 0xbb,0xcc,0xdd,0xee,0xff};
    memcpy(route_entry.destination.addr.ip6, ip6, 16);

    sai_ip6_t ip6mask = {0xff, 0xff, 0xff, 0xff,0xff, 0xff, 0xff,0xff, 0xff, 0xff, 0xff, 0xff,0xff,0xff,0xff,0x00};
    memcpy(route_entry.destination.mask.ip6, ip6mask, 16);

    SWSS_LOG_NOTICE("correct ipv6");
    status = g_meta->create(&route_entry, 2, list);
    EXPECT_EQ(SAI_STATUS_SUCCESS, status);

    SWSS_LOG_NOTICE("zero attribute count");
    attr.id = SAI_ROUTE_ENTRY_ATTR_PACKET_ACTION;
    status = g_meta->get(&route_entry, 0, &attr);
    EXPECT_NE(SAI_STATUS_SUCCESS, status);

    SWSS_LOG_NOTICE("attr is null");
    status = g_meta->get(&route_entry, 1, NULL);
    EXPECT_NE(SAI_STATUS_SUCCESS, status);

    SWSS_LOG_NOTICE("route entry is null");
    status = g_meta->get((sai_route_entry_t*)NULL, 1, &attr);
    EXPECT_NE(SAI_STATUS_SUCCESS, status);

    SWSS_LOG_NOTICE("attr id out of range");
    attr.id = -1;
    status = g_meta->get(&route_entry, 1, &attr);
    EXPECT_NE(SAI_STATUS_SUCCESS, status);

    SWSS_LOG_NOTICE("correct packet action");
    attr.id = SAI_ROUTE_ENTRY_ATTR_PACKET_ACTION;
    status = g_meta->get(&route_entry, 1, &attr);
    EXPECT_EQ(SAI_STATUS_SUCCESS, status);

    SWSS_LOG_NOTICE("correct next hop");
    attr.id = SAI_ROUTE_ENTRY_ATTR_NEXT_HOP_ID;
    status = g_meta->get(&route_entry, 1, &attr);
    EXPECT_EQ(SAI_STATUS_SUCCESS, status);

    SWSS_LOG_NOTICE("correct meta");
    attr.id = SAI_ROUTE_ENTRY_ATTR_META_DATA;
    status = g_meta->get(&route_entry, 1, &attr);
    EXPECT_EQ(SAI_STATUS_SUCCESS, status);

    remove_switch(switch_id);
}

TEST(LegacyRouteEntry, route_entry_flow)
{
    SWSS_LOG_ENTER();

    clear_local();

    sai_status_t    status;
    sai_attribute_t attr;

    sai_route_entry_t route_entry;
    sai_object_id_t switch_id = create_switch();

    sai_object_id_t vr = create_virtual_router(switch_id);
    sai_object_id_t hop = create_next_hop(switch_id);

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
    status = g_meta->create(&route_entry, 2, list);
    EXPECT_EQ(SAI_STATUS_SUCCESS, status);

    SWSS_LOG_NOTICE("already exists ipv4");
    route_entry.destination.addr_family = SAI_IP_ADDR_FAMILY_IPV4;
    status = g_meta->create(&route_entry, 2, list);
    EXPECT_NE(SAI_STATUS_SUCCESS, status);

    route_entry.destination.addr_family = SAI_IP_ADDR_FAMILY_IPV6;
    sai_ip6_t ip62 = {0x00, 0x11, 0x22, 0x33,0x44, 0x55, 0x66,0x77, 0x88, 0x99, 0xaa, 0xbb,0xcc,0xdd,0xee,0x99};
    memcpy(route_entry.destination.addr.ip6, ip62, 16);

    sai_ip6_t ip6mask2 = {0xff, 0xff, 0xff, 0xff,0xff, 0xff, 0xff,0xff, 0xff, 0xff, 0xff, 0xff,0xff,0xff,0xf7,0x00};
    memcpy(route_entry.destination.mask.ip6, ip6mask2, 16);

    SWSS_LOG_NOTICE("invalid mask");
    status = g_meta->create(&route_entry, 2, list);
    EXPECT_NE(SAI_STATUS_SUCCESS, status);

    route_entry.destination.addr_family = SAI_IP_ADDR_FAMILY_IPV6;
    sai_ip6_t ip6 = {0x00, 0x11, 0x22, 0x33,0x44, 0x55, 0x66,0x77, 0x88, 0x99, 0xaa, 0xbb,0xcc,0xdd,0xee,0xff};
    memcpy(route_entry.destination.addr.ip6, ip6, 16);

    sai_ip6_t ip6mask = {0xff, 0xff, 0xff, 0xff,0xff, 0xff, 0xff,0xff, 0xff, 0xff, 0xff, 0xff,0xff,0xff,0xff,0x00};
    memcpy(route_entry.destination.mask.ip6, ip6mask, 16);

    SWSS_LOG_NOTICE("correct ipv6");
    status = g_meta->create(&route_entry, 2, list);
    EXPECT_EQ(SAI_STATUS_SUCCESS, status);

    SWSS_LOG_NOTICE("already exists");
    status = g_meta->create(&route_entry, 2, list);
    EXPECT_NE(SAI_STATUS_SUCCESS, status);

    SWSS_LOG_NOTICE("set tests");

    SWSS_LOG_NOTICE("correct packet action");
    attr.id = SAI_ROUTE_ENTRY_ATTR_PACKET_ACTION;
    attr.value.s32 = SAI_PACKET_ACTION_DROP;
    status = g_meta->set(&route_entry, &attr);
    EXPECT_EQ(SAI_STATUS_SUCCESS, status);

    SWSS_LOG_NOTICE("correct next hop");
    attr.id = SAI_ROUTE_ENTRY_ATTR_NEXT_HOP_ID;
    attr.value.oid = hop;
    status = g_meta->set(&route_entry, &attr);
    EXPECT_EQ(SAI_STATUS_SUCCESS, status);

    SWSS_LOG_NOTICE("correct metadata");
    attr.id = SAI_ROUTE_ENTRY_ATTR_META_DATA;
    attr.value.u32 = 0x12345678;
    status = g_meta->set(&route_entry, &attr);
    EXPECT_EQ(SAI_STATUS_SUCCESS, status);

    SWSS_LOG_NOTICE("get tests");

    SWSS_LOG_NOTICE("correct packet action");
    attr.id = SAI_ROUTE_ENTRY_ATTR_PACKET_ACTION;
    status = g_meta->get(&route_entry, 1, &attr);
    EXPECT_EQ(SAI_STATUS_SUCCESS, status);

    SWSS_LOG_NOTICE("correct next hop");
    attr.id = SAI_ROUTE_ENTRY_ATTR_NEXT_HOP_ID;
    status = g_meta->get(&route_entry, 1, &attr);
    EXPECT_EQ(SAI_STATUS_SUCCESS, status);

    SWSS_LOG_NOTICE("correct meta");
    attr.id = SAI_ROUTE_ENTRY_ATTR_META_DATA;
    status = g_meta->get(&route_entry, 1, &attr);
    EXPECT_EQ(SAI_STATUS_SUCCESS, status);

    SWSS_LOG_NOTICE("remove tests");

    SWSS_LOG_NOTICE("success");
    status = g_meta->remove(&route_entry);
    EXPECT_EQ(SAI_STATUS_SUCCESS, status);

    SWSS_LOG_NOTICE("non existing");
    status = g_meta->remove(&route_entry);
    EXPECT_NE(SAI_STATUS_SUCCESS, status);

    route_entry.destination.addr_family = SAI_IP_ADDR_FAMILY_IPV4;
    route_entry.destination.addr.ip4 = htonl(0x0a00000f);
    route_entry.destination.mask.ip4 = htonl(0xffffff00);

    SWSS_LOG_NOTICE("success");
    status = g_meta->remove(&route_entry);
    EXPECT_EQ(SAI_STATUS_SUCCESS, status);

    SWSS_LOG_NOTICE("non existing");
    status = g_meta->remove(&route_entry);
    EXPECT_NE(SAI_STATUS_SUCCESS, status);

    remove_switch(switch_id);
}
