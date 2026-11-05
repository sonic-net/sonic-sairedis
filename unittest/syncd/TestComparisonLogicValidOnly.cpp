#include "AsicOperation.h"
#include "AsicView.h"
#include "ComparisonLogic.h"
#include "MockableSaiInterface.h"
#include "MockableSaiSwitchInterface.h"
#include "SaiObj.h"

#include "swss/logger.h"

#include <gtest/gtest.h>

#include <memory>
#include <set>

using namespace syncd;
using namespace unittests;

namespace
{
    constexpr sai_object_id_t SWITCH_VID = 0x21000000000000;
    constexpr sai_object_id_t SWITCH_RID = 0x21000000000000;

    const std::string WRED_OID = "oid:0x13000000000001";
    const std::string WRED_KEY = "SAI_OBJECT_TYPE_WRED:" + WRED_OID;

    /*
     * ComparisonLogic's constructor reads the switch's VID/RID maps and the
     * default trap group; the mock throws for those, so answer them here.
     */
    class TestSwitch: public MockableSaiSwitchInterface
    {
        public:

            TestSwitch(): MockableSaiSwitchInterface(SWITCH_VID, SWITCH_RID) { }

            virtual std::unordered_map<sai_object_id_t, sai_object_id_t> getVidToRidMap() const override { return {}; }

            virtual std::unordered_map<sai_object_id_t, sai_object_id_t> getRidToVidMap() const override { return {}; }

            virtual bool isDiscoveredRid(_In_ sai_object_id_t) const override { return false; }

            virtual sai_object_id_t getSwitchDefaultAttrOid(_In_ sai_attr_id_t) const override { return SAI_NULL_OBJECT_ID; }
    };

    /*
     * A WRED profile as orchagent creates it: green and yellow enabled, red
     * as given, no ECN marking. Red's thresholds are added only when asked.
     */
    swss::TableDump makeDump(
            _In_ bool redEnabled,
            _In_ bool withRedThresholds,
            _In_ const char *redMin = "3000000")
    {
        SWSS_LOG_ENTER();

        std::map<std::string, std::string> wred = {
            {"SAI_WRED_ATTR_GREEN_ENABLE", "true"},
            {"SAI_WRED_ATTR_GREEN_MIN_THRESHOLD", "1000000"},
            {"SAI_WRED_ATTR_GREEN_MAX_THRESHOLD", "5000000"},
            {"SAI_WRED_ATTR_YELLOW_ENABLE", "true"},
            {"SAI_WRED_ATTR_YELLOW_MIN_THRESHOLD", "2000000"},
            {"SAI_WRED_ATTR_YELLOW_MAX_THRESHOLD", "8000000"},
            {"SAI_WRED_ATTR_RED_ENABLE", redEnabled ? "true" : "false"},
            {"SAI_WRED_ATTR_ECN_MARK_MODE", "SAI_ECN_MARK_MODE_NONE"},
        };

        if (withRedThresholds)
        {
            wred["SAI_WRED_ATTR_RED_MIN_THRESHOLD"] = redMin;
            wred["SAI_WRED_ATTR_RED_MAX_THRESHOLD"] = "12000000";
        }

        return {
            {"SAI_OBJECT_TYPE_SWITCH:oid:0x21000000000000", {
                {"SAI_SWITCH_ATTR_INIT_SWITCH", "true"},
                {"SAI_SWITCH_ATTR_SRC_MAC_ADDRESS", "02:00:00:00:00:01"},
            }},
            {WRED_KEY, wred},
        };
    }

    struct Views
    {
        std::shared_ptr<AsicView> current = std::make_shared<AsicView>();
        std::shared_ptr<AsicView> temp = std::make_shared<AsicView>();
        std::shared_ptr<ComparisonLogic> cl;

        Views(
                _In_ const swss::TableDump &currentDump,
                _In_ const swss::TableDump &tempDump)
        {
            SWSS_LOG_ENTER();

            current->fromDump(currentDump);
            temp->fromDump(tempDump);

            auto sai = std::make_shared<MockableSaiInterface>();
            auto sw = std::make_shared<TestSwitch>();

            cl = std::make_shared<ComparisonLogic>(sai, sw, nullptr, std::set<sai_object_id_t>{}, current, temp, nullptr);
        }

        std::shared_ptr<SaiObj> currentWred() { return current->m_soAll.at(WRED_OID); }
        std::shared_ptr<SaiObj> tempWred() { return temp->m_soAll.at(WRED_OID); }
    };

