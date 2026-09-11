#include "BestCandidateFinder.h"
#include "MockableSaiSwitchInterface.h"

#include "meta/sai_serialize.h"

#include "swss/logger.h"

#include <gtest/gtest.h>

#include <vector>

using namespace syncd;
using namespace unittests;

namespace
{
    struct TestOidPair
    {
        sai_object_id_t vid;
        sai_object_id_t rid;
    };

    TestOidPair deserializePair(
            _In_ const char *vidStr,
            _In_ const char *ridStr)
    {
        SWSS_LOG_ENTER();

        TestOidPair pair;

        sai_deserialize_object_id(vidStr, pair.vid);
        sai_deserialize_object_id(ridStr, pair.rid);

        return pair;
    }

    std::shared_ptr<SaiObj> addOidObject(
            _Inout_ AsicView &view,
            _In_ const TestOidPair &ids,
            _In_ const std::vector<std::pair<const char*, const char*>> &attrs)
    {
        SWSS_LOG_ENTER();

        auto obj = view.createDummyExistingObject(ids.rid, ids.vid);

        for (const auto &attr: attrs)
        {
            obj->setAttr(std::make_shared<SaiAttr>(attr.first, attr.second));
        }

        return obj;
    }
}

TEST(BestCandidateFinderIdentityIndex, buildAttrFilterFromObject_SkipsReadOnly)
{
    AsicView view;

    const auto ids = deserializePair("oid:0x4000000000912", "oid:0x400000000040");

    auto nh = addOidObject(view, ids, {
        {"SAI_NEXT_HOP_ATTR_TYPE", "SAI_NEXT_HOP_TYPE_IP"},
        {"SAI_NEXT_HOP_ATTR_IP", "10.0.0.9"},
    });

    const auto filter = BestCandidateFinder::buildAttrFilterFromObject(nh);

    EXPECT_EQ(filter.size(), 2u);
    EXPECT_NE(filter.find(SAI_NEXT_HOP_ATTR_TYPE), filter.end());
    EXPECT_NE(filter.find(SAI_NEXT_HOP_ATTR_IP), filter.end());
}

TEST(BestCandidateFinderIdentityIndex, findCurrentBestMatchUsingIdentityIndex)
{
    AsicView currentView;
    AsicView tempView;

    const auto curIds = deserializePair("oid:0x4000000000913", "oid:0x400000000050");
    const auto tmpIds = deserializePair("oid:0x4000000000914", "oid:0x400000000050");

    addOidObject(currentView, curIds, {
        {"SAI_NEXT_HOP_ATTR_TYPE", "SAI_NEXT_HOP_TYPE_IP"},
        {"SAI_NEXT_HOP_ATTR_IP", "10.0.0.10"},
    });

    auto tmpNh = addOidObject(tempView, tmpIds, {
        {"SAI_NEXT_HOP_ATTR_TYPE", "SAI_NEXT_HOP_TYPE_IP"},
        {"SAI_NEXT_HOP_ATTR_IP", "10.0.0.10"},
    });

    auto sw = std::make_shared<MockableSaiSwitchInterface>(0, 0);
    BestCandidateFinder bcf(currentView, tempView, sw);

    auto match = bcf.findCurrentBestMatchUsingIdentityIndex(tmpNh);

    ASSERT_NE(match, nullptr);
    EXPECT_EQ(match->getVid(), curIds.vid);
}

TEST(BestCandidateFinderIdentityIndex, findCurrentBestMatchUsingIdentityIndex_DuplicateAclTableGroupsFallThrough)
{
    AsicView currentView;
    AsicView tempView;

    const auto cur1 = deserializePair("oid:0xb000000000001", "oid:0xb000000000010");
    const auto cur2 = deserializePair("oid:0xb000000000002", "oid:0xb000000000011");
    const auto tmpIds = deserializePair("oid:0xb000000000003", "oid:0xb000000000012");

    const std::vector<std::pair<const char*, const char*>> atgAttrs = {
        {"SAI_ACL_TABLE_GROUP_ATTR_ACL_STAGE", "SAI_ACL_STAGE_INGRESS"},
        {"SAI_ACL_TABLE_GROUP_ATTR_ACL_BIND_POINT_TYPE_LIST", "1:SAI_ACL_BIND_POINT_TYPE_PORT"},
        {"SAI_ACL_TABLE_GROUP_ATTR_TYPE", "SAI_ACL_TABLE_GROUP_TYPE_PARALLEL"},
    };

    addOidObject(currentView, cur1, atgAttrs);
    addOidObject(currentView, cur2, atgAttrs);

    auto tmpAtg = addOidObject(tempView, tmpIds, atgAttrs);

    auto sw = std::make_shared<MockableSaiSwitchInterface>(0, 0);
    BestCandidateFinder bcf(currentView, tempView, sw);

    auto match = bcf.findCurrentBestMatchUsingIdentityIndex(tmpAtg);

    EXPECT_EQ(match, nullptr);
}

