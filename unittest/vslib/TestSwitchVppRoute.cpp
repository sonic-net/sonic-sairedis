#include "SwitchVppTestHarness.h"
#include "meta/sai_serialize.h"

using namespace saivs;
using namespace saivs_test;

class SwitchVppRouteTest : public SwitchVppTest, public ::testing::WithParamInterface<bool>
{
protected:
    void SetUp() override
    {
        SwitchVppTest::SetUp();
        m_sw->nbr_env_read = true;
        m_sw->nbr_active = true;

        auto port = m_mgr->allocateNewObjectId(SAI_OBJECT_TYPE_PORT, m_switchId);
        ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->create_internal(
                SAI_OBJECT_TYPE_PORT, sai_serialize_object_id(port), m_switchId, 0, nullptr));
        ASSERT_NE(nullptr, m_sw->m_ifaceRegistry.addPhysicalPort("bobm0", "Ethernet0", port));

        m_rif = m_mgr->allocateNewObjectId(SAI_OBJECT_TYPE_ROUTER_INTERFACE, m_switchId);
        sai_attribute_t rif_attrs[3] = {};
        rif_attrs[0].id = SAI_ROUTER_INTERFACE_ATTR_TYPE;
        rif_attrs[0].value.s32 = SAI_ROUTER_INTERFACE_TYPE_PORT;
        rif_attrs[1].id = SAI_ROUTER_INTERFACE_ATTR_PORT_ID;
        rif_attrs[1].value.oid = port;
        rif_attrs[2].id = SAI_ROUTER_INTERFACE_ATTR_VIRTUAL_ROUTER_ID;
        rif_attrs[2].value.oid = m_defaultVr;
        ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->create_internal(
                SAI_OBJECT_TYPE_ROUTER_INTERFACE, sai_serialize_object_id(m_rif), m_switchId, 3, rif_attrs));

        ASSERT_NO_FATAL_FAILURE(addNextHop(GetParam() ? "2001:db8::1" : "192.0.2.1", m_oldNextHop));
        m_newAddress = GetParam() ? "2001:db8::2" : "192.0.2.2";
        ASSERT_NO_FATAL_FAILURE(addNextHop(m_newAddress, m_newNextHop));

        sai_route_entry_t route = {};
        route.switch_id = m_switchId;
        route.vr_id = m_defaultVr;
        sai_deserialize_ip_prefix(GetParam() ? "2001:db8:1::1/128" : "198.51.100.1/32", route.destination);
        m_route = sai_serialize_route_entry(route);

        sai_attribute_t attrs[2] = {};
        attrs[0].id = SAI_ROUTE_ENTRY_ATTR_NEXT_HOP_ID;
        attrs[0].value.oid = m_oldNextHop;
        attrs[1].id = SAI_ROUTE_ENTRY_ATTR_PACKET_ACTION;
        attrs[1].value.s32 = SAI_PACKET_ACTION_FORWARD;
        ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->create_internal(
                SAI_OBJECT_TYPE_ROUTE_ENTRY, m_route, m_switchId, 2, attrs));
        m_sw->m_routeStatsIndexMap[m_route] = vppMock().routeStatsIndex;
        vppMock().routeCalls.clear();
    }

    void addNextHop(const std::string& address, sai_object_id_t& next_hop)
    {
        SWSS_LOG_ENTER();
        next_hop = m_mgr->allocateNewObjectId(SAI_OBJECT_TYPE_NEXT_HOP, m_switchId);
        sai_attribute_t attrs[3] = {};
        attrs[0].id = SAI_NEXT_HOP_ATTR_TYPE;
        attrs[0].value.s32 = SAI_NEXT_HOP_TYPE_IP;
        attrs[1].id = SAI_NEXT_HOP_ATTR_IP;
        sai_deserialize_ip_address(address, attrs[1].value.ipaddr);
        attrs[2].id = SAI_NEXT_HOP_ATTR_ROUTER_INTERFACE_ID;
        attrs[2].value.oid = m_rif;
        ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->create_internal(
                SAI_OBJECT_TYPE_NEXT_HOP, sai_serialize_object_id(next_hop), m_switchId, 3, attrs));
    }

    sai_status_t updateNextHop(sai_object_id_t next_hop)
    {
        SWSS_LOG_ENTER();
        sai_attribute_t attr = {};
        attr.id = SAI_ROUTE_ENTRY_ATTR_NEXT_HOP_ID;
        attr.value.oid = next_hop;
        return m_sw->updateIpRoute(m_route, &attr);
    }

    void expectNextHop(sai_object_id_t next_hop)
    {
        SWSS_LOG_ENTER();
        sai_attribute_t attr = {};
        attr.id = SAI_ROUTE_ENTRY_ATTR_NEXT_HOP_ID;
        ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->get(
                SAI_OBJECT_TYPE_ROUTE_ENTRY, m_route, 1, &attr));
        EXPECT_EQ(next_hop, attr.value.oid);
        EXPECT_EQ(vppMock().routeStatsIndex, m_sw->m_routeStatsIndexMap.at(m_route));
    }

    sai_object_id_t m_rif = SAI_NULL_OBJECT_ID;
    sai_object_id_t m_oldNextHop = SAI_NULL_OBJECT_ID;
    sai_object_id_t m_newNextHop = SAI_NULL_OBJECT_ID;
    std::string m_route;
    std::string m_newAddress;
};

