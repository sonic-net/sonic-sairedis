#include "SwitchVppTestHarness.h"

#include "vpp/SwitchVpp.h"
#include "meta/sai_serialize.h"
#include "saimetadatautils.h"

#include <gtest/gtest.h>

#include <cstdio>
#include <cstring>
#include <deque>
#include <string>
#include <vector>

using namespace saivs;
using namespace saivs_test;

namespace
{
    // One vpp_sflow_drop_monitoring_set() call, holding the flag it was given.
    std::vector<bool> drop_monitoring_calls;

    // Return codes handed to successive calls; empty means success.
    std::deque<int> drop_monitoring_results;

    struct DropPolicerCall
    {
        std::string name;
        // drop_monitoring_calls.size() at the time, to check ordering
        size_t drop_monitoring_calls_before;
    };

    std::vector<DropPolicerCall> drop_policer_calls;

    int nextDropMonitoringResult()
    {
        SWSS_LOG_ENTER();

        if (drop_monitoring_results.empty())
        {
            return 0;
        }

        int result = drop_monitoring_results.front();
        drop_monitoring_results.pop_front();
        return result;
    }

    /*
     * Size orchagent uses for its enum capability. See
     * SwitchOrch::querySwitchMirrorOnDropCapability().
     */
    uint32_t metadataValuesCount(
            sai_object_type_t object_type,
            sai_attr_id_t attr_id)
    {
        SWSS_LOG_ENTER();

        auto meta = sai_metadata_get_attr_metadata(object_type, attr_id);

        EXPECT_NE(nullptr, meta);
        EXPECT_TRUE(meta->isenum);

        return (uint32_t) meta->enummetadata->valuescount;
    }
}

extern "C" int __wrap_vpp_sflow_drop_monitoring_set(bool enable)
{
    SWSS_LOG_ENTER();

    drop_monitoring_calls.push_back(enable);
    return nextDropMonitoringResult();
}

extern "C" int __wrap_vpp_sflow_drop_policer_set(const char *name)
{
    SWSS_LOG_ENTER();

    drop_policer_calls.push_back({name ? name : "", drop_monitoring_calls.size()});
    return 0;
}

namespace
{
    class SwitchVppSflowTest : public SwitchVppTest
    {
        protected:

            void SetUp() override
            {
                SwitchVppTest::SetUp();

                drop_monitoring_calls.clear();
                drop_monitoring_results.clear();
                drop_policer_calls.clear();
            }

            sai_object_id_t createObject(
                    sai_object_type_t type,
                    const std::vector<sai_attribute_t>& attrs)
            {
                SWSS_LOG_ENTER();

                sai_object_id_t oid = m_mgr->allocateNewObjectId(type, m_switchId);

                EXPECT_EQ(SAI_STATUS_SUCCESS, m_sw->create_internal(type,
                    sai_serialize_object_id(oid), m_switchId,
                    (uint32_t)attrs.size(), attrs.data()));

                return oid;
            }

            /*
             * A policer with the VPP policer createPolicer() would have
             * recorded for it, without calling VPP.
             */
            sai_object_id_t createPacketPolicer(
                    uint64_t limit)
            {
                SWSS_LOG_ENTER();

                sai_attribute_t attr;
                std::vector<sai_attribute_t> pol;

                attr.id = SAI_POLICER_ATTR_METER_TYPE;
                attr.value.s32 = SAI_METER_TYPE_PACKETS;
                pol.push_back(attr);
                attr.id = SAI_POLICER_ATTR_MODE;
                attr.value.s32 = SAI_POLICER_MODE_SR_TCM;
                pol.push_back(attr);
                attr.id = SAI_POLICER_ATTR_CIR;
                attr.value.u64 = limit;
                pol.push_back(attr);
                attr.id = SAI_POLICER_ATTR_CBS;
                attr.value.u64 = limit;
                pol.push_back(attr);

                auto oid = createObject(SAI_OBJECT_TYPE_POLICER, pol);

                vpp_policer_entry_t entry;
                memset(&entry, 0, sizeof(entry));
                snprintf(entry.vpp_name, sizeof(entry.vpp_name), "%s",
                        vppPolicerName(oid).c_str());
                entry.vpp_policer_index = (uint32_t)m_sw->m_policer_map.size();
                m_sw->m_policer_map[oid] = entry;

                return oid;
            }