TEST(BestCandidateFinderIdentityIndex, findCurrentBestMatchForNextHopGroupUsingMemberIndex)
{
    AsicView currentView;
    AsicView tempView;

    const auto curNhg = deserializePair("oid:0x5000000002caf", "oid:0x500000000020");
    const auto tmpNhg = deserializePair("oid:0x5000000002cb0", "oid:0x500000000020");
    const auto curNh = deserializePair("oid:0x4000000000915", "oid:0x400000000060");
    const auto tmpNh = deserializePair("oid:0x4000000000916", "oid:0x400000000060");

    addOidObject(currentView, curNhg, {
        {"SAI_NEXT_HOP_GROUP_ATTR_TYPE", "SAI_NEXT_HOP_GROUP_TYPE_DYNAMIC_UNORDERED_ECMP"},
    });

    addOidObject(currentView, curNh, {
        {"SAI_NEXT_HOP_ATTR_TYPE", "SAI_NEXT_HOP_TYPE_IP"},
        {"SAI_NEXT_HOP_ATTR_IP", "10.0.0.11"},
    });

    addOidObject(currentView, deserializePair("oid:0x2d000000002cb0", "oid:0x2d000000000020"), {
        {"SAI_NEXT_HOP_GROUP_MEMBER_ATTR_NEXT_HOP_GROUP_ID", "oid:0x5000000002caf"},
        {"SAI_NEXT_HOP_GROUP_MEMBER_ATTR_NEXT_HOP_ID", "oid:0x4000000000915"},
    });

    auto tmpNhgObj = addOidObject(tempView, tmpNhg, {
        {"SAI_NEXT_HOP_GROUP_ATTR_TYPE", "SAI_NEXT_HOP_GROUP_TYPE_DYNAMIC_UNORDERED_ECMP"},
    });

    addOidObject(tempView, tmpNh, {
        {"SAI_NEXT_HOP_ATTR_TYPE", "SAI_NEXT_HOP_TYPE_IP"},
        {"SAI_NEXT_HOP_ATTR_IP", "10.0.0.11"},
    });

    addOidObject(tempView, deserializePair("oid:0x2d000000002cb1", "oid:0x2d000000000021"), {
        {"SAI_NEXT_HOP_GROUP_MEMBER_ATTR_NEXT_HOP_GROUP_ID", "oid:0x5000000002cb0"},
        {"SAI_NEXT_HOP_GROUP_MEMBER_ATTR_NEXT_HOP_ID", "oid:0x4000000000916"},
    });

    tempView.m_vidToRid[tmpNh.vid] = curNh.rid;

    auto sw = std::make_shared<MockableSaiSwitchInterface>(0, 0);
    BestCandidateFinder bcf(currentView, tempView, sw);

    auto match = bcf.findCurrentBestMatchForNextHopGroupUsingMemberIndex(tmpNhgObj);

    ASSERT_NE(match, nullptr);
    EXPECT_EQ(match->getVid(), curNhg.vid);
}

TEST(BestCandidateFinderIdentityIndex, findCurrentBestMatchForGenericObject_NhgmSameRidDifferentVid)
{
    AsicView currentView;
    AsicView tempView;

    const auto curNhg = deserializePair("oid:0x5000000002cb2", "oid:0x500000000030");
    const auto tmpNhg = deserializePair("oid:0x5000000002cb3", "oid:0x500000000030");
    const auto curNh = deserializePair("oid:0x4000000000917", "oid:0x400000000070");
    const auto tmpNh = deserializePair("oid:0x4000000000918", "oid:0x400000000070");
    const auto curNhgm = deserializePair("oid:0x2d000000002cb2", "oid:0x2d000000000030");
    const auto tmpNhgm = deserializePair("oid:0x2d000000002cb3", "oid:0x2d000000000031");

    addOidObject(currentView, curNhg, {
        {"SAI_NEXT_HOP_GROUP_ATTR_TYPE", "SAI_NEXT_HOP_GROUP_TYPE_DYNAMIC_UNORDERED_ECMP"},
    });

    addOidObject(currentView, curNh, {
        {"SAI_NEXT_HOP_ATTR_TYPE", "SAI_NEXT_HOP_TYPE_IP"},
        {"SAI_NEXT_HOP_ATTR_IP", "10.0.0.12"},
    });

    addOidObject(currentView, curNhgm, {
        {"SAI_NEXT_HOP_GROUP_MEMBER_ATTR_NEXT_HOP_GROUP_ID", "oid:0x5000000002cb2"},
        {"SAI_NEXT_HOP_GROUP_MEMBER_ATTR_NEXT_HOP_ID", "oid:0x4000000000917"},
    });

    auto tmpNhgmObj = addOidObject(tempView, tmpNhgm, {
        {"SAI_NEXT_HOP_GROUP_MEMBER_ATTR_NEXT_HOP_GROUP_ID", "oid:0x5000000002cb3"},
        {"SAI_NEXT_HOP_GROUP_MEMBER_ATTR_NEXT_HOP_ID", "oid:0x4000000000918"},
    });

    tempView.m_vidToRid[tmpNhg.vid] = curNhg.rid;
    tempView.m_vidToRid[tmpNh.vid] = curNh.rid;

    auto sw = std::make_shared<MockableSaiSwitchInterface>(0, 0);
    BestCandidateFinder bcf(currentView, tempView, sw);

    auto match = bcf.findCurrentBestMatchForGenericObject(tmpNhgmObj);

    ASSERT_NE(match, nullptr);
    EXPECT_EQ(match->getVid(), curNhgm.vid);
}