TEST_P(SwitchVppRouteTest, ReplacesNextHopWithoutWithdrawingRoute)
{
    ASSERT_EQ(SAI_STATUS_SUCCESS, updateNextHop(m_newNextHop));
    ASSERT_EQ(1u, vppMock().routeCalls.size());
    const auto& call = vppMock().routeCalls.front();
    EXPECT_TRUE(call.isAdd);
    EXPECT_FALSE(call.isMultipath);
    EXPECT_EQ(1u, call.nextHopCount);
    EXPECT_EQ(m_newAddress, call.nextHop);
    expectNextHop(m_newNextHop);
}

TEST_P(SwitchVppRouteTest, FailedReplacementKeepsCommittedRoute)
{
    vppMock().routeResults = {-1};
    EXPECT_EQ(SAI_STATUS_FAILURE, updateNextHop(m_newNextHop));
    ASSERT_EQ(1u, vppMock().routeCalls.size());
    EXPECT_TRUE(vppMock().routeCalls.front().isAdd);
    expectNextHop(m_oldNextHop);
}

TEST_P(SwitchVppRouteTest, PacketActionChangeKeepsDeleteAddPath)
{
    sai_attribute_t attr = {};
    attr.id = SAI_ROUTE_ENTRY_ATTR_PACKET_ACTION;
    attr.value.s32 = SAI_PACKET_ACTION_DROP;
    ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->updateIpRoute(m_route, &attr));
    ASSERT_EQ(2u, vppMock().routeCalls.size());
    EXPECT_FALSE(vppMock().routeCalls[0].isAdd);
    EXPECT_TRUE(vppMock().routeCalls[1].isAdd);
}

TEST_P(SwitchVppRouteTest, EcmpNextHopKeepsDeleteAddPath)
{
    auto group = m_mgr->allocateNewObjectId(SAI_OBJECT_TYPE_NEXT_HOP_GROUP, m_switchId);
    sai_attribute_t group_attr = {};
    group_attr.id = SAI_NEXT_HOP_GROUP_ATTR_TYPE;
    group_attr.value.s32 = SAI_NEXT_HOP_GROUP_TYPE_DYNAMIC_UNORDERED_ECMP;
    ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->create_internal(
            SAI_OBJECT_TYPE_NEXT_HOP_GROUP, sai_serialize_object_id(group), m_switchId, 1, &group_attr));

    for (auto next_hop : {m_oldNextHop, m_newNextHop})
    {
        auto member = m_mgr->allocateNewObjectId(SAI_OBJECT_TYPE_NEXT_HOP_GROUP_MEMBER, m_switchId);
        sai_attribute_t attrs[2] = {};
        attrs[0].id = SAI_NEXT_HOP_GROUP_MEMBER_ATTR_NEXT_HOP_GROUP_ID;
        attrs[0].value.oid = group;
        attrs[1].id = SAI_NEXT_HOP_GROUP_MEMBER_ATTR_NEXT_HOP_ID;
        attrs[1].value.oid = next_hop;
        ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->create_internal(
                SAI_OBJECT_TYPE_NEXT_HOP_GROUP_MEMBER, sai_serialize_object_id(member), m_switchId, 2, attrs));
    }
    ASSERT_EQ(SAI_STATUS_SUCCESS, updateNextHop(group));
    ASSERT_EQ(2u, vppMock().routeCalls.size());
    EXPECT_FALSE(vppMock().routeCalls[0].isAdd);
    EXPECT_TRUE(vppMock().routeCalls[1].isAdd);
    EXPECT_TRUE(vppMock().routeCalls[1].isMultipath);
    EXPECT_EQ(2u, vppMock().routeCalls[1].nextHopCount);
    expectNextHop(group);
}

