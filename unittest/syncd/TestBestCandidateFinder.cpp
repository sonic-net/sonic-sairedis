#include "BestCandidateFinder.h"
#include "MockableSaiSwitchInterface.h"

#include "meta/sai_serialize.h"

#include <gtest/gtest.h>

#include <inttypes.h>
#include <map>

using namespace syncd;
using namespace unittests;

TEST(BestCandidateFinder, getSaiAttrFromDefaultValue)
{
    AsicView av;

    auto *meta = sai_metadata_get_attr_metadata(
            SAI_OBJECT_TYPE_SWITCH,
            SAI_SWITCH_ATTR_VXLAN_DEFAULT_ROUTER_MAC);

    EXPECT_NE(meta, nullptr);

    auto sw = std::make_shared<MockableSaiSwitchInterface>(0,0);

    auto attr = BestCandidateFinder::getSaiAttrFromDefaultValue(av, sw, *meta);
    EXPECT_NE(attr, nullptr);
}

/*
 * Two bridge port next hops to the same remote tunnel endpoint, one per
 * bridge port next hop group, have equal attributes. Group 1 = {leaf2},
 * group 2 = {leaf2, leaf3} (an ES backup group and a remote ES group).
 */
static swss::TableDump getL2NhgDump(
        _In_ uint64_t base,
        _In_ bool sameShape)
{
    SWSS_LOG_ENTER();

    auto oid = [base](uint64_t type, uint64_t idx)
    {
        char buf[32];
        snprintf(buf, sizeof(buf), "oid:0x%" PRIx64, (type << 48) | (base + idx));
        return std::string(buf);
    };

    auto nh = [&](const std::string& ip) -> std::map<std::string, std::string>
    {
        return {
            {"SAI_NEXT_HOP_ATTR_TYPE", "SAI_NEXT_HOP_TYPE_BRIDGE_PORT"},
            {"SAI_NEXT_HOP_ATTR_IP", ip},
            {"SAI_NEXT_HOP_ATTR_TUNNEL_ID", "oid:0x2a000000000031"},
        };
    };

    auto nhgm = [&](uint64_t group, uint64_t nexthop) -> std::map<std::string, std::string>
    {
        return {
            {"SAI_NEXT_HOP_GROUP_MEMBER_ATTR_NEXT_HOP_GROUP_ID", oid(5, group)},
            {"SAI_NEXT_HOP_GROUP_MEMBER_ATTR_NEXT_HOP_ID", oid(4, nexthop)},
        };
    };

    const std::map<std::string, std::string> nhg = {
        {"SAI_NEXT_HOP_GROUP_ATTR_TYPE", "SAI_NEXT_HOP_GROUP_TYPE_BRIDGE_PORT"},
    };

    swss::TableDump dump = {
        {"SAI_OBJECT_TYPE_SWITCH:oid:0x21000000000000", {{"SAI_SWITCH_ATTR_INIT_SWITCH", "true"}}},
        {"SAI_OBJECT_TYPE_TUNNEL:oid:0x2a000000000031", {{"SAI_TUNNEL_ATTR_TYPE", "SAI_TUNNEL_TYPE_VXLAN"}}},
        {"SAI_OBJECT_TYPE_NEXT_HOP_GROUP:" + oid(5, 1), nhg},
        {"SAI_OBJECT_TYPE_NEXT_HOP_GROUP:" + oid(5, 2), nhg},
        {"SAI_OBJECT_TYPE_NEXT_HOP:" + oid(4, 0x11), nh("10.255.2.2")},
        {"SAI_OBJECT_TYPE_NEXT_HOP:" + oid(4, 0x12), nh("10.255.2.2")},
        {"SAI_OBJECT_TYPE_NEXT_HOP:" + oid(4, 0x13), nh("10.255.3.3")},
        {"SAI_OBJECT_TYPE_NEXT_HOP_GROUP_MEMBER:" + oid(0x2d, 0x21), nhgm(1, 0x11)},
        {"SAI_OBJECT_TYPE_NEXT_HOP_GROUP_MEMBER:" + oid(0x2d, 0x22), nhgm(2, 0x12)},
        {"SAI_OBJECT_TYPE_NEXT_HOP_GROUP_MEMBER:" + oid(0x2d, 0x23), nhgm(2, 0x13)},
    };

    if (sameShape)
    {
        dump["SAI_OBJECT_TYPE_NEXT_HOP:" + oid(4, 0x14)] = nh("10.255.3.3");
        dump["SAI_OBJECT_TYPE_NEXT_HOP_GROUP_MEMBER:" + oid(0x2d, 0x24)] = nhgm(1, 0x14);
    }

    return dump;
}