TEST(BestCandidateFinderIdentityIndex, findCurrentBestMatchForGenericObject_NhgmDifferentRidRejected)
{
    AsicView currentView;
    AsicView tempView;

    const auto curNhg = deserializePair("oid:0x5000000002cb4", "oid:0x500000000040");
    const auto tmpNhg = deserializePair("oid:0x5000000002cb5", "oid:0x500000000040");
    const auto curNh = deserializePair("oid:0x4000000000919", "oid:0x400000000080");
    const auto tmpNh = deserializePair("oid:0x400000000091a", "oid:0x400000000081");
    const auto curNhgm = deserializePair("oid:0x2d000000002cb4", "oid:0x2d000000000040");
    const auto tmpNhgm = deserializePair("oid:0x2d000000002cb5", "oid:0x2d000000000041");

    addOidObject(currentView, curNhg, {
        {"SAI_NEXT_HOP_GROUP_ATTR_TYPE", "SAI_NEXT_HOP_GROUP_TYPE_DYNAMIC_UNORDERED_ECMP"},
    });

    addOidObject(currentView, curNh, {
        {"SAI_NEXT_HOP_ATTR_TYPE", "SAI_NEXT_HOP_TYPE_IP"},
        {"SAI_NEXT_HOP_ATTR_IP", "10.0.0.13"},
    });

    addOidObject(currentView, curNhgm, {
        {"SAI_NEXT_HOP_GROUP_MEMBER_ATTR_NEXT_HOP_GROUP_ID", "oid:0x5000000002cb4"},
        {"SAI_NEXT_HOP_GROUP_MEMBER_ATTR_NEXT_HOP_ID", "oid:0x4000000000919"},
    });

    auto tmpNhgmObj = addOidObject(tempView, tmpNhgm, {
        {"SAI_NEXT_HOP_GROUP_MEMBER_ATTR_NEXT_HOP_GROUP_ID", "oid:0x5000000002cb5"},
        {"SAI_NEXT_HOP_GROUP_MEMBER_ATTR_NEXT_HOP_ID", "oid:0x400000000091a"},
    });

    tempView.m_vidToRid[tmpNhg.vid] = curNhg.rid;
    tempView.m_vidToRid[tmpNh.vid] = tmpNh.rid;

    auto sw = std::make_shared<MockableSaiSwitchInterface>(0, 0);
    BestCandidateFinder bcf(currentView, tempView, sw);

    auto match = bcf.findCurrentBestMatchForGenericObject(tmpNhgmObj);

    EXPECT_EQ(match, nullptr);
}

TEST(BestCandidateFinderIdentityIndex, findCurrentBestMatchUsingIdentityIndex_SkipsAlreadyProcessed)
{
    AsicView currentView;
    AsicView tempView;

    const auto curIds = deserializePair("oid:0x4000000000c01", "oid:0x4000000000e050");
    const auto tmpIds = deserializePair("oid:0x4000000000c02", "oid:0x4000000000e050");

    auto curNh = addOidObject(currentView, curIds, {
        {"SAI_NEXT_HOP_ATTR_TYPE", "SAI_NEXT_HOP_TYPE_IP"},
        {"SAI_NEXT_HOP_ATTR_IP", "10.0.0.80"},
    });

    auto tmpNh = addOidObject(tempView, tmpIds, {
        {"SAI_NEXT_HOP_ATTR_TYPE", "SAI_NEXT_HOP_TYPE_IP"},
        {"SAI_NEXT_HOP_ATTR_IP", "10.0.0.80"},
    });

    curNh->setObjectStatus(SAI_OBJECT_STATUS_MATCHED);

    auto sw = std::make_shared<MockableSaiSwitchInterface>(0, 0);
    BestCandidateFinder bcf(currentView, tempView, sw);

    EXPECT_EQ(bcf.findCurrentBestMatchUsingIdentityIndex(tmpNh), nullptr);
}

