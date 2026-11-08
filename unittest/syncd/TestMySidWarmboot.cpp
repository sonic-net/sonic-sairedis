#include "AsicView.h"
#include "BestCandidateFinder.h"
#include "SaiObj.h"

#include "meta/sai_serialize.h"

#include "swss/logger.h"

#include <gtest/gtest.h>

#include <arpa/inet.h>
#include <cstring>
#include <memory>

using namespace syncd;

namespace
{
    constexpr sai_object_id_t SWITCH_VID = 0x21000000000000;
    constexpr sai_object_id_t VR_VID_CURRENT = 0x3000000000001;
    constexpr sai_object_id_t VR_VID_TEMP = 0x3000000000002;
    constexpr sai_object_id_t SWITCH_RID = 0x21000000000000;
    constexpr sai_object_id_t VR_RID = 0x4000000000001;

    sai_my_sid_entry_t makeMySidEntry(
            _In_ sai_object_id_t switchVid,
            _In_ sai_object_id_t vrVid,
            _In_ const char *sid = "3000:1:1::")
    {
        SWSS_LOG_ENTER();

        sai_my_sid_entry_t entry;

        memset(&entry, 0, sizeof(entry));

        entry.switch_id = switchVid;
        entry.vr_id = vrVid;
        entry.locator_block_len = 32;
        entry.locator_node_len = 16;
        entry.function_len = 0;
        entry.args_len = 0;

        inet_pton(AF_INET6, sid, &entry.sid);

        return entry;
    }

    std::string serializeMySidObjectId(
            _In_ sai_object_id_t switchVid,
            _In_ sai_object_id_t vrVid,
            _In_ const char *sid = "3000:1:1::")
    {
        SWSS_LOG_ENTER();

        return sai_serialize_my_sid_entry(makeMySidEntry(switchVid, vrVid, sid));
    }

    std::string mySidAsicKey(
            _In_ sai_object_id_t switchVid,
            _In_ sai_object_id_t vrVid,
            _In_ const char *sid = "3000:1:1::")
    {
        SWSS_LOG_ENTER();

        return "SAI_OBJECT_TYPE_MY_SID_ENTRY:" + serializeMySidObjectId(switchVid, vrVid, sid);
    }

    swss::TableDump makeSwitchDump()
    {
        SWSS_LOG_ENTER();

        return {
            {"SAI_OBJECT_TYPE_SWITCH:oid:0x21000000000000", {
                {"SAI_SWITCH_ATTR_ECMP_DEFAULT_HASH_SEED", "0"},
                {"SAI_SWITCH_ATTR_FDB_AGING_TIME", "600"},
                {"SAI_SWITCH_ATTR_INIT_SWITCH", "true"},
                {"SAI_SWITCH_ATTR_LAG_DEFAULT_HASH_SEED", "0"},
                {"SAI_SWITCH_ATTR_SRC_MAC_ADDRESS", "02:00:00:00:00:01"},
            }},
        };
    }

    swss::TableDump makeSwitchAndMySidDump(
            _In_ sai_object_id_t vrVid,
            _In_ const char *sid = "3000:1:1::")
    {
        SWSS_LOG_ENTER();

        auto dump = makeSwitchDump();

        dump[mySidAsicKey(SWITCH_VID, vrVid, sid)] = {
            {"SAI_MY_SID_ENTRY_ATTR_ENDPOINT_BEHAVIOR", "SAI_MY_SID_ENTRY_ENDPOINT_BEHAVIOR_E"},
        };

        return dump;
    }

    void wireVidRidMaps(
            _Inout_ AsicView &currentView,
            _Inout_ AsicView &temporaryView)
    {
        SWSS_LOG_ENTER();

        temporaryView.m_vidToRid[SWITCH_VID] = SWITCH_RID;
        temporaryView.m_vidToRid[VR_VID_TEMP] = VR_RID;

        currentView.m_ridToVid[SWITCH_RID] = SWITCH_VID;
        currentView.m_ridToVid[VR_RID] = VR_VID_CURRENT;
    }
}

TEST(MySidWarmboot, asicView_fromDump_deserializes_my_sid_entry)
{
    AsicView view;

    ASSERT_NO_THROW(view.fromDump(makeSwitchAndMySidDump(VR_VID_CURRENT)));

    ASSERT_EQ(view.m_soMySidEntries.size(), 1u);
    EXPECT_EQ(view.m_soAll.size(), 2u);

    auto obj = view.m_soMySidEntries.begin()->second;

    EXPECT_EQ(obj->getObjectType(), SAI_OBJECT_TYPE_MY_SID_ENTRY);
    EXPECT_EQ(obj->m_str_object_id, serializeMySidObjectId(SWITCH_VID, VR_VID_CURRENT));
    EXPECT_EQ(obj->m_meta_key.objectkey.key.my_sid_entry.switch_id, SWITCH_VID);
    EXPECT_EQ(obj->m_meta_key.objectkey.key.my_sid_entry.vr_id, VR_VID_CURRENT);
    EXPECT_EQ(obj->m_meta_key.objectkey.key.my_sid_entry.locator_block_len, 32u);
    EXPECT_EQ(obj->m_meta_key.objectkey.key.my_sid_entry.locator_node_len, 16u);
}

