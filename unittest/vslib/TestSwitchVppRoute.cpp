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
