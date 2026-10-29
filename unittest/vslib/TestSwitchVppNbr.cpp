#include "SwitchVppTestHarness.h"

#include "meta/sai_serialize.h"

#include <arpa/inet.h>

#include <cstring>
#include <string>

using namespace saivs;
using namespace saivs_test;

namespace
{
    const sai_mac_t BROADCAST_MAC = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
    const sai_mac_t UNICAST_MAC = { 0x00, 0x11, 0x22, 0x33, 0x44, 0x55 };

    /*
     * A VLAN router interface on bvi100, which is all the neighbor path reads to
     * resolve the VPP interface. The RIF is created with create_internal() so no
     * BVI or bridge domain is programmed: these tests are about what the neighbor
     * code sends to VPP, not about RIF creation.
     */
    class SwitchVppNbrTest : public SwitchVppTest
    {
        protected:

            void SetUp() override
            {
                SwitchVppTest::SetUp();

                m_vlan = m_mgr->allocateNewObjectId(SAI_OBJECT_TYPE_VLAN, m_switchId);

                sai_attribute_t attr {};

                attr.id = SAI_VLAN_ATTR_VLAN_ID;
                attr.value.u16 = 100;

                ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->create_internal(
                            SAI_OBJECT_TYPE_VLAN, sai_serialize_object_id(m_vlan),
                            m_switchId, 1, &attr));

                m_rif = m_mgr->allocateNewObjectId(SAI_OBJECT_TYPE_ROUTER_INTERFACE, m_switchId);

                sai_attribute_t rifAttrs[3] {};

                rifAttrs[0].id = SAI_ROUTER_INTERFACE_ATTR_TYPE;
                rifAttrs[0].value.s32 = SAI_ROUTER_INTERFACE_TYPE_VLAN;
                rifAttrs[1].id = SAI_ROUTER_INTERFACE_ATTR_VLAN_ID;
                rifAttrs[1].value.oid = m_vlan;
                rifAttrs[2].id = SAI_ROUTER_INTERFACE_ATTR_VIRTUAL_ROUTER_ID;
                rifAttrs[2].value.oid = m_defaultVr;

                ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->create_internal(
                            SAI_OBJECT_TYPE_ROUTER_INTERFACE, sai_serialize_object_id(m_rif),
                            m_switchId, 3, rifAttrs));

                vppMock().calls.clear();
            }

            std::string neighborId(
                    const char *ip) const
            {
                sai_neighbor_entry_t entry {};

                entry.switch_id = m_switchId;
                entry.rif_id = m_rif;
                entry.ip_address.addr_family = SAI_IP_ADDR_FAMILY_IPV4;

                EXPECT_EQ(1, inet_pton(AF_INET, ip, &entry.ip_address.addr.ip4));

                return sai_serialize_neighbor_entry(entry);
            }

            sai_status_t addNeighbor(
                    const char *ip,
                    const sai_mac_t mac)
            {
                sai_attribute_t attr {};

                attr.id = SAI_NEIGHBOR_ENTRY_ATTR_DST_MAC_ADDRESS;
                memcpy(attr.value.mac, mac, sizeof(sai_mac_t));

                return m_sw->addIpNbr(neighborId(ip), m_switchId, 1, &attr);
            }

            sai_status_t removeNeighbor(
                    const char *ip)
            {
                return m_sw->removeIpNbr(neighborId(ip));
            }

            bool neighborExists(
                    const char *ip) const
            {
                auto it = m_sw->m_objectHash.find(SAI_OBJECT_TYPE_NEIGHBOR_ENTRY);

                return it != m_sw->m_objectHash.end() && it->second.count(neighborId(ip)) != 0;
            }

            std::vector<VppCall> directedBcastCalls() const
            {
                return vppCallsTo("sw_interface_set_ip_directed_broadcast");
            }

            sai_object_id_t m_vlan = SAI_NULL_OBJECT_ID;
            sai_object_id_t m_rif = SAI_NULL_OBJECT_ID;
    };
}

/*
 * IntfsOrch programs one subnet broadcast neighbor per subnet, so a VLAN with
 * two subnets puts two of them on the same BVI. The interface flag is one bit,
 * so it is enabled on the first and only disabled once the last one is gone.
 */