            static std::string vppPolicerName(
                    sai_object_id_t policer)
            {
                SWSS_LOG_ENTER();

                return "copp-policer-" + sai_serialize_object_id(policer);
            }

            /*
             * The object chain SflowDropMonitor::initializeDropMonitor()
             * builds, as SAI state only. Returns the TAM object.
             */

            sai_object_id_t createDropMonitorChain(
                    uint64_t limit,
                    sai_object_id_t& policer,
                    sai_object_id_t* trap_group = nullptr)
            {
                SWSS_LOG_ENTER();

                sai_attribute_t attr;

                policer = createPacketPolicer(limit);

                attr.id = SAI_HOSTIF_TRAP_GROUP_ATTR_POLICER;
                attr.value.oid = policer;
                auto group = createObject(SAI_OBJECT_TYPE_HOSTIF_TRAP_GROUP, {attr});
                if (trap_group)
                {
                    *trap_group = group;
                }

                attr.id = SAI_HOSTIF_USER_DEFINED_TRAP_ATTR_TRAP_GROUP;
                attr.value.oid = group;
                auto trap = createObject(SAI_OBJECT_TYPE_HOSTIF_USER_DEFINED_TRAP, {attr});

                attr.id = SAI_TAM_COLLECTOR_ATTR_HOSTIF_TRAP;
                attr.value.oid = trap;
                auto collector = createObject(SAI_OBJECT_TYPE_TAM_COLLECTOR, {attr});

                std::vector<sai_attribute_t> ev;
                attr.id = SAI_TAM_EVENT_ATTR_TYPE;
                attr.value.s32 = SAI_TAM_EVENT_TYPE_PACKET_DROP;
                ev.push_back(attr);
                attr.id = SAI_TAM_EVENT_ATTR_COLLECTOR_LIST;
                attr.value.objlist.count = 1;
                attr.value.objlist.list = &collector;
                ev.push_back(attr);
                auto event = createObject(SAI_OBJECT_TYPE_TAM_EVENT, ev);

                attr.id = SAI_TAM_ATTR_EVENT_OBJECTS_LIST;
                attr.value.objlist.count = 1;
                attr.value.objlist.list = &event;
                return createObject(SAI_OBJECT_TYPE_TAM, {attr});
            }
    };
}

/*
 * The provider claims exactly the two (object_type, attr_id) pairs
 * SwitchOrch::querySwitchMirrorOnDropCapability() probes, and answers each
 * with the single value that probe searches for.
 */
TEST(SwitchVppSflow, getSflowEnumValuesCapability)
{
    std::vector<sai_int32_t> values;

    ASSERT_TRUE(SwitchVpp::getSflowEnumValuesCapability(
        SAI_OBJECT_TYPE_TAM_EVENT, SAI_TAM_EVENT_ATTR_TYPE, values));
    ASSERT_EQ(1u, values.size());
    EXPECT_EQ((sai_int32_t)SAI_TAM_EVENT_TYPE_PACKET_DROP, values[0]);

    values.clear();

    ASSERT_TRUE(SwitchVpp::getSflowEnumValuesCapability(
        SAI_OBJECT_TYPE_HOSTIF_USER_DEFINED_TRAP, SAI_HOSTIF_USER_DEFINED_TRAP_ATTR_TYPE, values));
    ASSERT_EQ(1u, values.size());
    EXPECT_EQ((sai_int32_t)SAI_HOSTIF_USER_DEFINED_TRAP_TYPE_TAM, values[0]);

    // Another enum attribute on an owned object type is not claimed, and an
    // unclaimed pair must leave the caller's vector untouched.
    values.clear();

    EXPECT_FALSE(SwitchVpp::getSflowEnumValuesCapability(
        SAI_OBJECT_TYPE_TAM_EVENT, SAI_TAM_EVENT_ATTR_PACKET_DROP_TYPE, values));
    EXPECT_TRUE(values.empty());

    // A claimed attr id against a different object type is not claimed.
    EXPECT_FALSE(SwitchVpp::getSflowEnumValuesCapability(
        SAI_OBJECT_TYPE_PORT, SAI_HOSTIF_USER_DEFINED_TRAP_ATTR_TYPE, values));
    EXPECT_TRUE(values.empty());
}

