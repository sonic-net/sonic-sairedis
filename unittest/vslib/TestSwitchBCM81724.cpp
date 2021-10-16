#include "SwitchBCM81724.h"

#include "meta/sai_serialize.h"

#include <gtest/gtest.h>

#include <memory>

using namespace saivs;

TEST(SwitchBCM81724, ctr)
{
    auto sc = std::make_shared<SwitchConfig>(0, "");
    auto signal = std::make_shared<Signal>();
    auto eventQueue = std::make_shared<EventQueue>(signal);

    sc->m_saiSwitchType = SAI_SWITCH_TYPE_NPU;
    sc->m_switchType = SAI_VS_SWITCH_TYPE_BCM56850;
    sc->m_bootType = SAI_VS_BOOT_TYPE_COLD;
    sc->m_useTapDevice = false;
    sc->m_laneMap = LaneMap::getDefaultLaneMap(0);
    sc->m_eventQueue = eventQueue;

    auto scc = std::make_shared<SwitchConfigContainer>();

    scc->insert(sc);

    SwitchBCM81724 sw(
            0x2100000000,
            std::make_shared<RealObjectIdManager>(0, scc),
            sc);

    SwitchBCM81724 sw2(
            0x2100000000,
            std::make_shared<RealObjectIdManager>(0, scc),
            sc,
            nullptr);

    sai_attribute_t attr;

    attr.id = SAI_SWITCH_ATTR_INIT_SWITCH;
    attr.value.booldata = true;

    EXPECT_EQ(sw.initialize_default_objects(1, &attr), SAI_STATUS_SUCCESS);
}

TEST(SwitchBCM81724, refresh_read_only)
{
    auto sc = std::make_shared<SwitchConfig>(0, "");
    auto signal = std::make_shared<Signal>();
    auto eventQueue = std::make_shared<EventQueue>(signal);

    sc->m_saiSwitchType = SAI_SWITCH_TYPE_NPU;
    sc->m_switchType = SAI_VS_SWITCH_TYPE_BCM56850;
    sc->m_bootType = SAI_VS_BOOT_TYPE_COLD;
    sc->m_useTapDevice = false;
    sc->m_laneMap = LaneMap::getDefaultLaneMap(0);
    sc->m_eventQueue = eventQueue;

    auto scc = std::make_shared<SwitchConfigContainer>();

    scc->insert(sc);

    auto mgr = std::make_shared<RealObjectIdManager>(0, scc);

    auto switchId = mgr->allocateNewSwitchObjectId("");

    SwitchBCM81724 sw(
            switchId,
            mgr,
            sc);

    sai_attribute_t attr;

    attr.id = SAI_SWITCH_ATTR_INIT_SWITCH;
    attr.value.booldata = true;

    EXPECT_EQ(sw.initialize_default_objects(1, &attr), SAI_STATUS_SUCCESS);

    attr.id = SAI_SWITCH_ATTR_DEFAULT_1Q_BRIDGE_ID;
    attr.value.oid = SAI_NULL_OBJECT_ID;

    auto strSwitchId = sai_serialize_object_id(switchId);

    EXPECT_EQ(sw.get(SAI_OBJECT_TYPE_SWITCH, strSwitchId, 1, &attr), SAI_STATUS_NOT_IMPLEMENTED);

    // create port, since this switch have zero ports at start

    sai_attribute_t attrs[2];

    sai_uint32_t list[1] = { 1 };

    attrs[0].id = SAI_PORT_ATTR_HW_LANE_LIST;
    attrs[0].value.u32list.count = 1;
    attrs[0].value.u32list.list = list;

    attrs[1].id = SAI_PORT_ATTR_SPEED;
    attrs[1].value.u32 = 10000;

    sai_object_id_t portId = mgr->allocateNewObjectId(SAI_OBJECT_TYPE_PORT, switchId);

    auto strPortId = sai_serialize_object_id(portId);

    EXPECT_EQ(sw.create(SAI_OBJECT_TYPE_PORT, strPortId, switchId, 2, attrs), SAI_STATUS_SUCCESS);

    attr.id = SAI_SWITCH_ATTR_NUMBER_OF_ACTIVE_PORTS;

    EXPECT_EQ(sw.get(SAI_OBJECT_TYPE_SWITCH, strSwitchId, 1, &attr), SAI_STATUS_SUCCESS);

    EXPECT_EQ(attr.value.u32, 1);

    attr.id = SAI_SWITCH_ATTR_DEFAULT_TRAP_GROUP;

    EXPECT_EQ(sw.get(SAI_OBJECT_TYPE_SWITCH, strSwitchId, 1, &attr), SAI_STATUS_SUCCESS);

    attr.id = SAI_PORT_ATTR_OPER_STATUS;

    EXPECT_EQ(sw.get(SAI_OBJECT_TYPE_PORT, strPortId, 1, &attr), SAI_STATUS_SUCCESS);

    //std::cout << sw.dump_switch_database_for_warm_restart();
}

TEST(SwitchBCM81724, warm_boot_initialize_objects)
{
    auto sc = std::make_shared<SwitchConfig>(0, "");
    auto signal = std::make_shared<Signal>();
    auto eventQueue = std::make_shared<EventQueue>(signal);

    sc->m_saiSwitchType = SAI_SWITCH_TYPE_NPU;
    sc->m_switchType = SAI_VS_SWITCH_TYPE_BCM56850;
    sc->m_bootType = SAI_VS_BOOT_TYPE_COLD;
    sc->m_useTapDevice = false;
    sc->m_laneMap = LaneMap::getDefaultLaneMap(0);
    sc->m_eventQueue = eventQueue;

    auto scc = std::make_shared<SwitchConfigContainer>();

    scc->insert(sc);

    SwitchBCM81724 sw(
            0x2100000000,
            std::make_shared<RealObjectIdManager>(0, scc),
            sc);

    EXPECT_EQ(sw.warm_boot_initialize_objects(), SAI_STATUS_NOT_IMPLEMENTED);
}