    const sai_attr_metadata_t* redMinMeta()
    {
        SWSS_LOG_ENTER();

        return sai_metadata_get_attr_metadata(SAI_OBJECT_TYPE_WRED, SAI_WRED_ATTR_RED_MIN_THRESHOLD);
    }

    std::vector<sai_attribute_t> attrsOf(
            _In_ const std::shared_ptr<SaiObj> &obj)
    {
        SWSS_LOG_ENTER();

        std::vector<sai_attribute_t> attrs;

        for (auto &at: obj->getAllAttributes())
        {
            attrs.push_back(*at.second->getSaiAttr());
        }

        return attrs;
    }

    size_t positionOf(
            _In_ const std::vector<std::shared_ptr<SaiAttr>> &ordered,
            _In_ sai_attr_id_t id)
    {
        SWSS_LOG_ENTER();

        for (size_t i = 0; i < ordered.size(); i++)
        {
            if (ordered[i]->getSaiAttr()->id == id)
            {
                return i;
            }
        }

        return ordered.size();
    }
}

TEST(ComparisonLogicValidOnly, threshold_of_disabled_unmarked_colour_is_unmet)
{
    Views v(makeDump(false, true), makeDump(false, false));

    EXPECT_TRUE(ComparisonLogic::isValidOnlyConditionUnmet(attrsOf(v.tempWred()), redMinMeta()));
}

TEST(ComparisonLogicValidOnly, threshold_of_enabled_colour_is_met)
{
    Views v(makeDump(true, true), makeDump(true, false));

    EXPECT_FALSE(ComparisonLogic::isValidOnlyConditionUnmet(attrsOf(v.tempWred()), redMinMeta()));
}

TEST(ComparisonLogicValidOnly, threshold_of_ecn_marked_colour_is_met)
{
    auto dump = makeDump(false, false);
    dump[WRED_KEY]["SAI_WRED_ATTR_ECN_MARK_MODE"] = "SAI_ECN_MARK_MODE_RED";

    Views v(makeDump(false, true), dump);

    EXPECT_FALSE(ComparisonLogic::isValidOnlyConditionUnmet(attrsOf(v.tempWred()), redMinMeta()));
}

TEST(ComparisonLogicValidOnly, attribute_without_valid_only_is_never_unmet)
{
    Views v(makeDump(false, true), makeDump(false, false));

    auto meta = sai_metadata_get_attr_metadata(SAI_OBJECT_TYPE_WRED, SAI_WRED_ATTR_RED_ENABLE);

    EXPECT_FALSE(ComparisonLogic::isValidOnlyConditionUnmet(attrsOf(v.tempWred()), meta));
    EXPECT_FALSE(ComparisonLogic::isValidOnlyConditionUnmet(attrsOf(v.tempWred()), nullptr));
}

/*
 * Warm reboot after a colour was disabled by a set: the current object keeps
 * the thresholds from its create, the recreated profile omits them. They must
 * not be brought to their default, since the SAI rejects a set on a disabled
 * colour; the current values are carried over instead.
 */
TEST(ComparisonLogicValidOnly, missing_threshold_of_disabled_colour_is_transferred_not_defaulted)
{
    Views v(makeDump(false, true), makeDump(false, false));

    ASSERT_FALSE(v.tempWred()->hasAttr(SAI_WRED_ATTR_RED_MIN_THRESHOLD));

    EXPECT_TRUE(v.cl->performObjectSetTransition(*v.current, *v.temp, v.currentWred(), v.tempWred(), false));
    EXPECT_TRUE(v.cl->performObjectSetTransition(*v.current, *v.temp, v.currentWred(), v.tempWred(), true));

    EXPECT_EQ(v.current->asicGetOperationsCount(), 0u);
    ASSERT_TRUE(v.tempWred()->hasAttr(SAI_WRED_ATTR_RED_MIN_THRESHOLD));
    EXPECT_EQ(v.tempWred()->getSaiAttr(SAI_WRED_ATTR_RED_MIN_THRESHOLD)->getStrAttrValue(), "3000000");
    ASSERT_TRUE(v.tempWred()->hasAttr(SAI_WRED_ATTR_RED_MAX_THRESHOLD));
}

/*
 * Warm reboot after a threshold change the SAI refused: CONFIG_DB (and so the
 * recreated profile) carries the new value, the ASIC the old one. No SET is
 * generated while the colour is disabled; the current value is kept.
 */