/*
 * Regression: orchagent sizes its buffer from its own copy of the SAI
 * metadata, which holds more values than the provider advertises. A buffer
 * larger than needed is not an overflow - reporting one here made
 * querySwitchMirrorOnDropCapability() give up and write
 * MIRROR_ON_DROP_CAPABLE=false.
 */
TEST_F(SwitchVppSflowTest, queryAttrEnumValuesCapabilityAcceptsOversizedBuffer)
{
    auto count = metadataValuesCount(
        SAI_OBJECT_TYPE_HOSTIF_USER_DEFINED_TRAP, SAI_HOSTIF_USER_DEFINED_TRAP_ATTR_TYPE);

    ASSERT_GT(count, 1u);

    std::vector<sai_int32_t> buffer(count, -1);

    sai_s32_list_t list;

    list.count = count;
    list.list = buffer.data();

    ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->queryAttrEnumValuesCapability(
        m_switchId, SAI_OBJECT_TYPE_HOSTIF_USER_DEFINED_TRAP,
        SAI_HOSTIF_USER_DEFINED_TRAP_ATTR_TYPE, &list));

    // Only the advertised values are reported back, not the buffer size.
    ASSERT_EQ(1u, list.count);
    EXPECT_EQ((sai_int32_t)SAI_HOSTIF_USER_DEFINED_TRAP_TYPE_TAM, list.list[0]);

    count = metadataValuesCount(SAI_OBJECT_TYPE_TAM_EVENT, SAI_TAM_EVENT_ATTR_TYPE);

    ASSERT_GT(count, 1u);

    buffer.assign(count, -1);
    list.count = count;
    list.list = buffer.data();

    ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->queryAttrEnumValuesCapability(
        m_switchId, SAI_OBJECT_TYPE_TAM_EVENT, SAI_TAM_EVENT_ATTR_TYPE, &list));

    ASSERT_EQ(1u, list.count);
    EXPECT_EQ((sai_int32_t)SAI_TAM_EVENT_TYPE_PACKET_DROP, list.list[0]);
}

/*
 * A buffer too small for the advertised values reports the size needed,
 * per the sai_s32_list_t contract.
 */
TEST_F(SwitchVppSflowTest, queryAttrEnumValuesCapabilityReportsRequiredSize)
{
    sai_s32_list_t list;

    list.count = 0;
    list.list = nullptr;

    ASSERT_EQ(SAI_STATUS_BUFFER_OVERFLOW, m_sw->queryAttrEnumValuesCapability(
        m_switchId, SAI_OBJECT_TYPE_TAM_EVENT, SAI_TAM_EVENT_ATTR_TYPE, &list));

    EXPECT_EQ(1u, list.count);
}

/*
 * The dispatcher walks every provider and only then falls back on
 * SwitchStateBase, so registering the provider must not shadow the
 * ones already there.
 */
TEST_F(SwitchVppSflowTest, queryAttrEnumValuesCapabilityChainsProviders)
{
    sai_int32_t storage[16];

    sai_s32_list_t list;

    list.count = 16;
    list.list = storage;

    // Owned by the trim provider, which is consulted before the sFlow one.
    ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->queryAttrEnumValuesCapability(
        m_switchId, SAI_OBJECT_TYPE_SWITCH,
        SAI_SWITCH_ATTR_PACKET_TRIM_DSCP_RESOLUTION_MODE, &list));

    ASSERT_EQ(1u, list.count);
    EXPECT_EQ((sai_int32_t)SAI_PACKET_TRIM_DSCP_RESOLUTION_MODE_DSCP_VALUE, list.list[0]);

    // Owned by no provider, so SwitchStateBase answers.
    list.count = 16;
    list.list = storage;

    EXPECT_EQ(SAI_STATUS_NOT_SUPPORTED, m_sw->queryAttrEnumValuesCapability(
        m_switchId, SAI_OBJECT_TYPE_SWITCH, SAI_SWITCH_ATTR_SWITCHING_MODE, &list));
}

/*
 * The global VPP plugin flag is driven straight from the SAI call, and any
 * non-zero VPP status is a failure.
 */
