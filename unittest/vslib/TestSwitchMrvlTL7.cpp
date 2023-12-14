#include "SwitchMrvlTL7.h"

#include "meta/sai_serialize.h"

#include <gtest/gtest.h>

#include <memory>

using namespace saivs;

TEST(SwitchMrvlTL7, ctr)
{
    auto sc = std::make_shared<SwitchConfig>(0, "");
    auto signal = std::make_shared<Signal>();
    auto eventQueue = std::make_shared<EventQueue>(signal);

    // TODO: Should the switch type be changed when new API is available?
    sc->m_saiSwitchType = SAI_SWITCH_TYPE_NPU;
    sc->m_switchType = SAI_VS_SWITCH_TYPE_MRVLTL7;
    sc->m_bootType = SAI_VS_BOOT_TYPE_COLD;
    sc->m_useTapDevice = false;
    sc->m_laneMap = LaneMap::getDefaultLaneMap(0);
    sc->m_eventQueue = eventQueue;

    auto scc = std::make_shared<SwitchConfigContainer>();

    scc->insert(sc);

    SwitchMrvlTL7 sw(
            0x2100000000,
            std::make_shared<RealObjectIdManager>(0, scc),
            sc);

    SwitchMrvlTL7 sw2(
            0x2100000000,
            std::make_shared<RealObjectIdManager>(0, scc),
            sc,
            nullptr);

    sai_attribute_t attr;

    attr.id = SAI_SWITCH_ATTR_INIT_SWITCH;
    attr.value.booldata = true;

    EXPECT_EQ(sw.initialize_default_objects(1, &attr), SAI_STATUS_SUCCESS);
}
TEST(SwitchMrvlTL7, test_stats_query_capability)
{
    auto sc = std::make_shared<SwitchConfig>(0, "");
    auto signal = std::make_shared<Signal>();
    auto eventQueue = std::make_shared<EventQueue>(signal);

    sc->m_saiSwitchType = SAI_SWITCH_TYPE_NPU;
    sc->m_switchType = SAI_VS_SWITCH_TYPE_MRVLTL7;
    sc->m_bootType = SAI_VS_BOOT_TYPE_COLD;
    sc->m_useTapDevice = false;
    sc->m_laneMap = LaneMap::getDefaultLaneMap(0);
    sc->m_eventQueue = eventQueue;

    auto scc = std::make_shared<SwitchConfigContainer>();

    scc->insert(sc);

    SwitchMrvlTL7 sw(
            0x2100000000,
            std::make_shared<RealObjectIdManager>(0, scc),
            sc);

    sai_stat_capability_t capability_list[51];
    sai_stat_capability_list_t stats_capability;
    stats_capability.count = 38;
    stats_capability.list = capability_list;

    EXPECT_EQ(sw.queryStatsCapability(0x2100000000,
                                          SAI_OBJECT_TYPE_QUEUE,
                                          &stats_capability),
                                          SAI_STATUS_SUCCESS);

    stats_capability.count = 51;

    EXPECT_EQ(sw.queryStatsCapability(0x2100000000,
                                          SAI_OBJECT_TYPE_PORT,
                                          &stats_capability),
                                          SAI_STATUS_SUCCESS);

}