INSTANTIATE_TEST_SUITE_P(AddressFamilies, SwitchVppRouteTest, ::testing::Bool());

class SwitchVppSharedVxlanEncapTest : public SwitchVppTest
{
protected:
    void SetUp() override
    {
        SwitchVppTest::SetUp();

        sai_attribute_t sw_mac_attr = {};
        sw_mac_attr.id = SAI_SWITCH_ATTR_SRC_MAC_ADDRESS;
        const sai_mac_t sw_mac = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55};
        memcpy(sw_mac_attr.value.mac, sw_mac, sizeof(sai_mac_t));
        ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->set_internal(
                SAI_OBJECT_TYPE_SWITCH, sai_serialize_object_id(m_switchId), &sw_mac_attr));

        vppMock().rejectDuplicateVxlanTunnels = true;
        vppMock().calls.clear();
    }

    sai_object_id_t addVxlanTunnel(
            const std::string& srcIp,
            sai_object_id_t encapMapper = SAI_NULL_OBJECT_ID,
            bool p2p = false)
    {
        SWSS_LOG_ENTER();
        auto tunnel = m_mgr->allocateNewObjectId(SAI_OBJECT_TYPE_TUNNEL, m_switchId);
        sai_attribute_t attrs[5] = {};
        uint32_t count = 2;
        attrs[0].id = SAI_TUNNEL_ATTR_TYPE;
        attrs[0].value.s32 = SAI_TUNNEL_TYPE_VXLAN;
        attrs[1].id = SAI_TUNNEL_ATTR_ENCAP_SRC_IP;
        sai_deserialize_ip_address(srcIp, attrs[1].value.ipaddr);
        if (encapMapper != SAI_NULL_OBJECT_ID)
        {
            attrs[count].id = SAI_TUNNEL_ATTR_ENCAP_MAPPERS;
            attrs[count].value.objlist.count = 1;
            attrs[count].value.objlist.list = &encapMapper;
            count++;
        }
        if (p2p)
        {
            attrs[count].id = SAI_TUNNEL_ATTR_PEER_MODE;
            attrs[count].value.s32 = SAI_TUNNEL_PEER_MODE_P2P;
            count++;
            attrs[count].id = SAI_TUNNEL_ATTR_ENCAP_DST_IP;
            sai_deserialize_ip_address("192.168.102.13", attrs[count].value.ipaddr);
            count++;
        }
        EXPECT_EQ(SAI_STATUS_SUCCESS, m_sw->create(
                SAI_OBJECT_TYPE_TUNNEL, sai_serialize_object_id(tunnel), m_switchId, count, attrs));
        return tunnel;
    }

    sai_object_id_t addEncapNextHop(
            sai_object_id_t tunnel,
            const std::string& dstIp,
            const sai_mac_t mac,
            uint32_t vni = 0)
    {
        SWSS_LOG_ENTER();
        auto nh = m_mgr->allocateNewObjectId(SAI_OBJECT_TYPE_NEXT_HOP, m_switchId);
        sai_attribute_t attrs[5] = {};
        uint32_t count = 4;
        attrs[0].id = SAI_NEXT_HOP_ATTR_TYPE;
        attrs[0].value.s32 = SAI_NEXT_HOP_TYPE_TUNNEL_ENCAP;
        attrs[1].id = SAI_NEXT_HOP_ATTR_TUNNEL_ID;
        attrs[1].value.oid = tunnel;
        attrs[2].id = SAI_NEXT_HOP_ATTR_IP;
        sai_deserialize_ip_address(dstIp, attrs[2].value.ipaddr);
        attrs[3].id = SAI_NEXT_HOP_ATTR_TUNNEL_MAC;
        memcpy(attrs[3].value.mac, mac, sizeof(sai_mac_t));
        if (vni != 0)
        {
            attrs[4].id = SAI_NEXT_HOP_ATTR_TUNNEL_VNI;
            attrs[4].value.u32 = vni;
            count = 5;
        }
        EXPECT_EQ(SAI_STATUS_SUCCESS, m_sw->create(
                SAI_OBJECT_TYPE_NEXT_HOP, sai_serialize_object_id(nh), m_switchId, count, attrs));
        return nh;
    }

    // a VR-to-VNI encap mapper whose next hops carry a VRF decap
    sai_object_id_t addVrfEncapMapper()
    {
        SWSS_LOG_ENTER();
        auto vrf = m_mgr->allocateNewObjectId(SAI_OBJECT_TYPE_VIRTUAL_ROUTER, m_switchId);
        EXPECT_EQ(SAI_STATUS_SUCCESS, m_sw->create_internal(
                SAI_OBJECT_TYPE_VIRTUAL_ROUTER, sai_serialize_object_id(vrf), m_switchId, 0, nullptr));
        // what the VRF's first router interface records
        EXPECT_EQ(0, m_sw->vpp_add_ip_vrf(vrf, 1001));

        auto mapper = m_mgr->allocateNewObjectId(SAI_OBJECT_TYPE_TUNNEL_MAP, m_switchId);
        sai_attribute_t mattr = {};
        mattr.id = SAI_TUNNEL_MAP_ATTR_TYPE;
        mattr.value.s32 = SAI_TUNNEL_MAP_TYPE_VIRTUAL_ROUTER_ID_TO_VNI;
        EXPECT_EQ(SAI_STATUS_SUCCESS, m_sw->create(
                SAI_OBJECT_TYPE_TUNNEL_MAP, sai_serialize_object_id(mapper), m_switchId, 1, &mattr));

        auto mapEntry = m_mgr->allocateNewObjectId(SAI_OBJECT_TYPE_TUNNEL_MAP_ENTRY, m_switchId);
        sai_attribute_t eattrs[4] = {};
        eattrs[0].id = SAI_TUNNEL_MAP_ENTRY_ATTR_TUNNEL_MAP_TYPE;
        eattrs[0].value.s32 = SAI_TUNNEL_MAP_TYPE_VIRTUAL_ROUTER_ID_TO_VNI;
        eattrs[1].id = SAI_TUNNEL_MAP_ENTRY_ATTR_TUNNEL_MAP;
        eattrs[1].value.oid = mapper;
        eattrs[2].id = SAI_TUNNEL_MAP_ENTRY_ATTR_VIRTUAL_ROUTER_ID_KEY;
        eattrs[2].value.oid = vrf;
        eattrs[3].id = SAI_TUNNEL_MAP_ENTRY_ATTR_VNI_ID_VALUE;
        eattrs[3].value.u32 = 4000;
        EXPECT_EQ(SAI_STATUS_SUCCESS, m_sw->create(
                SAI_OBJECT_TYPE_TUNNEL_MAP_ENTRY, sai_serialize_object_id(mapEntry), m_switchId, 4, eattrs));

        return mapper;
    }
};