TEST_F(SwitchVppSflowTest, sflowDropMonitoringSet)
{
    ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->sflowDropMonitoringSet(true));
    ASSERT_EQ(1u, drop_monitoring_calls.size());
    EXPECT_TRUE(drop_monitoring_calls[0]);

    ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->sflowDropMonitoringSet(false));
    ASSERT_EQ(2u, drop_monitoring_calls.size());
    EXPECT_FALSE(drop_monitoring_calls[1]);

    // VPP reports errors as negative values, but any non-zero status counts.
    drop_monitoring_results.push_back(-1);

    EXPECT_EQ(SAI_STATUS_FAILURE, m_sw->sflowDropMonitoringSet(true));
    EXPECT_EQ(3u, drop_monitoring_calls.size());

    drop_monitoring_results.push_back(1);

    EXPECT_EQ(SAI_STATUS_FAILURE, m_sw->sflowDropMonitoringSet(false));
    EXPECT_EQ(4u, drop_monitoring_calls.size());
}

/*
 * SflowOrch arms drop monitoring by binding a TAM object to the switch and
 * disarms it by clearing the list; only the list being non-empty matters.
 */
TEST_F(SwitchVppSflowTest, tamObjectIdDrivesDropMonitoring)
{
    sai_object_id_t tam = m_mgr->allocateNewObjectId(SAI_OBJECT_TYPE_TAM, m_switchId);

    sai_attribute_t attr;

    attr.id = SAI_SWITCH_ATTR_TAM_OBJECT_ID;
    attr.value.objlist.count = 1;
    attr.value.objlist.list = &tam;

    ASSERT_EQ(SAI_STATUS_SUCCESS, setSwitch(attr));

    ASSERT_EQ(1u, drop_monitoring_calls.size());
    EXPECT_TRUE(drop_monitoring_calls[0]);

    // The case falls through to set_internal(), so the bound objects are still
    // cached and readable back.
    sai_object_id_t readback[4];

    sai_attribute_t get_attr;

    get_attr.id = SAI_SWITCH_ATTR_TAM_OBJECT_ID;
    get_attr.value.objlist.count = 4;
    get_attr.value.objlist.list = readback;

    ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->get(
        SAI_OBJECT_TYPE_SWITCH, sai_serialize_object_id(m_switchId), 1, &get_attr));

    ASSERT_EQ(1u, get_attr.value.objlist.count);
    EXPECT_EQ(tam, get_attr.value.objlist.list[0]);

    // Clearing the bound TAM objects disarms it.
    attr.value.objlist.count = 0;

    ASSERT_EQ(SAI_STATUS_SUCCESS, setSwitch(attr));

    ASSERT_EQ(2u, drop_monitoring_calls.size());
    EXPECT_FALSE(drop_monitoring_calls[1]);
}

/*
 * A VPP failure must surface to the caller rather than leaving SAI state
 * claiming drop monitoring is armed.
 */
TEST_F(SwitchVppSflowTest, tamObjectIdPropagatesVppFailure)
{
    drop_monitoring_results.push_back(-1);

    sai_object_id_t tam = m_mgr->allocateNewObjectId(SAI_OBJECT_TYPE_TAM, m_switchId);

    sai_attribute_t attr;

    attr.id = SAI_SWITCH_ATTR_TAM_OBJECT_ID;
    attr.value.objlist.count = 1;
    attr.value.objlist.list = &tam;

    EXPECT_EQ(SAI_STATUS_FAILURE, setSwitch(attr));

    EXPECT_EQ(1u, drop_monitoring_calls.size());
}

/*
 * drop_monitor_limit reaches the plugin as the VPP policer of the policer at
 * the end of the TAM chain, before drop monitoring is armed, and is unbound
 * on disarm.
 */
