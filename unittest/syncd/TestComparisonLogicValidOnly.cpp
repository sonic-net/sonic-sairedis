#include "AsicOperation.h"
#include "AsicView.h"
#include "ComparisonLogic.h"
#include "MockableSaiInterface.h"
#include "MockableSaiSwitchInterface.h"
#include "SaiObj.h"
#include "Workaround.h"

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

    constexpr sai_object_id_t WRED_VID = 0x13000000000001;
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

/*
 * A failed SET of a threshold whose colour is neither enabled nor ECN-marked
 * is ignored during apply view: the threshold has no effect in that state.
 */
TEST(ComparisonLogicValidOnly, failed_set_of_disabled_colour_threshold_is_ignored)
{
    AsicView view;
    view.fromDump(makeDump(false, true));

    EXPECT_TRUE(Workaround::isSetAttributeWorkaroundDuringApplyView(view, WRED_VID, SAI_WRED_ATTR_RED_MIN_THRESHOLD, SAI_STATUS_INVALID_PARAMETER));
    EXPECT_TRUE(Workaround::isSetAttributeWorkaroundDuringApplyView(view, WRED_VID, SAI_WRED_ATTR_RED_MAX_THRESHOLD, SAI_STATUS_INVALID_PARAMETER));
}

TEST(ComparisonLogicValidOnly, failed_set_of_enabled_colour_threshold_is_not_ignored)
{
    AsicView view;
    view.fromDump(makeDump(true, true));

    EXPECT_FALSE(Workaround::isSetAttributeWorkaroundDuringApplyView(view, WRED_VID, SAI_WRED_ATTR_RED_MIN_THRESHOLD, SAI_STATUS_INVALID_PARAMETER));
}

TEST(ComparisonLogicValidOnly, failed_set_of_ecn_marked_colour_threshold_is_not_ignored)
{
    auto dump = makeDump(false, true);
    dump[WRED_KEY]["SAI_WRED_ATTR_ECN_MARK_MODE"] = "SAI_ECN_MARK_MODE_RED";

    AsicView view;
    view.fromDump(dump);

    EXPECT_FALSE(Workaround::isSetAttributeWorkaroundDuringApplyView(view, WRED_VID, SAI_WRED_ATTR_RED_MIN_THRESHOLD, SAI_STATUS_INVALID_PARAMETER));
}

TEST(ComparisonLogicValidOnly, failed_set_of_attribute_without_valid_only_is_not_ignored)
{
    AsicView view;
    view.fromDump(makeDump(false, true));

    EXPECT_FALSE(Workaround::isSetAttributeWorkaroundDuringApplyView(view, WRED_VID, SAI_WRED_ATTR_RED_ENABLE, SAI_STATUS_INVALID_PARAMETER));
    EXPECT_FALSE(Workaround::isSetAttributeWorkaroundDuringApplyView(view, WRED_VID, SAI_WRED_ATTR_RED_MIN_THRESHOLD, SAI_STATUS_SUCCESS));
}

/*
 * The condition is evaluated on the object as it is after the transition:
 * red is enabled on the ASIC and disabled in the new configuration together
 * with a threshold change, so the threshold SET runs after RED_ENABLE false
 * and its failure is ignored.
 */
TEST(ComparisonLogicValidOnly, condition_is_evaluated_after_the_transition)
{
    Views v(makeDump(true, true, "3000000"), makeDump(false, true, "2500000"));

    EXPECT_FALSE(Workaround::isSetAttributeWorkaroundDuringApplyView(*v.current, WRED_VID, SAI_WRED_ATTR_RED_MIN_THRESHOLD, SAI_STATUS_INVALID_PARAMETER));

    EXPECT_TRUE(v.cl->performObjectSetTransition(*v.current, *v.temp, v.currentWred(), v.tempWred(), false));
    EXPECT_TRUE(v.cl->performObjectSetTransition(*v.current, *v.temp, v.currentWred(), v.tempWred(), true));

    ASSERT_EQ(v.current->asicGetOperationsCount(), 2u);
    EXPECT_TRUE(Workaround::isSetAttributeWorkaroundDuringApplyView(*v.current, WRED_VID, SAI_WRED_ATTR_RED_MIN_THRESHOLD, SAI_STATUS_INVALID_PARAMETER));
}

/*
 * Warm reboot after a threshold change the SAI refused while the colour was
 * disabled: the recreated profile carries the new value. The SET is still
 * generated, so a SAI that stores the value while the condition is unmet
 * receives it.
 */
TEST(ComparisonLogicValidOnly, differing_threshold_of_disabled_colour_is_set)
{
    Views v(makeDump(false, true, "3000000"), makeDump(false, true, "2500000"));

    EXPECT_TRUE(v.cl->performObjectSetTransition(*v.current, *v.temp, v.currentWred(), v.tempWred(), false));
    EXPECT_TRUE(v.cl->performObjectSetTransition(*v.current, *v.temp, v.currentWred(), v.tempWred(), true));

    ASSERT_EQ(v.current->asicGetOperationsCount(), 1u);
    EXPECT_EQ(fvField(kfvFieldsValues(*v.current->m_asicOperations[0].m_op).at(0)), "SAI_WRED_ATTR_RED_MIN_THRESHOLD");
}

/*
 * Warm reboot after a colour was disabled: the recreated profile omits its
 * thresholds, so they are brought to their default as for any attribute.
 */
TEST(ComparisonLogicValidOnly, missing_threshold_of_disabled_colour_is_brought_to_default)
{
    Views v(makeDump(false, true), makeDump(false, false));

    EXPECT_TRUE(v.cl->performObjectSetTransition(*v.current, *v.temp, v.currentWred(), v.tempWred(), false));
    EXPECT_TRUE(v.cl->performObjectSetTransition(*v.current, *v.temp, v.currentWred(), v.tempWred(), true));

    EXPECT_EQ(v.current->asicGetOperationsCount(), 2u);
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
