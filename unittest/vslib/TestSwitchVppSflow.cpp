#include "SwitchVppTestHarness.h"

#include "vpp/SwitchVpp.h"
#include "meta/sai_serialize.h"
#include "saimetadatautils.h"

#include <gtest/gtest.h>

#include <deque>
#include <vector>

using namespace saivs;
using namespace saivs_test;

namespace
{
    // One vpp_sflow_drop_monitoring_set() call, holding the flag it was given.
    std::vector<bool> drop_monitoring_calls;

    // Return codes handed to successive calls; empty means success.
    std::deque<int> drop_monitoring_results;

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