TEST_F(SwitchVppSharedVxlanEncapTest, SharesTunnelAcrossEncapNextHopsWithDistinctTunnelMacs)
{
    const sai_mac_t mac1 = {0x02, 0x00, 0x00, 0x00, 0x01, 0x01};
    const sai_mac_t mac2 = {0x02, 0x00, 0x00, 0x00, 0x01, 0x02};
    auto tunnel = addVxlanTunnel("192.168.101.13");

    auto nh1 = addEncapNextHop(tunnel, "192.168.102.13", mac1, 3000);
    auto nh2 = addEncapNextHop(tunnel, "192.168.102.13", mac2, 3000);

    auto tcalls = vppCallsTo("vpp_vxlan_tunnel_add_del");
    ASSERT_EQ(1u, tcalls.size());
    EXPECT_TRUE(tcalls[0].flag);
    EXPECT_EQ(3000u, tcalls[0].id);

    uint32_t swif1 = ~0u;
    uint32_t swif2 = ~0u;
    ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->m_tunnel_mgr.get_tunnel_if(nh1, m_defaultVr, swif1));
    ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->m_tunnel_mgr.get_tunnel_if(nh2, m_defaultVr, swif2));
    EXPECT_EQ(swif1, swif2);

    auto ncalls = vppCallsTo("ip4_nbr_add_del");
    ASSERT_EQ(2u, ncalls.size());
    EXPECT_EQ("02:00:00:00:01:01", ncalls[0].name);
    EXPECT_EQ("02:00:00:00:01:02", ncalls[1].name);

    // Removing the active owner keeps the shared tunnel and restores the first next hop's inner MAC
    vppMock().calls.clear();
    ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->remove(SAI_OBJECT_TYPE_NEXT_HOP, sai_serialize_object_id(nh2)));
    EXPECT_TRUE(vppCallsTo("vpp_vxlan_tunnel_add_del").empty());
    ncalls = vppCallsTo("ip4_nbr_add_del");
    ASSERT_EQ(1u, ncalls.size());
    EXPECT_EQ("02:00:00:00:01:01", ncalls[0].name);
    EXPECT_TRUE(ncalls[0].flag);

    // Removing the final owner tears down the VPP tunnel
    vppMock().calls.clear();
    ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->remove(SAI_OBJECT_TYPE_NEXT_HOP, sai_serialize_object_id(nh1)));
    tcalls = vppCallsTo("vpp_vxlan_tunnel_add_del");
    ASSERT_EQ(1u, tcalls.size());
    EXPECT_FALSE(tcalls[0].flag);
    ncalls = vppCallsTo("ip4_nbr_add_del");
    ASSERT_EQ(1u, ncalls.size());
    EXPECT_EQ("02:00:00:00:01:01", ncalls[0].name);
    EXPECT_FALSE(ncalls[0].flag);
}

