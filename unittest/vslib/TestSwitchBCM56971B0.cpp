#include <gtest/gtest.h>

#include "SwitchBCM56971B0.h"

using namespace saivs;

TEST(SwitchBCM56971B0, test_queue_number_get)
{
    auto sc = std::make_shared<SwitchConfig>(0, "");
    auto signal = std::make_shared<Signal>();
    auto eventQueue = std::make_shared<EventQueue>(signal);

    sc->m_saiSwitchType = SAI_SWITCH_TYPE_NPU;
    sc->m_switchType = SAI_VS_SWITCH_TYPE_BCM56971B0;
    sc->m_bootType = SAI_VS_BOOT_TYPE_COLD;
    sc->m_useTapDevice = false;
    sc->m_laneMap = LaneMap::getDefaultLaneMap(0);
    sc->m_eventQueue = eventQueue;

    auto scc = std::make_shared<SwitchConfigContainer>();

    scc->insert(sc);

    SwitchBCM56971B0 sw(
            0x2100000000,
            std::make_shared<RealObjectIdManager>(0, scc),
            sc);

    // Initialize switch state
    ASSERT_EQ(sw.initialize_default_objects(0, nullptr), SAI_STATUS_SUCCESS);

    const sai_uint32_t uqNum = 10;
    const sai_uint32_t mqNum = 10;
    const sai_uint32_t qNum = uqNum + mqNum;

    sai_attribute_t attr;

    // Verify unicast queue number
    attr.id = SAI_SWITCH_ATTR_NUMBER_OF_UNICAST_QUEUES;
    ASSERT_EQ(sw.get(SAI_OBJECT_TYPE_SWITCH, 0x2100000000, 1, &attr), SAI_STATUS_SUCCESS);
    ASSERT_EQ(attr.value.u32, uqNum);

    // Verify multicast queue number
    attr.id = SAI_SWITCH_ATTR_NUMBER_OF_MULTICAST_QUEUES;
    ASSERT_EQ(sw.get(SAI_OBJECT_TYPE_SWITCH, 0x2100000000, 1, &attr), SAI_STATUS_SUCCESS);
    ASSERT_EQ(attr.value.u32, mqNum);

    // Verify total queue number
    attr.id = SAI_SWITCH_ATTR_NUMBER_OF_QUEUES;
    ASSERT_EQ(sw.get(SAI_OBJECT_TYPE_SWITCH, 0x2100000000, 1, &attr), SAI_STATUS_SUCCESS);
    ASSERT_EQ(attr.value.u32, qNum);
}