TEST(MySidWarmboot, asicView_create_and_remove_my_sid_entry)
{
    AsicView view;

    view.fromDump(makeSwitchAndMySidDump(VR_VID_CURRENT));

    ASSERT_EQ(view.m_soMySidEntries.size(), 1u);
    auto obj = view.m_soMySidEntries.begin()->second;

    view.asicRemoveObject(obj);
    EXPECT_TRUE(view.m_soMySidEntries.empty());

    view.asicCreateObject(obj);
    ASSERT_EQ(view.m_soMySidEntries.size(), 1u);
    EXPECT_EQ(view.m_soMySidEntries.begin()->first, serializeMySidObjectId(SWITCH_VID, VR_VID_CURRENT));
}

TEST(MySidWarmboot, my_sid_entry_serializes_with_exchanged_vr_vid)
{
    /*
     * ComparisonLogic rewrites the object id after translating struct VIDs
     * from the temporary view to the current view. Validate that contract
     * without calling private ComparisonLogic methods.
     */
    auto entry = makeMySidEntry(SWITCH_VID, VR_VID_TEMP);

    entry.vr_id = VR_VID_CURRENT;

    EXPECT_EQ(
            sai_serialize_my_sid_entry(entry),
            serializeMySidObjectId(SWITCH_VID, VR_VID_CURRENT));
}

TEST(MySidWarmboot, bestCandidateFinder_matches_my_sid_entry_after_vid_exchange)
{
    AsicView currentView;
    AsicView temporaryView;

    currentView.fromDump(makeSwitchAndMySidDump(VR_VID_CURRENT));
    temporaryView.fromDump(makeSwitchAndMySidDump(VR_VID_TEMP));

    wireVidRidMaps(currentView, temporaryView);

    ASSERT_EQ(temporaryView.m_soMySidEntries.size(), 1u);
    ASSERT_EQ(currentView.m_soMySidEntries.size(), 1u);

    auto tempObj = temporaryView.m_soMySidEntries.begin()->second;
    auto currentObj = currentView.m_soMySidEntries.begin()->second;

    BestCandidateFinder bcf(currentView, temporaryView, nullptr);

    auto match = bcf.findCurrentBestMatch(tempObj);

    ASSERT_NE(match, nullptr);
    EXPECT_EQ(match, currentObj);
    EXPECT_EQ(match->getObjectStatus(), SAI_OBJECT_STATUS_NOT_PROCESSED);
}

TEST(MySidWarmboot, bestCandidateFinder_returns_null_when_vid_mapping_missing)
{
    AsicView currentView;
    AsicView temporaryView;

    currentView.fromDump(makeSwitchAndMySidDump(VR_VID_CURRENT));
    temporaryView.fromDump(makeSwitchAndMySidDump(VR_VID_TEMP));

    ASSERT_EQ(temporaryView.m_soMySidEntries.size(), 1u);
    auto tempObj = temporaryView.m_soMySidEntries.begin()->second;

    BestCandidateFinder bcf(currentView, temporaryView, nullptr);

    EXPECT_EQ(bcf.findCurrentBestMatch(tempObj), nullptr);
}

TEST(MySidWarmboot, bestCandidateFinder_returns_null_when_entry_missing_in_current_view)
{
    AsicView currentView;
    AsicView temporaryView;

    currentView.fromDump(makeSwitchAndMySidDump(VR_VID_CURRENT, "3000:2:2::"));
    temporaryView.fromDump(makeSwitchAndMySidDump(VR_VID_TEMP, "3000:1:1::"));

    wireVidRidMaps(currentView, temporaryView);

    ASSERT_EQ(temporaryView.m_soMySidEntries.size(), 1u);
    auto tempObj = temporaryView.m_soMySidEntries.begin()->second;

    BestCandidateFinder bcf(currentView, temporaryView, nullptr);

    EXPECT_EQ(bcf.findCurrentBestMatch(tempObj), nullptr);
}

TEST(MySidWarmboot, bestCandidateFinder_throws_when_entry_already_processed)
{
    AsicView currentView;
    AsicView temporaryView;

    currentView.fromDump(makeSwitchAndMySidDump(VR_VID_CURRENT));
    temporaryView.fromDump(makeSwitchAndMySidDump(VR_VID_TEMP));

    wireVidRidMaps(currentView, temporaryView);

    ASSERT_EQ(temporaryView.m_soMySidEntries.size(), 1u);
    ASSERT_EQ(currentView.m_soMySidEntries.size(), 1u);

    auto tempObj = temporaryView.m_soMySidEntries.begin()->second;
    auto currentObj = currentView.m_soMySidEntries.begin()->second;

    currentObj->setObjectStatus(SAI_OBJECT_STATUS_FINAL);

    BestCandidateFinder bcf(currentView, temporaryView, nullptr);

    EXPECT_THROW(bcf.findCurrentBestMatch(tempObj), std::runtime_error);
}