TEST(ComparisonLogicValidOnly, differing_threshold_of_disabled_colour_is_not_set)
{
    Views v(makeDump(false, true, "3000000"), makeDump(false, true, "2500000"));

    EXPECT_TRUE(v.cl->performObjectSetTransition(*v.current, *v.temp, v.currentWred(), v.tempWred(), false));
    EXPECT_TRUE(v.cl->performObjectSetTransition(*v.current, *v.temp, v.currentWred(), v.tempWred(), true));

    EXPECT_EQ(v.current->asicGetOperationsCount(), 0u);
    EXPECT_EQ(v.tempWred()->getSaiAttr(SAI_WRED_ATTR_RED_MIN_THRESHOLD)->getStrAttrValue(), "3000000");
}

/*
 * The control: with the colour enabled the threshold is valid, so a differing
 * value is set on the current object as before.
 */
TEST(ComparisonLogicValidOnly, differing_threshold_of_enabled_colour_is_set)
{
    Views v(makeDump(true, true, "3000000"), makeDump(true, true, "2500000"));

    EXPECT_TRUE(v.cl->performObjectSetTransition(*v.current, *v.temp, v.currentWred(), v.tempWred(), false));
    EXPECT_TRUE(v.cl->performObjectSetTransition(*v.current, *v.temp, v.currentWred(), v.tempWred(), true));

    EXPECT_EQ(v.current->asicGetOperationsCount(), 1u);
    EXPECT_EQ(v.tempWred()->getSaiAttr(SAI_WRED_ATTR_RED_MIN_THRESHOLD)->getStrAttrValue(), "2500000");
}

TEST(ComparisonLogicValidOnly, condition_sources_are_ordered_first)
{
    Views v(makeDump(false, true), makeDump(true, true, "2500000"));

    auto ordered = ComparisonLogic::orderConditionSourcesFirst(v.tempWred());

    ASSERT_EQ(ordered.size(), v.tempWred()->getAllAttributes().size());

    const std::set<sai_attr_id_t> sources = {
        SAI_WRED_ATTR_GREEN_ENABLE, SAI_WRED_ATTR_YELLOW_ENABLE, SAI_WRED_ATTR_RED_ENABLE, SAI_WRED_ATTR_ECN_MARK_MODE };

    for (size_t i = 0; i < ordered.size(); i++)
    {
        EXPECT_EQ(sources.count(ordered[i]->getAttrMetadata()->attrid) > 0, i < sources.size()) << "position " << i;
    }
}

/*
 * A condition source that is itself valid only: mirror session VLAN_ID
 * depends on VLAN_HEADER_VALID, which depends on TYPE.
 */
TEST(ComparisonLogicValidOnly, chained_condition_sources_are_ordered_by_depth)
{
    swss::TableDump dump = {
        {"SAI_OBJECT_TYPE_SWITCH:oid:0x21000000000000", {
            {"SAI_SWITCH_ATTR_INIT_SWITCH", "true"},
            {"SAI_SWITCH_ATTR_SRC_MAC_ADDRESS", "02:00:00:00:00:01"},
        }},
        {"SAI_OBJECT_TYPE_MIRROR_SESSION:oid:0xe000000000001", {
            {"SAI_MIRROR_SESSION_ATTR_VLAN_ID", "100"},
            {"SAI_MIRROR_SESSION_ATTR_VLAN_HEADER_VALID", "true"},
            {"SAI_MIRROR_SESSION_ATTR_TYPE", "SAI_MIRROR_SESSION_TYPE_ENHANCED_REMOTE"},
            {"SAI_MIRROR_SESSION_ATTR_TTL", "255"},
        }},
    };

    AsicView view;
    view.fromDump(dump);

    auto ordered = ComparisonLogic::orderConditionSourcesFirst(view.m_soAll.at("oid:0xe000000000001"));

    ASSERT_EQ(ordered.size(), 4u);
    EXPECT_LT(positionOf(ordered, SAI_MIRROR_SESSION_ATTR_TYPE), positionOf(ordered, SAI_MIRROR_SESSION_ATTR_VLAN_HEADER_VALID));
    EXPECT_LT(positionOf(ordered, SAI_MIRROR_SESSION_ATTR_VLAN_HEADER_VALID), positionOf(ordered, SAI_MIRROR_SESSION_ATTR_VLAN_ID));
}

