#include "BestCandidateFinder.h"
#include "MockableSaiSwitchInterface.h"

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
