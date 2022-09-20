#include "SwitchStateBase.h"
#include "MACsecAttr.h"

#include <gtest/gtest.h>

#include <vector>

using namespace saivs;

//Test the following function:
//sai_status_t initialize_voq_switch_objects(
//             _In_ uint32_t attr_count,
//             _In_ const sai_attribute_t *attr_list);

TEST(SwitchStateBase, initialize_voq_switch_objects)
{
    auto sc = std::make_shared<SwitchConfig>(0, "");
    auto scc = std::make_shared<SwitchConfigContainer>();

    SwitchStateBase ss(
            0x2100000000,
            std::make_shared<RealObjectIdManager>(0, scc),
            sc);

    sai_attribute_t attr;

    attr.id = SAI_SWITCH_ATTR_TYPE;
    attr.value.u32 = SAI_SWITCH_TYPE_FABRIC;
    sc->m_fabricLaneMap = LaneMap::getDefaultLaneMap(0);
    // Check the result of the initialize_voq_switch_objects
    EXPECT_EQ(SAI_STATUS_SUCCESS,
              ss.initialize_voq_switch_objects(1, &attr));
}