TEST_F(SwitchVppSharedVxlanEncapTest, FailedLastOwnerTunnelDeleteKeepsEntryForRetry)
{
    const sai_mac_t mac1 = {0x02, 0x00, 0x00, 0x00, 0x01, 0x01};
    auto tunnel = addVxlanTunnel("192.168.101.13");
    auto nh1 = addEncapNextHop(tunnel, "192.168.102.13", mac1, 3000);

    vppMock().vxlanTunnelDelResult = -1;
    EXPECT_EQ(SAI_STATUS_FAILURE, m_sw->remove(SAI_OBJECT_TYPE_NEXT_HOP, sai_serialize_object_id(nh1)));
    EXPECT_EQ(1u, m_sw->m_objectHash.at(SAI_OBJECT_TYPE_NEXT_HOP).count(sai_serialize_object_id(nh1)));

    vppMock().vxlanTunnelDelResult = 0;
    EXPECT_EQ(SAI_STATUS_SUCCESS, m_sw->remove(SAI_OBJECT_TYPE_NEXT_HOP, sai_serialize_object_id(nh1)));
    EXPECT_EQ(0u, m_sw->m_objectHash.at(SAI_OBJECT_TYPE_NEXT_HOP).count(sai_serialize_object_id(nh1)));
}

TEST_F(SwitchVppSharedVxlanEncapTest, FailedLastOwnerTunnelDeleteRestoresVrfDecap)
{
    auto mapper = addVrfEncapMapper();
    auto tunnel = addVxlanTunnel("192.168.101.13", mapper);
    const sai_mac_t mac1 = {0x02, 0x00, 0x00, 0x00, 0x03, 0x01};
    auto nh1 = addEncapNextHop(tunnel, "192.168.102.13", mac1);
    ASSERT_EQ(1u, vppMock().interfaces.size());

    vppMock().vxlanTunnelDelResult = -1;
    EXPECT_EQ(SAI_STATUS_FAILURE, m_sw->remove(SAI_OBJECT_TYPE_NEXT_HOP, sai_serialize_object_id(nh1)));
    EXPECT_EQ(1u, m_sw->m_objectHash.at(SAI_OBJECT_TYPE_NEXT_HOP).count(sai_serialize_object_id(nh1)));
    EXPECT_EQ(1u, vppMock().interfaces.size());

    vppMock().vxlanTunnelDelResult = 0;
    EXPECT_EQ(SAI_STATUS_SUCCESS, m_sw->remove(SAI_OBJECT_TYPE_NEXT_HOP, sai_serialize_object_id(nh1)));
    EXPECT_EQ(0u, m_sw->m_objectHash.at(SAI_OBJECT_TYPE_NEXT_HOP).count(sai_serialize_object_id(nh1)));
    EXPECT_TRUE(vppMock().interfaces.empty());
}