TEST(BestCandidateFinderIdentityIndex, findCurrentBestMatchForNextHopGroupUsingMemberIndex_TunnelNhFallbackMappedTunnel)
{
    AsicView currentView;
    AsicView tempView;

    const auto curTunnel = deserializePair("oid:0x2a0000000000ee", "oid:0x2a0000000000e0");
    const auto tmpTunnel = deserializePair("oid:0x2a0000000000ef", "oid:0x2a0000000000e0");
    const auto curNh = deserializePair("oid:0x4000000000c10", "oid:0x4000000000e060");
    const auto tmpNh = deserializePair("oid:0x4000000000c11", "oid:0x4000000000e061");
    const auto ipNh = deserializePair("oid:0x4000000000c12", "oid:0x4000000000e062");
    const auto otherTunnel = deserializePair("oid:0x2a0000000000f0", "oid:0x2a0000000000e1");
    const auto otherNh = deserializePair("oid:0x4000000000c13", "oid:0x4000000000e063");
    const auto curNhg = deserializePair("oid:0x5000000003c10", "oid:0x5000000000e020");
    const auto tmpNhg = deserializePair("oid:0x5000000003c11", "oid:0x5000000000e020");

    addOidObject(currentView, curTunnel, {
        {"SAI_TUNNEL_ATTR_TYPE", "SAI_TUNNEL_TYPE_VXLAN"},
        {"SAI_TUNNEL_ATTR_PEER_MODE", "SAI_TUNNEL_PEER_MODE_P2MP"},
        {"SAI_TUNNEL_ATTR_ENCAP_SRC_IP", "10.1.1.1"},
    });

    addOidObject(currentView, otherTunnel, {
        {"SAI_TUNNEL_ATTR_TYPE", "SAI_TUNNEL_TYPE_VXLAN"},
        {"SAI_TUNNEL_ATTR_PEER_MODE", "SAI_TUNNEL_PEER_MODE_P2MP"},
        {"SAI_TUNNEL_ATTR_ENCAP_SRC_IP", "10.1.1.2"},
    });

    addOidObject(currentView, curNh, {
        {"SAI_NEXT_HOP_ATTR_TYPE", "SAI_NEXT_HOP_TYPE_TUNNEL_ENCAP"},
        {"SAI_NEXT_HOP_ATTR_IP", "10.200.200.1"},
        {"SAI_NEXT_HOP_ATTR_TUNNEL_ID", "oid:0x2a0000000000ee"},
    });

    addOidObject(currentView, ipNh, {
        {"SAI_NEXT_HOP_ATTR_TYPE", "SAI_NEXT_HOP_TYPE_IP"},
        {"SAI_NEXT_HOP_ATTR_IP", "10.0.0.81"},
    });

    addOidObject(currentView, otherNh, {
        {"SAI_NEXT_HOP_ATTR_TYPE", "SAI_NEXT_HOP_TYPE_TUNNEL_ENCAP"},
        {"SAI_NEXT_HOP_ATTR_IP", "10.200.200.9"},
        {"SAI_NEXT_HOP_ATTR_TUNNEL_ID", "oid:0x2a0000000000f0"},
    });

    addOidObject(currentView, curNhg, {
        {"SAI_NEXT_HOP_GROUP_ATTR_TYPE", "SAI_NEXT_HOP_GROUP_TYPE_DYNAMIC_UNORDERED_ECMP"},
    });

    addOidObject(currentView, deserializePair("oid:0x2d000000003c10", "oid:0x2d0000000000e020"), {
        {"SAI_NEXT_HOP_GROUP_MEMBER_ATTR_NEXT_HOP_GROUP_ID", "oid:0x5000000003c10"},
        {"SAI_NEXT_HOP_GROUP_MEMBER_ATTR_NEXT_HOP_ID", "oid:0x4000000000c10"},
    });

    addOidObject(tempView, tmpTunnel, {
        {"SAI_TUNNEL_ATTR_TYPE", "SAI_TUNNEL_TYPE_VXLAN"},
        {"SAI_TUNNEL_ATTR_PEER_MODE", "SAI_TUNNEL_PEER_MODE_P2MP"},
        {"SAI_TUNNEL_ATTR_ENCAP_SRC_IP", "10.1.1.1"},
    });

    addOidObject(tempView, tmpNh, {
        {"SAI_NEXT_HOP_ATTR_TYPE", "SAI_NEXT_HOP_TYPE_TUNNEL_ENCAP"},
        {"SAI_NEXT_HOP_ATTR_IP", "10.200.200.8"},
        {"SAI_NEXT_HOP_ATTR_TUNNEL_ID", "oid:0x2a0000000000ef"},
    });

    auto tmpNhgObj = addOidObject(tempView, tmpNhg, {
        {"SAI_NEXT_HOP_GROUP_ATTR_TYPE", "SAI_NEXT_HOP_GROUP_TYPE_DYNAMIC_UNORDERED_ECMP"},
    });

    addOidObject(tempView, deserializePair("oid:0x2d000000003c11", "oid:0x2d0000000000e021"), {
        {"SAI_NEXT_HOP_GROUP_MEMBER_ATTR_NEXT_HOP_GROUP_ID", "oid:0x5000000003c11"},
        {"SAI_NEXT_HOP_GROUP_MEMBER_ATTR_NEXT_HOP_ID", "oid:0x4000000000c11"},
    });

    tempView.m_vidToRid.erase(tmpNh.vid);

    auto sw = std::make_shared<MockableSaiSwitchInterface>(0, 0);
    BestCandidateFinder bcf(currentView, tempView, sw);

    auto match = bcf.findCurrentBestMatchForNextHopGroupUsingMemberIndex(tmpNhgObj);

    ASSERT_NE(match, nullptr);
    EXPECT_EQ(match->getVid(), curNhg.vid);
}

TEST(BestCandidateFinderIdentityIndex, findCurrentBestMatchForNextHopGroupUsingMemberIndex_TunnelNhFallbackViaTunnelIdentity)
{
    AsicView currentView;
    AsicView tempView;

    const auto curTunnel = deserializePair("oid:0x2a0000000000f1", "oid:0x2a0000000000e2");
    const auto tmpTunnel = deserializePair("oid:0x2a0000000000f2", "oid:0x2a0000000000e2");
    const auto curNh = deserializePair("oid:0x4000000000c20", "oid:0x4000000000e070");
    const auto tmpNh = deserializePair("oid:0x4000000000c21", "oid:0x4000000000e071");
    const auto curNhg = deserializePair("oid:0x5000000003c20", "oid:0x5000000000e030");
    const auto tmpNhg = deserializePair("oid:0x5000000003c21", "oid:0x5000000000e030");

    addOidObject(currentView, curTunnel, {
        {"SAI_TUNNEL_ATTR_TYPE", "SAI_TUNNEL_TYPE_VXLAN"},
        {"SAI_TUNNEL_ATTR_PEER_MODE", "SAI_TUNNEL_PEER_MODE_P2MP"},
        {"SAI_TUNNEL_ATTR_ENCAP_SRC_IP", "10.2.2.2"},
    });

    addOidObject(currentView, curNh, {
        {"SAI_NEXT_HOP_ATTR_TYPE", "SAI_NEXT_HOP_TYPE_TUNNEL_ENCAP"},
        {"SAI_NEXT_HOP_ATTR_IP", "10.200.201.1"},
        {"SAI_NEXT_HOP_ATTR_TUNNEL_ID", "oid:0x2a0000000000f1"},
    });

    addOidObject(currentView, curNhg, {
        {"SAI_NEXT_HOP_GROUP_ATTR_TYPE", "SAI_NEXT_HOP_GROUP_TYPE_DYNAMIC_UNORDERED_ECMP"},
    });

    addOidObject(currentView, deserializePair("oid:0x2d000000003c20", "oid:0x2d0000000000e030"), {
        {"SAI_NEXT_HOP_GROUP_MEMBER_ATTR_NEXT_HOP_GROUP_ID", "oid:0x5000000003c20"},
        {"SAI_NEXT_HOP_GROUP_MEMBER_ATTR_NEXT_HOP_ID", "oid:0x4000000000c20"},
    });

    addOidObject(tempView, tmpTunnel, {
        {"SAI_TUNNEL_ATTR_TYPE", "SAI_TUNNEL_TYPE_VXLAN"},
        {"SAI_TUNNEL_ATTR_PEER_MODE", "SAI_TUNNEL_PEER_MODE_P2MP"},
        {"SAI_TUNNEL_ATTR_ENCAP_SRC_IP", "10.2.2.2"},
    });

    addOidObject(tempView, tmpNh, {
        {"SAI_NEXT_HOP_ATTR_TYPE", "SAI_NEXT_HOP_TYPE_TUNNEL_ENCAP"},
        {"SAI_NEXT_HOP_ATTR_IP", "10.200.201.8"},
        {"SAI_NEXT_HOP_ATTR_TUNNEL_ID", "oid:0x2a0000000000f2"},
    });

    auto tmpNhgObj = addOidObject(tempView, tmpNhg, {
        {"SAI_NEXT_HOP_GROUP_ATTR_TYPE", "SAI_NEXT_HOP_GROUP_TYPE_DYNAMIC_UNORDERED_ECMP"},
    });

    addOidObject(tempView, deserializePair("oid:0x2d000000003c21", "oid:0x2d0000000000e031"), {
        {"SAI_NEXT_HOP_GROUP_MEMBER_ATTR_NEXT_HOP_GROUP_ID", "oid:0x5000000003c21"},
        {"SAI_NEXT_HOP_GROUP_MEMBER_ATTR_NEXT_HOP_ID", "oid:0x4000000000c21"},
    });

    tempView.m_vidToRid.erase(tmpNh.vid);
    tempView.m_vidToRid.erase(tmpTunnel.vid);

    auto sw = std::make_shared<MockableSaiSwitchInterface>(0, 0);
    BestCandidateFinder bcf(currentView, tempView, sw);

    auto match = bcf.findCurrentBestMatchForNextHopGroupUsingMemberIndex(tmpNhgObj);

    ASSERT_NE(match, nullptr);
    EXPECT_EQ(match->getVid(), curNhg.vid);
}

