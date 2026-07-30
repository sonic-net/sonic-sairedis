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

TEST(SwitchVpp, isPacketTrimSwitchAttr)
{
    // All six switch-level packet-trim attributes must be classified as trim
    // attrs so queryAttributeCapability() reports them not implemented and
    // sonic-swss publishes SWITCH_TRIMMING_CAPABLE=false.
    EXPECT_TRUE(SwitchVpp::isPacketTrimSwitchAttr(SAI_SWITCH_ATTR_PACKET_TRIM_SIZE));
    EXPECT_TRUE(SwitchVpp::isPacketTrimSwitchAttr(SAI_SWITCH_ATTR_PACKET_TRIM_DSCP_RESOLUTION_MODE));
    EXPECT_TRUE(SwitchVpp::isPacketTrimSwitchAttr(SAI_SWITCH_ATTR_PACKET_TRIM_DSCP_VALUE));
    EXPECT_TRUE(SwitchVpp::isPacketTrimSwitchAttr(SAI_SWITCH_ATTR_PACKET_TRIM_TC_VALUE));
    EXPECT_TRUE(SwitchVpp::isPacketTrimSwitchAttr(SAI_SWITCH_ATTR_PACKET_TRIM_QUEUE_RESOLUTION_MODE));
    EXPECT_TRUE(SwitchVpp::isPacketTrimSwitchAttr(SAI_SWITCH_ATTR_PACKET_TRIM_QUEUE_INDEX));

    // Unrelated switch attributes must not be gated off.
    EXPECT_FALSE(SwitchVpp::isPacketTrimSwitchAttr(SAI_SWITCH_ATTR_INIT_SWITCH));
    EXPECT_FALSE(SwitchVpp::isPacketTrimSwitchAttr(SAI_SWITCH_ATTR_ECMP_HASH));
}

TEST(SwitchVpp, isUnsupportedTrimEnumCapability)
{
    // The trim-related enum capabilities the base virtual switch advertises
    // must be reported unsupported on VPP.
    EXPECT_TRUE(SwitchVpp::isUnsupportedTrimEnumCapability(
        SAI_OBJECT_TYPE_SWITCH, SAI_SWITCH_ATTR_PACKET_TRIM_DSCP_RESOLUTION_MODE));
    EXPECT_TRUE(SwitchVpp::isUnsupportedTrimEnumCapability(
        SAI_OBJECT_TYPE_SWITCH, SAI_SWITCH_ATTR_PACKET_TRIM_QUEUE_RESOLUTION_MODE));
    EXPECT_TRUE(SwitchVpp::isUnsupportedTrimEnumCapability(
        SAI_OBJECT_TYPE_BUFFER_PROFILE, SAI_BUFFER_PROFILE_ATTR_PACKET_ADMISSION_FAIL_ACTION));

    // Non-trim enum attributes must still be delegated to the base class.
    EXPECT_FALSE(SwitchVpp::isUnsupportedTrimEnumCapability(
        SAI_OBJECT_TYPE_SWITCH, SAI_SWITCH_ATTR_ECMP_HASH));
    EXPECT_FALSE(SwitchVpp::isUnsupportedTrimEnumCapability(
        SAI_OBJECT_TYPE_TAM, SAI_TAM_ATTR_TAM_BIND_POINT_TYPE_LIST));

    // A trim switch attr queried against the wrong object type must not match.
    EXPECT_FALSE(SwitchVpp::isUnsupportedTrimEnumCapability(
        SAI_OBJECT_TYPE_BUFFER_PROFILE, SAI_SWITCH_ATTR_PACKET_TRIM_DSCP_RESOLUTION_MODE));
}