TEST_F(SwitchVppSharedVxlanEncapTest, RefcountsVrfDecapAcrossSharedMapperNextHops)
{
    auto mapper = addVrfEncapMapper();
    auto tunnel = addVxlanTunnel("192.168.101.13", mapper);
    vppMock().calls.clear();
    vppMock().interfaceCreates = 0;

    const sai_mac_t mac1 = {0x02, 0x00, 0x00, 0x00, 0x02, 0x01};
    const sai_mac_t mac2 = {0x02, 0x00, 0x00, 0x00, 0x02, 0x02};
    auto nh1 = addEncapNextHop(tunnel, "192.168.102.13", mac1);
    auto nh2 = addEncapNextHop(tunnel, "192.168.102.13", mac2);

    EXPECT_EQ(1u, vppCallsTo("vpp_vxlan_tunnel_add_del").size());
    EXPECT_EQ(1u, vppMock().interfaceCreates);
    EXPECT_EQ(1u, vppMock().interfaces.size());

    vppMock().calls.clear();
    ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->remove(SAI_OBJECT_TYPE_NEXT_HOP, sai_serialize_object_id(nh1)));
    EXPECT_EQ(1u, vppMock().interfaces.size());
    EXPECT_TRUE(vppCallsTo("vpp_vxlan_tunnel_add_del").empty());

    ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->remove(SAI_OBJECT_TYPE_NEXT_HOP, sai_serialize_object_id(nh2)));
    EXPECT_TRUE(vppMock().interfaces.empty());
    ASSERT_EQ(1u, vppCallsTo("vpp_vxlan_tunnel_add_del").size());
    EXPECT_FALSE(vppCallsTo("vpp_vxlan_tunnel_add_del")[0].flag);
}

TEST_F(SwitchVppSharedVxlanEncapTest, FailedMacRestoreKeepsSharedVrfDecapForRetry)
{
    auto encapTunnel = addVxlanTunnel("192.168.101.13");
    auto mapper = addVrfEncapMapper();
    auto vrfTunnel = addVxlanTunnel("192.168.101.13", mapper, true);

    const sai_mac_t mac1 = {0x02, 0x00, 0x00, 0x00, 0x04, 0x01};
    const sai_mac_t mac2 = {0x02, 0x00, 0x00, 0x00, 0x04, 0x02};
    auto nh1 = addEncapNextHop(encapTunnel, "192.168.102.13", mac1, 4000);
    auto nh2 = addEncapNextHop(vrfTunnel, "192.168.102.13", mac2);
    ASSERT_EQ(1u, vppMock().interfaces.size());

    vppMock().nbrAddDelResult = -1;
    EXPECT_EQ(SAI_STATUS_FAILURE, m_sw->remove(SAI_OBJECT_TYPE_NEXT_HOP, sai_serialize_object_id(nh2)));
    EXPECT_EQ(1u, vppMock().interfaces.size());

    vppMock().nbrAddDelResult = 0;
    EXPECT_EQ(SAI_STATUS_SUCCESS, m_sw->remove(SAI_OBJECT_TYPE_NEXT_HOP, sai_serialize_object_id(nh2)));
    EXPECT_TRUE(vppMock().interfaces.empty());
    EXPECT_EQ(SAI_STATUS_SUCCESS, m_sw->remove(SAI_OBJECT_TYPE_NEXT_HOP, sai_serialize_object_id(nh1)));
}
