#include "AsicView.h"

#include "meta/sai_serialize.h"

#include "swss/logger.h"

#include <gtest/gtest.h>

#include <set>
#include <string>
#include <vector>

using namespace syncd;

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

    std::set<sai_attr_id_t> attrFilterFromObject(
            _In_ const std::shared_ptr<SaiObj> &obj)
    {
        SWSS_LOG_ENTER();

        std::set<sai_attr_id_t> filter;

        for (const auto &kvp: obj->getAllAttributes())
        {
            if (AsicView::isAttrIncludedInIdentityKey(kvp.second->getAttrMetadata()))
            {
                filter.insert(kvp.first);
            }
        }

        return filter;
    }
}

TEST(AsicViewIdentityIndex, computeCreateOnlyKey_ScalarProjectionMatchesAcrossViews)
{
    AsicView currentView;
    AsicView tempView;

    const auto curIds = deserializePair("oid:0x4000000000905", "oid:0x400000000001");
    const auto tmpIds = deserializePair("oid:0x4000000000906", "oid:0x400000000001");

    const auto rifCur = deserializePair("oid:0x60000000008f8", "oid:0x600000000001");
    const auto rifTmp = deserializePair("oid:0x60000000008f9", "oid:0x600000000001");

    addOidObject(currentView, rifCur, {});
    addOidObject(tempView, rifTmp, {});

    const std::vector<std::pair<const char*, const char*>> sparseAttrs = {
        {"SAI_NEXT_HOP_ATTR_TYPE", "SAI_NEXT_HOP_TYPE_IP"},
        {"SAI_NEXT_HOP_ATTR_IP", "10.0.0.7"},
        {"SAI_NEXT_HOP_ATTR_ROUTER_INTERFACE_ID", "oid:0x60000000008f9"},
    };

    const std::vector<std::pair<const char*, const char*>> richAttrs = {
        {"SAI_NEXT_HOP_ATTR_TYPE", "SAI_NEXT_HOP_TYPE_IP"},
        {"SAI_NEXT_HOP_ATTR_IP", "10.0.0.7"},
        {"SAI_NEXT_HOP_ATTR_ROUTER_INTERFACE_ID", "oid:0x60000000008f8"},
        {"SAI_NEXT_HOP_ATTR_TUNNEL_VNI", "10100"},
    };

    auto curNh = addOidObject(currentView, curIds, richAttrs);
    auto tmpNh = addOidObject(tempView, tmpIds, sparseAttrs);

    tempView.m_vidToRid[rifTmp.vid] = rifCur.rid;

    const std::set<sai_attr_id_t> filter = attrFilterFromObject(tmpNh);

    const std::string curKey = AsicView::computeCreateOnlyKey(curNh, currentView.m_vidToRid, &filter);
    const std::string tmpKey = AsicView::computeCreateOnlyKey(tmpNh, tempView.m_vidToRid, &filter);

    EXPECT_FALSE(curKey.empty());
    EXPECT_EQ(curKey, tmpKey);
}

TEST(AsicViewIdentityIndex, computeCreateOnlyKey_ReturnsEmptyWhenOidUnresolved)
{
    AsicView tempView;

    const auto nhIds = deserializePair("oid:0x4000000000907", "oid:0x400000000002");

    auto nh = addOidObject(tempView, nhIds, {
        {"SAI_NEXT_HOP_ATTR_TYPE", "SAI_NEXT_HOP_TYPE_TUNNEL_ENCAP"},
        {"SAI_NEXT_HOP_ATTR_IP", "10.200.200.1"},
        {"SAI_NEXT_HOP_ATTR_TUNNEL_ID", "oid:0x1a000000000099"},
    });

    const std::set<sai_attr_id_t> filter = attrFilterFromObject(nh);
    const std::string key = AsicView::computeCreateOnlyKey(nh, tempView.m_vidToRid, &filter);

    EXPECT_TRUE(key.empty());
}