TEST(BestCandidateFinderIdentityIndex, findCurrentBestMatchForNextHopGroupUsingMemberIndex_TunnelNhFallbackAmbiguous)
{
    AsicView currentView;
    AsicView tempView;

    const auto curTunnel = deserializePair("oid:0x2a0000000000f3", "oid:0x2a0000000000e3");
    const auto tmpTunnel = deserializePair("oid:0x2a0000000000f4", "oid:0x2a0000000000e3");
    const auto curNh1 = deserializePair("oid:0x4000000000c30", "oid:0x4000000000e080");
    const auto curNh2 = deserializePair("oid:0x4000000000c31", "oid:0x4000000000e081");
    const auto tmpNh = deserializePair("oid:0x4000000000c32", "oid:0x4000000000e082");
    const auto curNhg = deserializePair("oid:0x5000000003c30", "oid:0x5000000000e040");
    const auto tmpNhg = deserializePair("oid:0x5000000003c31", "oid:0x5000000000e040");

    addOidObject(currentView, curTunnel, {
        {"SAI_TUNNEL_ATTR_TYPE", "SAI_TUNNEL_TYPE_VXLAN"},
        {"SAI_TUNNEL_ATTR_PEER_MODE", "SAI_TUNNEL_PEER_MODE_P2MP"},
        {"SAI_TUNNEL_ATTR_ENCAP_SRC_IP", "10.3.3.3"},
    });

    addOidObject(currentView, curNh1, {
        {"SAI_NEXT_HOP_ATTR_TYPE", "SAI_NEXT_HOP_TYPE_TUNNEL_ENCAP"},
        {"SAI_NEXT_HOP_ATTR_IP", "10.200.202.1"},
        {"SAI_NEXT_HOP_ATTR_TUNNEL_ID", "oid:0x2a0000000000f3"},
    });

    addOidObject(currentView, curNh2, {
        {"SAI_NEXT_HOP_ATTR_TYPE", "SAI_NEXT_HOP_TYPE_TUNNEL_ENCAP"},
        {"SAI_NEXT_HOP_ATTR_IP", "10.200.202.2"},
        {"SAI_NEXT_HOP_ATTR_TUNNEL_ID", "oid:0x2a0000000000f3"},
    });

    addOidObject(currentView, curNhg, {
        {"SAI_NEXT_HOP_GROUP_ATTR_TYPE", "SAI_NEXT_HOP_GROUP_TYPE_DYNAMIC_UNORDERED_ECMP"},
    });

    addOidObject(currentView, deserializePair("oid:0x2d000000003c30", "oid:0x2d0000000000e040"), {
        {"SAI_NEXT_HOP_GROUP_MEMBER_ATTR_NEXT_HOP_GROUP_ID", "oid:0x5000000003c30"},
        {"SAI_NEXT_HOP_GROUP_MEMBER_ATTR_NEXT_HOP_ID", "oid:0x4000000000c30"},
    });

    addOidObject(tempView, tmpTunnel, {
        {"SAI_TUNNEL_ATTR_TYPE", "SAI_TUNNEL_TYPE_VXLAN"},
        {"SAI_TUNNEL_ATTR_PEER_MODE", "SAI_TUNNEL_PEER_MODE_P2MP"},
        {"SAI_TUNNEL_ATTR_ENCAP_SRC_IP", "10.3.3.3"},
    });

    addOidObject(tempView, tmpNh, {
        {"SAI_NEXT_HOP_ATTR_TYPE", "SAI_NEXT_HOP_TYPE_TUNNEL_ENCAP"},
        {"SAI_NEXT_HOP_ATTR_IP", "10.200.202.9"},
        {"SAI_NEXT_HOP_ATTR_TUNNEL_ID", "oid:0x2a0000000000f4"},
    });

    auto tmpNhgObj = addOidObject(tempView, tmpNhg, {
        {"SAI_NEXT_HOP_GROUP_ATTR_TYPE", "SAI_NEXT_HOP_GROUP_TYPE_DYNAMIC_UNORDERED_ECMP"},
    });

    addOidObject(tempView, deserializePair("oid:0x2d000000003c31", "oid:0x2d0000000000e041"), {
        {"SAI_NEXT_HOP_GROUP_MEMBER_ATTR_NEXT_HOP_GROUP_ID", "oid:0x5000000003c31"},
        {"SAI_NEXT_HOP_GROUP_MEMBER_ATTR_NEXT_HOP_ID", "oid:0x4000000000c32"},
    });

    tempView.m_vidToRid.erase(tmpNh.vid);

    auto sw = std::make_shared<MockableSaiSwitchInterface>(0, 0);
    BestCandidateFinder bcf(currentView, tempView, sw);

    EXPECT_EQ(bcf.findCurrentBestMatchForNextHopGroupUsingMemberIndex(tmpNhgObj), nullptr);
}