TEST_F(SwitchVppNbrTest, broadcastNeighborsShareOneDirectedBroadcastFlag)
{
    ASSERT_EQ(SAI_STATUS_SUCCESS, addNeighbor("192.168.1.255", BROADCAST_MAC));
    ASSERT_EQ(SAI_STATUS_SUCCESS, addNeighbor("192.168.2.255", BROADCAST_MAC));

    auto calls = directedBcastCalls();

    ASSERT_EQ(1u, calls.size());
    EXPECT_EQ("bvi100", calls[0].name);
    EXPECT_TRUE(calls[0].flag);
    EXPECT_EQ(2u, m_sw->m_directed_bcast_hwif.at("bvi100"));

    // VPP owns these /32s itself, so neither neighbor programs an adjacency or a
    // host route.
    EXPECT_TRUE(vppCallsTo("ip4_nbr_add_del").empty());
    EXPECT_TRUE(vppCallsTo("ip_route_add_del").empty());

    ASSERT_EQ(SAI_STATUS_SUCCESS, removeNeighbor("192.168.1.255"));

    EXPECT_EQ(1u, directedBcastCalls().size());
    EXPECT_EQ(1u, m_sw->m_directed_bcast_hwif.at("bvi100"));

    ASSERT_EQ(SAI_STATUS_SUCCESS, removeNeighbor("192.168.2.255"));

    calls = directedBcastCalls();

    ASSERT_EQ(2u, calls.size());
    EXPECT_EQ("bvi100", calls[1].name);
    EXPECT_FALSE(calls[1].flag);
    EXPECT_EQ(0u, m_sw->m_directed_bcast_hwif.count("bvi100"));
}

/*
 * A failed enable must leave no trace: the count is only committed once VPP has
 * taken the flag, and the neighbor object created before this pass is removed
 * again, so orchagent's retry is a plain create rather than one that collides
 * with an object it was told had failed.
 */
TEST_F(SwitchVppNbrTest, failedDirectedBroadcastEnableIsRetryable)
{
    vppMock().directedBcastResult = -1;

    ASSERT_EQ(SAI_STATUS_FAILURE, addNeighbor("192.168.1.255", BROADCAST_MAC));

    ASSERT_EQ(1u, directedBcastCalls().size());
    EXPECT_EQ(0u, m_sw->m_directed_bcast_hwif.count("bvi100"));
    EXPECT_FALSE(neighborExists("192.168.1.255"));

    vppMock().directedBcastResult = 0;

    ASSERT_EQ(SAI_STATUS_SUCCESS, addNeighbor("192.168.1.255", BROADCAST_MAC));

    auto calls = directedBcastCalls();

    ASSERT_EQ(2u, calls.size());
    EXPECT_TRUE(calls[1].flag);
    EXPECT_EQ(1u, m_sw->m_directed_bcast_hwif.at("bvi100"));
    EXPECT_TRUE(neighborExists("192.168.1.255"));
}

/*
 * Only an all-ones MAC is a subnet broadcast: an ordinary neighbor keeps its
 * adjacency and host route and never touches the interface flag.
 */
TEST_F(SwitchVppNbrTest, unicastNeighborDoesNotTouchDirectedBroadcast)
{
    ASSERT_EQ(SAI_STATUS_SUCCESS, addNeighbor("192.168.1.10", UNICAST_MAC));

    EXPECT_TRUE(directedBcastCalls().empty());
    EXPECT_TRUE(m_sw->m_directed_bcast_hwif.empty());

    auto nbrCalls = vppCallsTo("ip4_nbr_add_del");
    auto routeCalls = vppCallsTo("ip_route_add_del");

    ASSERT_EQ(1u, nbrCalls.size());
    EXPECT_EQ("bvi100", nbrCalls[0].name);
    EXPECT_TRUE(nbrCalls[0].flag);

    ASSERT_EQ(1u, routeCalls.size());
    EXPECT_EQ(32u, routeCalls[0].id);
    EXPECT_TRUE(routeCalls[0].flag);

    ASSERT_EQ(SAI_STATUS_SUCCESS, removeNeighbor("192.168.1.10"));

    EXPECT_TRUE(directedBcastCalls().empty());
    EXPECT_EQ(2u, vppCallsTo("ip4_nbr_add_del").size());
    EXPECT_EQ(2u, vppCallsTo("ip_route_add_del").size());
}