TEST_F(SwitchVppSflowTest, tamObjectIdBindsDropPolicer)
{
    sai_object_id_t policer;
    sai_object_id_t tam = createDropMonitorChain(500, policer);

    EXPECT_EQ(policer, m_sw->sflowDropMonitorPolicer(tam));

    sai_attribute_t attr;

    attr.id = SAI_SWITCH_ATTR_TAM_OBJECT_ID;
    attr.value.objlist.count = 1;
    attr.value.objlist.list = &tam;

    ASSERT_EQ(SAI_STATUS_SUCCESS, setSwitch(attr));

    ASSERT_EQ(1u, drop_policer_calls.size());
    EXPECT_EQ(vppPolicerName(policer), drop_policer_calls[0].name);
    EXPECT_EQ(0u, drop_policer_calls[0].drop_monitoring_calls_before);
    EXPECT_EQ(policer, m_sw->m_sflow_drop_policer);

    attr.value.objlist.count = 0;

    ASSERT_EQ(SAI_STATUS_SUCCESS, setSwitch(attr));

    ASSERT_EQ(2u, drop_policer_calls.size());
    EXPECT_EQ("", drop_policer_calls[1].name);
    EXPECT_EQ(2u, drop_policer_calls[1].drop_monitoring_calls_before);
    EXPECT_EQ((sai_object_id_t)SAI_NULL_OBJECT_ID, m_sw->m_sflow_drop_policer);
    EXPECT_EQ((sai_object_id_t)SAI_NULL_OBJECT_ID, m_sw->m_sflow_drop_tam);
}

/*
 * Re-pointing a link of the bound chain at another policer binds its VPP
 * policer instead; a set that leaves the same policer at the end does not,
 * and nothing is re-bound once drop monitoring is off.
 */
TEST_F(SwitchVppSflowTest, dropMonitorChainSetFollowsPolicer)
{
    sai_object_id_t policer;
    sai_object_id_t group;
    sai_object_id_t tam = createDropMonitorChain(500, policer, &group);

    sai_attribute_t bind;
    bind.id = SAI_SWITCH_ATTR_TAM_OBJECT_ID;
    bind.value.objlist.count = 1;
    bind.value.objlist.list = &tam;
    ASSERT_EQ(SAI_STATUS_SUCCESS, setSwitch(bind));
    ASSERT_EQ(1u, drop_policer_calls.size());

    sai_object_id_t other = createPacketPolicer(100);

    sai_attribute_t attr;
    attr.id = SAI_HOSTIF_TRAP_GROUP_ATTR_POLICER;
    attr.value.oid = other;
    ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->set(SAI_OBJECT_TYPE_HOSTIF_TRAP_GROUP,
        sai_serialize_object_id(group), &attr));

    ASSERT_EQ(2u, drop_policer_calls.size());
    EXPECT_EQ(vppPolicerName(other), drop_policer_calls[1].name);
    EXPECT_EQ(other, m_sw->m_sflow_drop_policer);

    // same policer again: nothing to re-bind
    ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->set(SAI_OBJECT_TYPE_HOSTIF_TRAP_GROUP,
        sai_serialize_object_id(group), &attr));
    EXPECT_EQ(2u, drop_policer_calls.size());

    // unlinking the policer lifts the limit
    attr.value.oid = SAI_NULL_OBJECT_ID;
    ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->set(SAI_OBJECT_TYPE_HOSTIF_TRAP_GROUP,
        sai_serialize_object_id(group), &attr));
    ASSERT_EQ(3u, drop_policer_calls.size());
    EXPECT_EQ("", drop_policer_calls[2].name);

    bind.value.objlist.count = 0;
    ASSERT_EQ(SAI_STATUS_SUCCESS, setSwitch(bind));
    ASSERT_EQ(4u, drop_policer_calls.size());

    attr.value.oid = policer;
    ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->set(SAI_OBJECT_TYPE_HOSTIF_TRAP_GROUP,
        sai_serialize_object_id(group), &attr));
    EXPECT_EQ(4u, drop_policer_calls.size());
}

/*
 * A TAM object without the chain behind it, or a policer VPP has no policer
 * for (createPolicer() failed), leaves drop notifications unlimited.
 */
TEST_F(SwitchVppSflowTest, dropPolicerWithoutVppPolicerIsUnlimited)
{
    sai_object_id_t tam = m_mgr->allocateNewObjectId(SAI_OBJECT_TYPE_TAM, m_switchId);

    EXPECT_EQ((sai_object_id_t)SAI_NULL_OBJECT_ID, m_sw->sflowDropMonitorPolicer(tam));

    sai_object_id_t policer;
    createDropMonitorChain(500, policer);
    m_sw->m_policer_map.erase(policer);

    m_sw->m_sflow_drop_policer = policer;
    ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->sflowDropPolicerApply());

    ASSERT_EQ(1u, drop_policer_calls.size());
    EXPECT_EQ("", drop_policer_calls[0].name);
}