TEST(BestCandidateFinderIdentityIndex, findCurrentBestMatchForNextHopGroupUsingMemberIndex_MemberMissingNextHopId)
{
    AsicView currentView;
    AsicView tempView;

    const auto curNhg = deserializePair("oid:0x5000000003c40", "oid:0x5000000000e050");
    const auto tmpNhg = deserializePair("oid:0x5000000003c41", "oid:0x5000000000e050");

    addOidObject(currentView, curNhg, {
        {"SAI_NEXT_HOP_GROUP_ATTR_TYPE", "SAI_NEXT_HOP_GROUP_TYPE_DYNAMIC_UNORDERED_ECMP"},
    });

    auto tmpNhgObj = addOidObject(tempView, tmpNhg, {
        {"SAI_NEXT_HOP_GROUP_ATTR_TYPE", "SAI_NEXT_HOP_GROUP_TYPE_DYNAMIC_UNORDERED_ECMP"},
    });

    addOidObject(tempView, deserializePair("oid:0x2d000000003c40", "oid:0x2d0000000000e050"), {
        {"SAI_NEXT_HOP_GROUP_MEMBER_ATTR_NEXT_HOP_GROUP_ID", "oid:0x5000000003c41"},
    });

    auto sw = std::make_shared<MockableSaiSwitchInterface>(0, 0);
    BestCandidateFinder bcf(currentView, tempView, sw);

    EXPECT_EQ(bcf.findCurrentBestMatchForNextHopGroupUsingMemberIndex(tmpNhgObj), nullptr);
}

TEST(BestCandidateFinderIdentityIndex, findCurrentBestMatchForNextHopGroupUsingMemberIndex_EmptyMembers)
{
    AsicView currentView;
    AsicView tempView;

    const auto curNhg = deserializePair("oid:0x5000000003c50", "oid:0x5000000000e060");
    const auto tmpNhg = deserializePair("oid:0x5000000003c51", "oid:0x5000000000e060");

    addOidObject(currentView, curNhg, {
        {"SAI_NEXT_HOP_GROUP_ATTR_TYPE", "SAI_NEXT_HOP_GROUP_TYPE_DYNAMIC_UNORDERED_ECMP"},
    });

    auto tmpNhgObj = addOidObject(tempView, tmpNhg, {
        {"SAI_NEXT_HOP_GROUP_ATTR_TYPE", "SAI_NEXT_HOP_GROUP_TYPE_DYNAMIC_UNORDERED_ECMP"},
    });

    auto sw = std::make_shared<MockableSaiSwitchInterface>(0, 0);
    BestCandidateFinder bcf(currentView, tempView, sw);

    EXPECT_EQ(bcf.findCurrentBestMatchForNextHopGroupUsingMemberIndex(tmpNhgObj), nullptr);
}

TEST(BestCandidateFinderIdentityIndex, findCurrentBestMatchForNextHopGroupUsingMemberIndex_KeyMiss)
{
    AsicView currentView;
    AsicView tempView;

    const auto curNhg = deserializePair("oid:0x5000000003c60", "oid:0x5000000000e070");
    const auto tmpNhg = deserializePair("oid:0x5000000003c61", "oid:0x5000000000e070");
    const auto curNh = deserializePair("oid:0x4000000000c40", "oid:0x4000000000e090");
    const auto tmpNh = deserializePair("oid:0x4000000000c41", "oid:0x4000000000e091");

    addOidObject(currentView, curNhg, {
        {"SAI_NEXT_HOP_GROUP_ATTR_TYPE", "SAI_NEXT_HOP_GROUP_TYPE_DYNAMIC_UNORDERED_ECMP"},
    });

    addOidObject(currentView, curNh, {
        {"SAI_NEXT_HOP_ATTR_TYPE", "SAI_NEXT_HOP_TYPE_IP"},
        {"SAI_NEXT_HOP_ATTR_IP", "10.0.0.82"},
    });

    addOidObject(currentView, deserializePair("oid:0x2d000000003c60", "oid:0x2d0000000000e070"), {
        {"SAI_NEXT_HOP_GROUP_MEMBER_ATTR_NEXT_HOP_GROUP_ID", "oid:0x5000000003c60"},
        {"SAI_NEXT_HOP_GROUP_MEMBER_ATTR_NEXT_HOP_ID", "oid:0x4000000000c40"},
    });

    auto tmpNhgObj = addOidObject(tempView, tmpNhg, {
        {"SAI_NEXT_HOP_GROUP_ATTR_TYPE", "SAI_NEXT_HOP_GROUP_TYPE_DYNAMIC_UNORDERED_ECMP"},
    });

    addOidObject(tempView, tmpNh, {
        {"SAI_NEXT_HOP_ATTR_TYPE", "SAI_NEXT_HOP_TYPE_IP"},
        {"SAI_NEXT_HOP_ATTR_IP", "10.0.0.83"},
    });

    addOidObject(tempView, deserializePair("oid:0x2d000000003c61", "oid:0x2d0000000000e071"), {
        {"SAI_NEXT_HOP_GROUP_MEMBER_ATTR_NEXT_HOP_GROUP_ID", "oid:0x5000000003c61"},
        {"SAI_NEXT_HOP_GROUP_MEMBER_ATTR_NEXT_HOP_ID", "oid:0x4000000000c41"},
    });

    tempView.m_vidToRid[tmpNh.vid] = tmpNh.rid;

    auto sw = std::make_shared<MockableSaiSwitchInterface>(0, 0);
    BestCandidateFinder bcf(currentView, tempView, sw);

    EXPECT_EQ(bcf.findCurrentBestMatchForNextHopGroupUsingMemberIndex(tmpNhgObj), nullptr);
}