TEST(AsicViewIdentityIndex, getCreateOnlyIndex_UniqueKeyLookup)
{
    AsicView currentView;

    const auto nh1 = deserializePair("oid:0x4000000000908", "oid:0x400000000010");
    const auto nh2 = deserializePair("oid:0x4000000000909", "oid:0x400000000011");

    auto obj1 = addOidObject(currentView, nh1, {
        {"SAI_NEXT_HOP_ATTR_TYPE", "SAI_NEXT_HOP_TYPE_IP"},
        {"SAI_NEXT_HOP_ATTR_IP", "10.0.0.1"},
    });

    addOidObject(currentView, nh2, {
        {"SAI_NEXT_HOP_ATTR_TYPE", "SAI_NEXT_HOP_TYPE_IP"},
        {"SAI_NEXT_HOP_ATTR_IP", "10.0.0.2"},
    });

    const std::set<sai_attr_id_t> filter = attrFilterFromObject(obj1);
    const auto *index = currentView.getCreateOnlyIndex(SAI_OBJECT_TYPE_NEXT_HOP, filter);

    ASSERT_NE(index, nullptr);
    EXPECT_EQ(index->size(), 2u);

    const std::string key1 = AsicView::computeCreateOnlyKey(obj1, currentView.m_vidToRid, &filter);
    const auto it = index->find(key1);

    ASSERT_NE(it, index->end());
    EXPECT_EQ(it->second->getVid(), nh1.vid);
}

TEST(AsicViewIdentityIndex, getCreateOnlyIndex_ExcludesDuplicateKeys)
{
    AsicView currentView;

    const auto nh1 = deserializePair("oid:0x400000000090a", "oid:0x400000000012");
    const auto nh2 = deserializePair("oid:0x400000000090b", "oid:0x400000000013");

    auto obj1 = addOidObject(currentView, nh1, {
        {"SAI_NEXT_HOP_ATTR_TYPE", "SAI_NEXT_HOP_TYPE_IP"},
        {"SAI_NEXT_HOP_ATTR_IP", "10.0.0.3"},
    });

    addOidObject(currentView, nh2, {
        {"SAI_NEXT_HOP_ATTR_TYPE", "SAI_NEXT_HOP_TYPE_IP"},
        {"SAI_NEXT_HOP_ATTR_IP", "10.0.0.3"},
    });

    const std::set<sai_attr_id_t> filter = attrFilterFromObject(obj1);
    const auto *index = currentView.getCreateOnlyIndex(SAI_OBJECT_TYPE_NEXT_HOP, filter);

    ASSERT_NE(index, nullptr);
    EXPECT_EQ(index->size(), 0u);

    const std::string key1 = AsicView::computeCreateOnlyKey(obj1, currentView.m_vidToRid, &filter);

    EXPECT_EQ(index->find(key1), index->end());
}

TEST(AsicViewIdentityIndex, isAttrIncludedInIdentityKey_SkipsCreateAndSet)
{
    AsicView currentView;

    const auto ids = deserializePair("oid:0x400000000090c", "oid:0x400000000014");

    auto nh = addOidObject(currentView, ids, {
        {"SAI_NEXT_HOP_ATTR_TYPE", "SAI_NEXT_HOP_TYPE_IP"},
        {"SAI_NEXT_HOP_ATTR_IP", "10.0.0.4"},
        {"SAI_NEXT_HOP_ATTR_TUNNEL_VNI", "10100"},
    });

    const std::set<sai_attr_id_t> filter = attrFilterFromObject(nh);

    EXPECT_EQ(filter.size(), 2u);
    EXPECT_NE(filter.find(SAI_NEXT_HOP_ATTR_TYPE), filter.end());
    EXPECT_NE(filter.find(SAI_NEXT_HOP_ATTR_IP), filter.end());
    EXPECT_EQ(filter.find(SAI_NEXT_HOP_ATTR_TUNNEL_VNI), filter.end());
}

