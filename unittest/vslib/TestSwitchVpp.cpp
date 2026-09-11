#include "vpp/SwitchVpp.h"

#include <gtest/gtest.h>

using namespace saivs;

TEST(SwitchVpp, getLagMemberEgressDisableAction)
{
    using Action = SwitchVpp::LagMemberEgressDisableAction;

    EXPECT_EQ(Action::NONE, SwitchVpp::getLagMemberEgressDisableAction(false, true, false));
    EXPECT_EQ(Action::DISABLE, SwitchVpp::getLagMemberEgressDisableAction(true, true, false));
    EXPECT_EQ(Action::ENABLE, SwitchVpp::getLagMemberEgressDisableAction(false, true, true));
    EXPECT_EQ(Action::NONE, SwitchVpp::getLagMemberEgressDisableAction(true, true, true));
    EXPECT_EQ(Action::NONE, SwitchVpp::getLagMemberEgressDisableAction(false, false, false));
    EXPECT_EQ(Action::DISABLE, SwitchVpp::getLagMemberEgressDisableAction(true, false, false));
}

TEST(SwitchVpp, getTrimEnumValuesCapability)
{
    sai_int32_t value = -1;

    // The DSCP resolution mode advertises only DSCP_VALUE (symmetric trim); the
    // asymmetric FROM_TC mode is intentionally not offered on VPP.
    ASSERT_TRUE(SwitchVpp::getTrimEnumValuesCapability(
        SAI_OBJECT_TYPE_SWITCH, SAI_SWITCH_ATTR_PACKET_TRIM_DSCP_RESOLUTION_MODE, &value));
    EXPECT_EQ((sai_int32_t)SAI_PACKET_TRIM_DSCP_RESOLUTION_MODE_DSCP_VALUE, value);

    // The queue resolution mode advertises only STATIC.
    value = -1;
    ASSERT_TRUE(SwitchVpp::getTrimEnumValuesCapability(
        SAI_OBJECT_TYPE_SWITCH, SAI_SWITCH_ATTR_PACKET_TRIM_QUEUE_RESOLUTION_MODE, &value));
    EXPECT_EQ((sai_int32_t)SAI_PACKET_TRIM_QUEUE_RESOLUTION_MODE_STATIC, value);

    // Unrelated switch enum attributes fall through to the base metadata.
    EXPECT_FALSE(SwitchVpp::getTrimEnumValuesCapability(
        SAI_OBJECT_TYPE_SWITCH, SAI_SWITCH_ATTR_ECMP_HASH, &value));

    // A trim switch attr queried against the wrong object type must not match.
    EXPECT_FALSE(SwitchVpp::getTrimEnumValuesCapability(
        SAI_OBJECT_TYPE_BUFFER_PROFILE, SAI_SWITCH_ATTR_PACKET_TRIM_DSCP_RESOLUTION_MODE, &value));

    // A null out-pointer is tolerated (predicate still resolves).
    EXPECT_TRUE(SwitchVpp::getTrimEnumValuesCapability(
        SAI_OBJECT_TYPE_SWITCH, SAI_SWITCH_ATTR_PACKET_TRIM_QUEUE_RESOLUTION_MODE, nullptr));
}

TEST(SwitchVpp, isTrimDataplaneAttr)
{
    // Queue buffer-profile / scheduler bindings drive per-queue admission.
    EXPECT_TRUE(SwitchVpp::isTrimDataplaneAttr(
        SAI_OBJECT_TYPE_QUEUE, SAI_QUEUE_ATTR_BUFFER_PROFILE_ID));
    EXPECT_TRUE(SwitchVpp::isTrimDataplaneAttr(
        SAI_OBJECT_TYPE_QUEUE, SAI_QUEUE_ATTR_SCHEDULER_PROFILE_ID));
    EXPECT_TRUE(SwitchVpp::isTrimDataplaneAttr(
        SAI_OBJECT_TYPE_BUFFER_PROFILE, SAI_BUFFER_PROFILE_ATTR_PACKET_ADMISSION_FAIL_ACTION));
    EXPECT_TRUE(SwitchVpp::isTrimDataplaneAttr(
        SAI_OBJECT_TYPE_SCHEDULER_GROUP, SAI_SCHEDULER_GROUP_ATTR_SCHEDULER_PROFILE_ID));
    EXPECT_TRUE(SwitchVpp::isTrimDataplaneAttr(
        SAI_OBJECT_TYPE_SCHEDULER, SAI_SCHEDULER_ATTR_MAX_BANDWIDTH_RATE));

    // Port QoS-map (re)bindings and in-place QoS-map edits re-resolve the
    // switch-global DSCP->queue trim table.
    EXPECT_TRUE(SwitchVpp::isTrimDataplaneAttr(
        SAI_OBJECT_TYPE_PORT, SAI_PORT_ATTR_QOS_DSCP_TO_TC_MAP));
    EXPECT_TRUE(SwitchVpp::isTrimDataplaneAttr(
        SAI_OBJECT_TYPE_PORT, SAI_PORT_ATTR_QOS_TC_TO_QUEUE_MAP));
    EXPECT_TRUE(SwitchVpp::isTrimDataplaneAttr(
        SAI_OBJECT_TYPE_QOS_MAP, SAI_QOS_MAP_ATTR_MAP_TO_VALUE_LIST));

    // Unrelated attributes must not trigger a trim refresh.
    EXPECT_FALSE(SwitchVpp::isTrimDataplaneAttr(
        SAI_OBJECT_TYPE_QUEUE, SAI_QUEUE_ATTR_TYPE));
    EXPECT_FALSE(SwitchVpp::isTrimDataplaneAttr(
        SAI_OBJECT_TYPE_PORT, SAI_PORT_ATTR_MTU));
    EXPECT_FALSE(SwitchVpp::isTrimDataplaneAttr(
        SAI_OBJECT_TYPE_SWITCH, SAI_SWITCH_ATTR_PACKET_TRIM_SIZE));
}