TEST(BestCandidateFinderIdentityIndex, findCurrentBestMatchForNextHopGroupUsingMemberIndex_PoolSizeGreaterThanOne)
{
    AsicView currentView;
    AsicView tempView;

    const auto curNhg1 = deserializePair("oid:0x5000000003c70", "oid:0x5000000000e080");
    const auto curNhg2 = deserializePair("oid:0x5000000003c71", "oid:0x5000000000e081");
    const auto tmpNhg = deserializePair("oid:0x5000000003c72", "oid:0x5000000000e080");
    const auto curNh = deserializePair("oid:0x4000000000c50", "oid:0x4000000000e0a0");
    const auto tmpNh = deserializePair("oid:0x4000000000c51", "oid:0x4000000000e0a0");

    addOidObject(currentView, curNhg1, {
        {"SAI_NEXT_HOP_GROUP_ATTR_TYPE", "SAI_NEXT_HOP_GROUP_TYPE_DYNAMIC_UNORDERED_ECMP"},
    });

    addOidObject(currentView, curNhg2, {
        {"SAI_NEXT_HOP_GROUP_ATTR_TYPE", "SAI_NEXT_HOP_GROUP_TYPE_DYNAMIC_UNORDERED_ECMP"},
    });

    addOidObject(currentView, curNh, {
        {"SAI_NEXT_HOP_ATTR_TYPE", "SAI_NEXT_HOP_TYPE_IP"},
        {"SAI_NEXT_HOP_ATTR_IP", "10.0.0.84"},
    });

    addOidObject(currentView, deserializePair("oid:0x2d000000003c70", "oid:0x2d0000000000e080"), {
        {"SAI_NEXT_HOP_GROUP_MEMBER_ATTR_NEXT_HOP_GROUP_ID", "oid:0x5000000003c70"},
        {"SAI_NEXT_HOP_GROUP_MEMBER_ATTR_NEXT_HOP_ID", "oid:0x4000000000c50"},
    });

    addOidObject(currentView, deserializePair("oid:0x2d000000003c71", "oid:0x2d0000000000e081"), {
        {"SAI_NEXT_HOP_GROUP_MEMBER_ATTR_NEXT_HOP_GROUP_ID", "oid:0x5000000003c71"},
        {"SAI_NEXT_HOP_GROUP_MEMBER_ATTR_NEXT_HOP_ID", "oid:0x4000000000c50"},
    });

    auto tmpNhgObj = addOidObject(tempView, tmpNhg, {
        {"SAI_NEXT_HOP_GROUP_ATTR_TYPE", "SAI_NEXT_HOP_GROUP_TYPE_DYNAMIC_UNORDERED_ECMP"},
    });

    addOidObject(tempView, tmpNh, {
        {"SAI_NEXT_HOP_ATTR_TYPE", "SAI_NEXT_HOP_TYPE_IP"},
        {"SAI_NEXT_HOP_ATTR_IP", "10.0.0.84"},
    });

    addOidObject(tempView, deserializePair("oid:0x2d000000003c72", "oid:0x2d0000000000e082"), {
        {"SAI_NEXT_HOP_GROUP_MEMBER_ATTR_NEXT_HOP_GROUP_ID", "oid:0x5000000003c72"},
        {"SAI_NEXT_HOP_GROUP_MEMBER_ATTR_NEXT_HOP_ID", "oid:0x4000000000c51"},
    });

    tempView.m_vidToRid[tmpNh.vid] = curNh.rid;

    auto sw = std::make_shared<MockableSaiSwitchInterface>(0, 0);
    BestCandidateFinder bcf(currentView, tempView, sw);

    auto match = bcf.findCurrentBestMatchForNextHopGroupUsingMemberIndex(tmpNhgObj);

    ASSERT_NE(match, nullptr);
    EXPECT_TRUE(match->getVid() == curNhg1.vid || match->getVid() == curNhg2.vid);
}

