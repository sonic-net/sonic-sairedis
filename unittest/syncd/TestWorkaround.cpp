#include <cstdint>

#include <memory>
#include <vector>
#include <array>

#include <gtest/gtest.h>
#include "Workaround.h"
#include "swss/logger.h"

#include <arpa/inet.h>

using namespace syncd;

TEST(Workaround, isSetAttributeWorkaround)
{
    ASSERT_EQ(Workaround::isSetAttributeWorkaround(SAI_OBJECT_TYPE_HOSTIF, SAI_HOSTIF_ATTR_QUEUE, SAI_STATUS_FAILURE), true);
    ASSERT_EQ(Workaround::isSetAttributeWorkaround(SAI_OBJECT_TYPE_SWITCH, SAI_SWITCH_ATTR_SRC_MAC_ADDRESS, SAI_STATUS_FAILURE), true);
    ASSERT_EQ(Workaround::isSetAttributeWorkaround(SAI_OBJECT_TYPE_PORT, SAI_PORT_ATTR_TYPE, SAI_STATUS_FAILURE), false);
    ASSERT_EQ(Workaround::isSetAttributeWorkaround(SAI_OBJECT_TYPE_PORT, SAI_PORT_ATTR_TYPE, SAI_STATUS_SUCCESS), false);
    ASSERT_EQ(Workaround::isSetAttributeWorkaround(SAI_OBJECT_TYPE_SWITCH, SAI_SWITCH_ATTR_VXLAN_DEFAULT_ROUTER_MAC, SAI_STATUS_FAILURE), true);
}