/*
 * OID attributes are never carried over or skipped, even when their
 * condition is unmet: hostif trap TRAP_GROUP is valid only on a TRAP or COPY
 * action.
 */
TEST(ComparisonLogicValidOnly, oid_attribute_is_not_skipped)
{
    auto meta = sai_metadata_get_attr_metadata(SAI_OBJECT_TYPE_HOSTIF_TRAP, SAI_HOSTIF_TRAP_ATTR_TRAP_GROUP);

    ASSERT_NE(meta, nullptr);
    ASSERT_TRUE(meta->isvalidonly);
    ASSERT_TRUE(meta->isoidattribute);

    sai_attribute_t action;
    action.id = SAI_HOSTIF_TRAP_ATTR_PACKET_ACTION;
    action.value.s32 = SAI_PACKET_ACTION_DROP;

    std::vector<sai_attribute_t> attrs = { action };

    EXPECT_TRUE(ComparisonLogic::isValidOnlyConditionUnmet(attrs, meta));
    EXPECT_FALSE(ComparisonLogic::isSetSkippedForUnmetValidOnly(attrs, meta));
    EXPECT_TRUE(ComparisonLogic::isSetSkippedForUnmetValidOnly(attrsOf(Views(makeDump(false, true), makeDump(false, false)).tempWred()), redMinMeta()));
}

/*
 * Warm reboot after a threshold was added while the colour was disabled and
 * the SAI refused it: the recreated profile carries the threshold, the
 * current object does not. No SET while the colour is disabled.
 */
TEST(ComparisonLogicValidOnly, threshold_only_on_temporary_object_of_disabled_colour_is_not_set)
{
    auto temp = makeDump(false, false);
    temp[WRED_KEY]["SAI_WRED_ATTR_RED_MIN_THRESHOLD"] = "2500000";

    Views v(makeDump(false, false), temp);

    ASSERT_FALSE(v.currentWred()->hasAttr(SAI_WRED_ATTR_RED_MIN_THRESHOLD));

    EXPECT_TRUE(v.cl->performObjectSetTransition(*v.current, *v.temp, v.currentWred(), v.tempWred(), false));
    EXPECT_TRUE(v.cl->performObjectSetTransition(*v.current, *v.temp, v.currentWred(), v.tempWred(), true));

    EXPECT_EQ(v.current->asicGetOperationsCount(), 0u);
    EXPECT_EQ(v.tempWred()->getSaiAttr(SAI_WRED_ATTR_RED_MIN_THRESHOLD)->getStrAttrValue(), "2500000");
}

/*
 * The control: with the colour enabled, the threshold present only on the
 * recreated profile is set as before.
 */
TEST(ComparisonLogicValidOnly, threshold_only_on_temporary_object_of_enabled_colour_is_set)
{
    auto temp = makeDump(true, false);
    temp[WRED_KEY]["SAI_WRED_ATTR_RED_MIN_THRESHOLD"] = "2500000";

    Views v(makeDump(true, false), temp);

    EXPECT_TRUE(v.cl->performObjectSetTransition(*v.current, *v.temp, v.currentWred(), v.tempWred(), false));
    EXPECT_TRUE(v.cl->performObjectSetTransition(*v.current, *v.temp, v.currentWred(), v.tempWred(), true));

    EXPECT_EQ(v.current->asicGetOperationsCount(), 1u);
}

/*
 * The colour is enabled and its threshold changed in the same transition: the
 * SET that makes the condition true must be generated before the threshold SET,
 * whatever the attribute map's order.
 */
TEST(ComparisonLogicValidOnly, enable_set_precedes_dependent_threshold_set)
{
    Views v(makeDump(false, true, "3000000"), makeDump(true, true, "2500000"));

    EXPECT_TRUE(v.cl->performObjectSetTransition(*v.current, *v.temp, v.currentWred(), v.tempWred(), false));
    EXPECT_TRUE(v.cl->performObjectSetTransition(*v.current, *v.temp, v.currentWred(), v.tempWred(), true));

    ASSERT_EQ(v.current->asicGetOperationsCount(), 2u);

    auto &ops = v.current->m_asicOperations;

    EXPECT_EQ(fvField(kfvFieldsValues(*ops[0].m_op).at(0)), "SAI_WRED_ATTR_RED_ENABLE");
    EXPECT_EQ(fvField(kfvFieldsValues(*ops[1].m_op).at(0)), "SAI_WRED_ATTR_RED_MIN_THRESHOLD");
}