TEST(BestCandidateFinderIdentityIndex, findCurrentBestMatchForNextHopGroupUsingMemberIndex_SkipsAlreadyProcessed)
{
    AsicView currentView;
    AsicView tempView;

    const auto curNhg = deserializePair("oid:0x5000000003c80", "oid:0x5000000000e090");
    const auto tmpNhg = deserializePair("oid:0x5000000003c81", "oid:0x5000000000e090");
    const auto curNh = deserializePair("oid:0x4000000000c60", "oid:0x4000000000e0b0");
    const auto tmpNh = deserializePair("oid:0x4000000000c61", "oid:0x4000000000e0b0");

    auto curNhgObj = addOidObject(currentView, curNhg, {
        {"SAI_NEXT_HOP_GROUP_ATTR_TYPE", "SAI_NEXT_HOP_GROUP_TYPE_DYNAMIC_UNORDERED_ECMP"},
    });

    addOidObject(currentView, curNh, {
        {"SAI_NEXT_HOP_ATTR_TYPE", "SAI_NEXT_HOP_TYPE_IP"},
        {"SAI_NEXT_HOP_ATTR_IP", "10.0.0.85"},
    });

    addOidObject(currentView, deserializePair("oid:0x2d000000003c80", "oid:0x2d0000000000e090"), {
        {"SAI_NEXT_HOP_GROUP_MEMBER_ATTR_NEXT_HOP_GROUP_ID", "oid:0x5000000003c80"},
        {"SAI_NEXT_HOP_GROUP_MEMBER_ATTR_NEXT_HOP_ID", "oid:0x4000000000c60"},
    });

    auto tmpNhgObj = addOidObject(tempView, tmpNhg, {
        {"SAI_NEXT_HOP_GROUP_ATTR_TYPE", "SAI_NEXT_HOP_GROUP_TYPE_DYNAMIC_UNORDERED_ECMP"},
    });

    addOidObject(tempView, tmpNh, {
        {"SAI_NEXT_HOP_ATTR_TYPE", "SAI_NEXT_HOP_TYPE_IP"},
        {"SAI_NEXT_HOP_ATTR_IP", "10.0.0.85"},
    });

    addOidObject(tempView, deserializePair("oid:0x2d000000003c81", "oid:0x2d0000000000e091"), {
        {"SAI_NEXT_HOP_GROUP_MEMBER_ATTR_NEXT_HOP_GROUP_ID", "oid:0x5000000003c81"},
        {"SAI_NEXT_HOP_GROUP_MEMBER_ATTR_NEXT_HOP_ID", "oid:0x4000000000c61"},
    });

    tempView.m_vidToRid[tmpNh.vid] = curNh.rid;

    currentView.getNhgMemberIndex();
    curNhgObj->setObjectStatus(SAI_OBJECT_STATUS_MATCHED);

    auto sw = std::make_shared<MockableSaiSwitchInterface>(0, 0);
    BestCandidateFinder bcf(currentView, tempView, sw);

    EXPECT_EQ(bcf.findCurrentBestMatchForNextHopGroupUsingMemberIndex(tmpNhgObj), nullptr);
}

TEST(BestCandidateFinderIdentityIndex, findCurrentBestMatchForGenericObject_EmptyNhgFallsThroughToSlowPath)
{
    AsicView currentView;
    AsicView tempView;

    const auto curNhg = deserializePair("oid:0x5000000003c90", "oid:0x5000000000e0a0");
    const auto tmpNhg = deserializePair("oid:0x5000000003c91", "oid:0x5000000000e0a0");

    addOidObject(currentView, curNhg, {
        {"SAI_NEXT_HOP_GROUP_ATTR_TYPE", "SAI_NEXT_HOP_GROUP_TYPE_DYNAMIC_UNORDERED_ECMP"},
    });

    auto tmpNhgObj = addOidObject(tempView, tmpNhg, {
        {"SAI_NEXT_HOP_GROUP_ATTR_TYPE", "SAI_NEXT_HOP_GROUP_TYPE_DYNAMIC_UNORDERED_ECMP"},
    });

    auto sw = std::make_shared<MockableSaiSwitchInterface>(0, 0);
    BestCandidateFinder bcf(currentView, tempView, sw);

    auto match = bcf.findCurrentBestMatchForGenericObject(tmpNhgObj);

    ASSERT_NE(match, nullptr);
    EXPECT_EQ(match->getVid(), curNhg.vid);
}

TEST(BestCandidateFinderIdentityIndex, findCurrentBestMatchForGenericObject_NhgmRidEqualWhenGroupVidUnmapped)
{
    AsicView currentView;
    AsicView tempView;

    const auto curNhg = deserializePair("oid:0x5000000003ca0", "oid:0x5000000000e0b0");
    const auto curNh = deserializePair("oid:0x4000000000c70", "oid:0x4000000000e0c0");
    const auto tmpNh = deserializePair("oid:0x4000000000c71", "oid:0x4000000000e0c0");
    const auto curNhgm = deserializePair("oid:0x2d000000003ca0", "oid:0x2d0000000000e0b0");
    const auto tmpNhgm = deserializePair("oid:0x2d000000003ca1", "oid:0x2d0000000000e0b1");

    addOidObject(currentView, curNhg, {
        {"SAI_NEXT_HOP_GROUP_ATTR_TYPE", "SAI_NEXT_HOP_GROUP_TYPE_DYNAMIC_UNORDERED_ECMP"},
    });

    addOidObject(currentView, curNh, {
        {"SAI_NEXT_HOP_ATTR_TYPE", "SAI_NEXT_HOP_TYPE_IP"},
        {"SAI_NEXT_HOP_ATTR_IP", "10.0.0.86"},
    });

    addOidObject(currentView, curNhgm, {
        {"SAI_NEXT_HOP_GROUP_MEMBER_ATTR_NEXT_HOP_GROUP_ID", "oid:0x5000000003ca0"},
        {"SAI_NEXT_HOP_GROUP_MEMBER_ATTR_NEXT_HOP_ID", "oid:0x4000000000c70"},
    });

    auto tmpNhgmObj = addOidObject(tempView, tmpNhgm, {
        {"SAI_NEXT_HOP_GROUP_MEMBER_ATTR_NEXT_HOP_GROUP_ID", "oid:0x5000000003ca0"},
        {"SAI_NEXT_HOP_GROUP_MEMBER_ATTR_NEXT_HOP_ID", "oid:0x4000000000c71"},
    });

    tempView.m_vidToRid[tmpNh.vid] = curNh.rid;

    auto sw = std::make_shared<MockableSaiSwitchInterface>(0, 0);
    BestCandidateFinder bcf(currentView, tempView, sw);

    auto match = bcf.findCurrentBestMatchForGenericObject(tmpNhgmObj);

    ASSERT_NE(match, nullptr);
    EXPECT_EQ(match->getVid(), curNhgm.vid);
}