TEST(AsicViewIdentityIndex, isAttrIncludedInIdentityKey_IncludesNhgmNextHopId)
{
    AsicView currentView;

    const auto nhg = deserializePair("oid:0x5000000002ca0", "oid:0x500000000010");
    const auto nh = deserializePair("oid:0x400000000090d", "oid:0x400000000015");
    const auto nhgm = deserializePair("oid:0x2d000000002ca0", "oid:0x2d000000000010");

    addOidObject(currentView, nhg, {
        {"SAI_NEXT_HOP_GROUP_ATTR_TYPE", "SAI_NEXT_HOP_GROUP_TYPE_DYNAMIC_UNORDERED_ECMP"},
    });

    addOidObject(currentView, nh, {
        {"SAI_NEXT_HOP_ATTR_TYPE", "SAI_NEXT_HOP_TYPE_IP"},
        {"SAI_NEXT_HOP_ATTR_IP", "10.0.0.5"},
    });

    auto member = addOidObject(currentView, nhgm, {
        {"SAI_NEXT_HOP_GROUP_MEMBER_ATTR_NEXT_HOP_GROUP_ID", "oid:0x5000000002ca0"},
        {"SAI_NEXT_HOP_GROUP_MEMBER_ATTR_NEXT_HOP_ID", "oid:0x400000000090d"},
        {"SAI_NEXT_HOP_GROUP_MEMBER_ATTR_WEIGHT", "2"},
    });

    const std::set<sai_attr_id_t> filter = attrFilterFromObject(member);

    EXPECT_EQ(filter.size(), 2u);
    EXPECT_NE(filter.find(SAI_NEXT_HOP_GROUP_MEMBER_ATTR_NEXT_HOP_GROUP_ID), filter.end());
    EXPECT_NE(filter.find(SAI_NEXT_HOP_GROUP_MEMBER_ATTR_NEXT_HOP_ID), filter.end());
    EXPECT_EQ(filter.find(SAI_NEXT_HOP_GROUP_MEMBER_ATTR_WEIGHT), filter.end());
}

TEST(AsicViewIdentityIndex, computeNhgMemberKey_SortedMemberRids)
{
    AsicView view;

    const auto nhg = deserializePair("oid:0x5000000002ca2", "oid:0x500000000001");
    const auto nh1 = deserializePair("oid:0x4000000000910", "oid:0x400000000020");
    const auto nh2 = deserializePair("oid:0x4000000000911", "oid:0x400000000021");

    addOidObject(view, nhg, {
        {"SAI_NEXT_HOP_GROUP_ATTR_TYPE", "SAI_NEXT_HOP_GROUP_TYPE_DYNAMIC_UNORDERED_ECMP"},
    });

    addOidObject(view, nh1, {});
    addOidObject(view, nh2, {});

    addOidObject(view, deserializePair("oid:0x2d000000002ca6", "oid:0x2d000000000001"), {
        {"SAI_NEXT_HOP_GROUP_MEMBER_ATTR_NEXT_HOP_GROUP_ID", "oid:0x5000000002ca2"},
        {"SAI_NEXT_HOP_GROUP_MEMBER_ATTR_NEXT_HOP_ID", "oid:0x4000000000911"},
    });

    addOidObject(view, deserializePair("oid:0x2d000000002ca7", "oid:0x2d000000000002"), {
        {"SAI_NEXT_HOP_GROUP_MEMBER_ATTR_NEXT_HOP_GROUP_ID", "oid:0x5000000002ca2"},
        {"SAI_NEXT_HOP_GROUP_MEMBER_ATTR_NEXT_HOP_ID", "oid:0x4000000000910"},
    });

    const std::string key = view.computeNhgMemberKey(nhg.vid, view.m_vidToRid);

    EXPECT_EQ(key,
            sai_serialize_object_id(nh1.rid) + ";" + sai_serialize_object_id(nh2.rid));
}

TEST(AsicViewIdentityIndex, getNhgMemberIndex_GroupsInterchangeableNhgs)
{
    AsicView view;

    const auto nhg1 = deserializePair("oid:0x5000000002af9", "oid:0x500000000010");
    const auto nhg2 = deserializePair("oid:0x5000000002afe", "oid:0x500000000011");
    const auto nh = deserializePair("oid:0x4000000002af8", "oid:0x400000000030");

    addOidObject(view, nhg1, {
        {"SAI_NEXT_HOP_GROUP_ATTR_TYPE", "SAI_NEXT_HOP_GROUP_TYPE_DYNAMIC_UNORDERED_ECMP"},
    });

    addOidObject(view, nhg2, {
        {"SAI_NEXT_HOP_GROUP_ATTR_TYPE", "SAI_NEXT_HOP_GROUP_TYPE_DYNAMIC_UNORDERED_ECMP"},
    });

    addOidObject(view, nh, {});

    addOidObject(view, deserializePair("oid:0x2d000000002afb", "oid:0x2d000000000010"), {
        {"SAI_NEXT_HOP_GROUP_MEMBER_ATTR_NEXT_HOP_GROUP_ID", "oid:0x5000000002af9"},
        {"SAI_NEXT_HOP_GROUP_MEMBER_ATTR_NEXT_HOP_ID", "oid:0x4000000002af8"},
    });

    addOidObject(view, deserializePair("oid:0x2d000000002aff", "oid:0x2d000000000011"), {
        {"SAI_NEXT_HOP_GROUP_MEMBER_ATTR_NEXT_HOP_GROUP_ID", "oid:0x5000000002afe"},
        {"SAI_NEXT_HOP_GROUP_MEMBER_ATTR_NEXT_HOP_ID", "oid:0x4000000002af8"},
    });

    const auto *index = view.getNhgMemberIndex();

    ASSERT_NE(index, nullptr);
    ASSERT_EQ(index->size(), 1u);

    const auto &pool = index->begin()->second;

    EXPECT_EQ(pool.size(), 2u);
}