static void wireL2NhgViews(
        _In_ AsicView& current,
        _In_ AsicView& temp)
{
    SWSS_LOG_ENTER();

    temp.m_vidToRid[0x21000000000000] = 0x2100;
    current.m_ridToVid[0x2100] = 0x21000000000000;
    temp.m_vidToRid[0x2a000000000031] = 0x2a00;
    current.m_ridToVid[0x2a00] = 0x2a000000000031;
}

TEST(BestCandidateFinder, findCurrentBestMatchNextHopByGroupShape)
{
    // current VIDs end in 0x..., temp VIDs are offset by 0x100
    AsicView current;
    current.fromDump(getL2NhgDump(0, false));

    AsicView temp;
    temp.fromDump(getL2NhgDump(0x100, false));

    wireL2NhgViews(current, temp);

    auto sw = std::make_shared<MockableSaiSwitchInterface>(0,0);

    BestCandidateFinder finder(current, temp, sw);

    // without the group comparison the pick between the two leaf2 next hops
    // is random, so repeat it
    for (int i = 0; i < 32; i++)
    {
        auto match = finder.findCurrentBestMatch(temp.m_oOids.at(0x4000000000111));
        ASSERT_NE(match, nullptr);
        EXPECT_EQ(match->getVid(), 0x4000000000011);

        match = finder.findCurrentBestMatch(temp.m_oOids.at(0x4000000000112));
        ASSERT_NE(match, nullptr);
        EXPECT_EQ(match->getVid(), 0x4000000000012);
    }
}

TEST(BestCandidateFinder, findCurrentBestMatchNextHopByMatchedGroup)
{
    // both groups are {leaf2, leaf3}: only the already matched group tells
    // the leaf2 next hops apart
    AsicView current;
    current.fromDump(getL2NhgDump(0, true));

    AsicView temp;
    temp.fromDump(getL2NhgDump(0x100, true));

    wireL2NhgViews(current, temp);

    temp.m_oOids.at(0x5000000000102)->setObjectStatus(SAI_OBJECT_STATUS_FINAL);
    temp.m_vidToRid[0x5000000000102] = 0x5002;
    current.m_ridToVid[0x5002] = 0x5000000000002;

    auto sw = std::make_shared<MockableSaiSwitchInterface>(0,0);

    BestCandidateFinder finder(current, temp, sw);

    for (int i = 0; i < 32; i++)
    {
        auto match = finder.findCurrentBestMatch(temp.m_oOids.at(0x4000000000112));
        ASSERT_NE(match, nullptr);
        EXPECT_EQ(match->getVid(), 0x4000000000012);
    }
}