TEST(AsicViewIdentityIndex, computeCreateOnlyKey_SkipsCreateAndSetInFilter)
{
    AsicView currentView;

    const auto nhIds = deserializePair("oid:0x4000000000a01", "oid:0x4000000000f001");

    auto nh = addOidObject(currentView, nhIds, {
        {"SAI_NEXT_HOP_ATTR_TYPE", "SAI_NEXT_HOP_TYPE_IP"},
        {"SAI_NEXT_HOP_ATTR_IP", "10.0.0.90"},
        {"SAI_NEXT_HOP_ATTR_TUNNEL_VNI", "10100"},
    });

    std::set<sai_attr_id_t> filter = {
        SAI_NEXT_HOP_ATTR_TYPE,
        SAI_NEXT_HOP_ATTR_IP,
        SAI_NEXT_HOP_ATTR_TUNNEL_VNI,
    };

    const std::string key = AsicView::computeCreateOnlyKey(nh, currentView.m_vidToRid, &filter);

    EXPECT_NE(key.find("SAI_NEXT_HOP_ATTR_TYPE"), std::string::npos);
    EXPECT_NE(key.find("SAI_NEXT_HOP_ATTR_IP"), std::string::npos);
    EXPECT_EQ(key.find("SAI_NEXT_HOP_ATTR_TUNNEL_VNI"), std::string::npos);
}

TEST(AsicViewIdentityIndex, computeCreateOnlyKey_SerializesNullOid)
{
    AsicView currentView;

    const auto nhg = deserializePair("oid:0x5000000003a01", "oid:0x5000000000f010");
    const auto nhgm = deserializePair("oid:0x2d000000003a01", "oid:0x2d0000000000f010");

    addOidObject(currentView, nhg, {
        {"SAI_NEXT_HOP_GROUP_ATTR_TYPE", "SAI_NEXT_HOP_GROUP_TYPE_DYNAMIC_UNORDERED_ECMP"},
    });

    auto member = addOidObject(currentView, nhgm, {
        {"SAI_NEXT_HOP_GROUP_MEMBER_ATTR_NEXT_HOP_GROUP_ID", "oid:0x5000000003a01"},
        {"SAI_NEXT_HOP_GROUP_MEMBER_ATTR_NEXT_HOP_ID", "oid:0x0"},
    });

    const std::set<sai_attr_id_t> filter = attrFilterFromObject(member);
    const std::string key = AsicView::computeCreateOnlyKey(member, currentView.m_vidToRid, &filter);

    EXPECT_FALSE(key.empty());
    EXPECT_NE(key.find(sai_serialize_object_id(SAI_NULL_OBJECT_ID)), std::string::npos);
}

TEST(AsicViewIdentityIndex, computeNhgMemberKey_MissingNextHopId)
{
    AsicView view;

    const auto nhg = deserializePair("oid:0x5000000003a10", "oid:0x5000000000f020");

    addOidObject(view, nhg, {
        {"SAI_NEXT_HOP_GROUP_ATTR_TYPE", "SAI_NEXT_HOP_GROUP_TYPE_DYNAMIC_UNORDERED_ECMP"},
    });

    addOidObject(view, deserializePair("oid:0x2d000000003a10", "oid:0x2d0000000000f020"), {
        {"SAI_NEXT_HOP_GROUP_MEMBER_ATTR_NEXT_HOP_GROUP_ID", "oid:0x5000000003a10"},
    });

    EXPECT_TRUE(view.computeNhgMemberKey(nhg.vid, view.m_vidToRid).empty());
}

TEST(AsicViewIdentityIndex, computeNhgMemberKey_UnresolvedNextHopVid)
{
    AsicView view;

    const auto nhg = deserializePair("oid:0x5000000003a20", "oid:0x5000000000f030");

    addOidObject(view, nhg, {
        {"SAI_NEXT_HOP_GROUP_ATTR_TYPE", "SAI_NEXT_HOP_GROUP_TYPE_DYNAMIC_UNORDERED_ECMP"},
    });

    addOidObject(view, deserializePair("oid:0x2d000000003a20", "oid:0x2d0000000000f030"), {
        {"SAI_NEXT_HOP_GROUP_MEMBER_ATTR_NEXT_HOP_GROUP_ID", "oid:0x5000000003a20"},
        {"SAI_NEXT_HOP_GROUP_MEMBER_ATTR_NEXT_HOP_ID", "oid:0x4000000000a20"},
    });

    EXPECT_TRUE(view.computeNhgMemberKey(nhg.vid, view.m_vidToRid).empty());
}

TEST(AsicViewIdentityIndex, computeNhgMemberKey_EmptyMembers)
{
    AsicView view;

    const auto nhg = deserializePair("oid:0x5000000003a30", "oid:0x5000000000f040");

    addOidObject(view, nhg, {
        {"SAI_NEXT_HOP_GROUP_ATTR_TYPE", "SAI_NEXT_HOP_GROUP_TYPE_DYNAMIC_UNORDERED_ECMP"},
    });

    EXPECT_TRUE(view.computeNhgMemberKey(nhg.vid, view.m_vidToRid).empty());
}

TEST(AsicViewIdentityIndex, getCreateOnlyIndex_SkipsEmptyKey)
{
    AsicView currentView;

    const auto tunnelNh = deserializePair("oid:0x4000000000a30", "oid:0x4000000000f050");
    const auto ipNh = deserializePair("oid:0x4000000000a31", "oid:0x4000000000f051");

    auto tunnelObj = addOidObject(currentView, tunnelNh, {
        {"SAI_NEXT_HOP_ATTR_TYPE", "SAI_NEXT_HOP_TYPE_TUNNEL_ENCAP"},
        {"SAI_NEXT_HOP_ATTR_IP", "10.200.210.1"},
        {"SAI_NEXT_HOP_ATTR_TUNNEL_ID", "oid:0x1a0000000000ef"},
    });

    addOidObject(currentView, ipNh, {
        {"SAI_NEXT_HOP_ATTR_TYPE", "SAI_NEXT_HOP_TYPE_IP"},
        {"SAI_NEXT_HOP_ATTR_IP", "10.0.0.91"},
    });

    const std::set<sai_attr_id_t> filter = attrFilterFromObject(tunnelObj);
    const auto *index = currentView.getCreateOnlyIndex(SAI_OBJECT_TYPE_NEXT_HOP, filter);

    ASSERT_NE(index, nullptr);
    EXPECT_EQ(index->size(), 1u);
}

TEST(AsicViewIdentityIndex, getNhgMemberIndex_SkipsEmptyKey)
{
    AsicView view;

    const auto emptyNhg = deserializePair("oid:0x5000000003a40", "oid:0x5000000000f060");
    const auto validNhg = deserializePair("oid:0x5000000003a41", "oid:0x5000000000f061");
    const auto nh = deserializePair("oid:0x4000000000a40", "oid:0x4000000000f070");

    addOidObject(view, emptyNhg, {
        {"SAI_NEXT_HOP_GROUP_ATTR_TYPE", "SAI_NEXT_HOP_GROUP_TYPE_DYNAMIC_UNORDERED_ECMP"},
    });

    addOidObject(view, validNhg, {
        {"SAI_NEXT_HOP_GROUP_ATTR_TYPE", "SAI_NEXT_HOP_GROUP_TYPE_DYNAMIC_UNORDERED_ECMP"},
    });

    addOidObject(view, nh, {});

    addOidObject(view, deserializePair("oid:0x2d000000003a40", "oid:0x2d0000000000f060"), {
        {"SAI_NEXT_HOP_GROUP_MEMBER_ATTR_NEXT_HOP_GROUP_ID", "oid:0x5000000003a40"},
    });

    addOidObject(view, deserializePair("oid:0x2d000000003a41", "oid:0x2d0000000000f061"), {
        {"SAI_NEXT_HOP_GROUP_MEMBER_ATTR_NEXT_HOP_GROUP_ID", "oid:0x5000000003a41"},
        {"SAI_NEXT_HOP_GROUP_MEMBER_ATTR_NEXT_HOP_ID", "oid:0x4000000000a40"},
    });

    const auto *index = view.getNhgMemberIndex();

    ASSERT_NE(index, nullptr);
    EXPECT_EQ(index->size(), 1u);
}