TEST(BestCandidateFinder, matchVirtualRoutersByVlanInterfaces)
{
    const sai_object_id_t switchId = 0x21000000000000;
    const sai_object_id_t currentVrf1 = 0x3000000000001;
    const sai_object_id_t currentVrf2 = 0x3000000000002;
    const sai_object_id_t temporaryVrf1 = 0x3000000000101;
    const sai_object_id_t temporaryVrf2 = 0x3000000000102;
    const sai_object_id_t currentVlan1 = 0x26000000000001;
    const sai_object_id_t currentVlan2 = 0x26000000000002;
    const sai_object_id_t currentVlan3 = 0x26000000000003;
    const sai_object_id_t currentVlan4 = 0x26000000000004;
    const sai_object_id_t temporaryVlan1 = 0x26000000000101;
    const sai_object_id_t temporaryVlan2 = 0x26000000000102;
    const sai_object_id_t temporaryVlan3 = 0x26000000000103;
    const sai_object_id_t temporaryVlan4 = 0x26000000000104;
    const sai_object_id_t currentRif1 = 0x6000000000001;
    const sai_object_id_t currentRif2 = 0x6000000000002;
    const sai_object_id_t currentRif3 = 0x6000000000003;
    const sai_object_id_t currentRif4 = 0x6000000000004;
    const sai_object_id_t temporaryRif1 = 0x6000000000101;
    const sai_object_id_t temporaryRif2 = 0x6000000000102;
    const sai_object_id_t temporaryRif3 = 0x6000000000103;
    const sai_object_id_t temporaryRif4 = 0x6000000000104;

    auto key = [](const std::string& objectType, sai_object_id_t objectId)
    {
        return objectType + ":" + sai_serialize_object_id(objectId);
    };

    swss::TableDump current = {
        { key("SAI_OBJECT_TYPE_SWITCH", switchId), {} },
        { key("SAI_OBJECT_TYPE_VIRTUAL_ROUTER", currentVrf1), {} },
        { key("SAI_OBJECT_TYPE_VIRTUAL_ROUTER", currentVrf2), {} },
        { key("SAI_OBJECT_TYPE_VLAN", currentVlan1),
            { { "SAI_VLAN_ATTR_VLAN_ID", "1001" } } },
        { key("SAI_OBJECT_TYPE_VLAN", currentVlan2),
            { { "SAI_VLAN_ATTR_VLAN_ID", "1002" } } },
        { key("SAI_OBJECT_TYPE_VLAN", currentVlan3),
            { { "SAI_VLAN_ATTR_VLAN_ID", "2001" } } },
        { key("SAI_OBJECT_TYPE_VLAN", currentVlan4),
            { { "SAI_VLAN_ATTR_VLAN_ID", "2002" } } },
        { key("SAI_OBJECT_TYPE_ROUTER_INTERFACE", currentRif1), {
            { "SAI_ROUTER_INTERFACE_ATTR_TYPE", "SAI_ROUTER_INTERFACE_TYPE_VLAN" },
            { "SAI_ROUTER_INTERFACE_ATTR_VIRTUAL_ROUTER_ID", sai_serialize_object_id(currentVrf1) },
            { "SAI_ROUTER_INTERFACE_ATTR_VLAN_ID", sai_serialize_object_id(currentVlan1) } } },
        { key("SAI_OBJECT_TYPE_ROUTER_INTERFACE", currentRif2), {
            { "SAI_ROUTER_INTERFACE_ATTR_TYPE", "SAI_ROUTER_INTERFACE_TYPE_VLAN" },
            { "SAI_ROUTER_INTERFACE_ATTR_VIRTUAL_ROUTER_ID", sai_serialize_object_id(currentVrf1) },
            { "SAI_ROUTER_INTERFACE_ATTR_VLAN_ID", sai_serialize_object_id(currentVlan2) } } },
        { key("SAI_OBJECT_TYPE_ROUTER_INTERFACE", currentRif3), {
            { "SAI_ROUTER_INTERFACE_ATTR_TYPE", "SAI_ROUTER_INTERFACE_TYPE_VLAN" },
            { "SAI_ROUTER_INTERFACE_ATTR_VIRTUAL_ROUTER_ID", sai_serialize_object_id(currentVrf2) },
            { "SAI_ROUTER_INTERFACE_ATTR_VLAN_ID", sai_serialize_object_id(currentVlan3) } } },
        { key("SAI_OBJECT_TYPE_ROUTER_INTERFACE", currentRif4), {
            { "SAI_ROUTER_INTERFACE_ATTR_TYPE", "SAI_ROUTER_INTERFACE_TYPE_VLAN" },
            { "SAI_ROUTER_INTERFACE_ATTR_VIRTUAL_ROUTER_ID", sai_serialize_object_id(currentVrf2) },
            { "SAI_ROUTER_INTERFACE_ATTR_VLAN_ID", sai_serialize_object_id(currentVlan4) } } },
    };

    swss::TableDump temporary = {
        { key("SAI_OBJECT_TYPE_SWITCH", switchId), {} },
        { key("SAI_OBJECT_TYPE_VIRTUAL_ROUTER", temporaryVrf1), {} },
        { key("SAI_OBJECT_TYPE_VIRTUAL_ROUTER", temporaryVrf2), {} },
        { key("SAI_OBJECT_TYPE_VLAN", temporaryVlan1),
            { { "SAI_VLAN_ATTR_VLAN_ID", "1001" } } },
        { key("SAI_OBJECT_TYPE_VLAN", temporaryVlan2),
            { { "SAI_VLAN_ATTR_VLAN_ID", "1002" } } },
        { key("SAI_OBJECT_TYPE_VLAN", temporaryVlan3),
            { { "SAI_VLAN_ATTR_VLAN_ID", "2001" } } },
        { key("SAI_OBJECT_TYPE_VLAN", temporaryVlan4),
            { { "SAI_VLAN_ATTR_VLAN_ID", "2002" } } },
        { key("SAI_OBJECT_TYPE_ROUTER_INTERFACE", temporaryRif1), {
            { "SAI_ROUTER_INTERFACE_ATTR_TYPE", "SAI_ROUTER_INTERFACE_TYPE_VLAN" },
            { "SAI_ROUTER_INTERFACE_ATTR_VIRTUAL_ROUTER_ID", sai_serialize_object_id(temporaryVrf1) },
            { "SAI_ROUTER_INTERFACE_ATTR_VLAN_ID", sai_serialize_object_id(temporaryVlan4) } } },
        { key("SAI_OBJECT_TYPE_ROUTER_INTERFACE", temporaryRif2), {
            { "SAI_ROUTER_INTERFACE_ATTR_TYPE", "SAI_ROUTER_INTERFACE_TYPE_VLAN" },
            { "SAI_ROUTER_INTERFACE_ATTR_VIRTUAL_ROUTER_ID", sai_serialize_object_id(temporaryVrf1) },
            { "SAI_ROUTER_INTERFACE_ATTR_VLAN_ID", sai_serialize_object_id(temporaryVlan3) } } },
        { key("SAI_OBJECT_TYPE_ROUTER_INTERFACE", temporaryRif3), {
            { "SAI_ROUTER_INTERFACE_ATTR_TYPE", "SAI_ROUTER_INTERFACE_TYPE_VLAN" },
            { "SAI_ROUTER_INTERFACE_ATTR_VIRTUAL_ROUTER_ID", sai_serialize_object_id(temporaryVrf2) },
            { "SAI_ROUTER_INTERFACE_ATTR_VLAN_ID", sai_serialize_object_id(temporaryVlan2) } } },
        { key("SAI_OBJECT_TYPE_ROUTER_INTERFACE", temporaryRif4), {
            { "SAI_ROUTER_INTERFACE_ATTR_TYPE", "SAI_ROUTER_INTERFACE_TYPE_VLAN" },
            { "SAI_ROUTER_INTERFACE_ATTR_VIRTUAL_ROUTER_ID", sai_serialize_object_id(temporaryVrf2) },
            { "SAI_ROUTER_INTERFACE_ATTR_VLAN_ID", sai_serialize_object_id(temporaryVlan1) } } },
    };

    AsicView currentView(current);
    AsicView temporaryView(temporary);
    auto sw = std::make_shared<MockableSaiSwitchInterface>(0, 0);
    BestCandidateFinder finder(currentView, temporaryView, sw);

    auto match1 = finder.findCurrentBestMatch(temporaryView.m_oOids.at(temporaryVrf1));
    auto match2 = finder.findCurrentBestMatch(temporaryView.m_oOids.at(temporaryVrf2));

    ASSERT_NE(match1, nullptr);
    ASSERT_NE(match2, nullptr);
    EXPECT_EQ(match1->getVid(), currentVrf2);
    EXPECT_EQ(match2->getVid(), currentVrf1);
}
