#include "SwitchPoE.h"

#include "meta/sai_serialize.h"

#include <gtest/gtest.h>

#include <memory>

using namespace saivs;

TEST(SwitchPoE, ctr)
{
    auto sc = std::make_shared<SwitchConfig>(0, "");
    auto signal = std::make_shared<Signal>();
    auto eventQueue = std::make_shared<EventQueue>(signal);

    sc->m_saiSwitchType = SAI_SWITCH_TYPE_POE;
    sc->m_switchType = SAI_VS_SWITCH_TYPE_POE;
    sc->m_bootType = SAI_VS_BOOT_TYPE_COLD;
    sc->m_useTapDevice = false;
    sc->m_laneMap = LaneMap::getDefaultLaneMap(0);
    sc->m_eventQueue = eventQueue;

    auto scc = std::make_shared<SwitchConfigContainer>();

    scc->insert(sc);

    SwitchPoE sw(
            0x2100000000,
            std::make_shared<RealObjectIdManager>(0, scc),
            sc);

    SwitchPoE sw2(
            0x2100000000,
            std::make_shared<RealObjectIdManager>(0, scc),
            sc,
            nullptr);

    sai_attribute_t attr;

    attr.id = SAI_SWITCH_ATTR_INIT_SWITCH;
    attr.value.booldata = true;

    EXPECT_EQ(sw.initialize_default_objects(1, &attr), SAI_STATUS_SUCCESS);
}

TEST(SwitchPoE, refresh_read_only)
{
    auto sc = std::make_shared<SwitchConfig>(0, "");
    auto signal = std::make_shared<Signal>();
    auto eventQueue = std::make_shared<EventQueue>(signal);

    sc->m_saiSwitchType = SAI_SWITCH_TYPE_POE;
    sc->m_switchType = SAI_VS_SWITCH_TYPE_POE;
    sc->m_bootType = SAI_VS_BOOT_TYPE_COLD;
    sc->m_useTapDevice = false;
    sc->m_laneMap = LaneMap::getDefaultLaneMap(0);
    sc->m_eventQueue = eventQueue;

    auto scc = std::make_shared<SwitchConfigContainer>();

    scc->insert(sc);

    auto mgr = std::make_shared<RealObjectIdManager>(0, scc);

    auto switchId = mgr->allocateNewSwitchObjectId("");
    auto strSwitchId = sai_serialize_object_id(switchId);

    SwitchPoE sw(switchId, mgr, sc);

    const uint32_t oidListLength = 5;
    sai_object_id_t oidList[oidListLength];
    sai_attribute_t attrs[2];

    attrs[0].id = SAI_SWITCH_ATTR_INIT_SWITCH;
    attrs[0].value.booldata = true;
    EXPECT_EQ(sw.initialize_default_objects(1, attrs), SAI_STATUS_SUCCESS);

    attrs[0].id = SAI_SWITCH_ATTR_DEFAULT_1Q_BRIDGE_ID;
    attrs[0].value.oid = SAI_NULL_OBJECT_ID;
    EXPECT_EQ(sw.get(SAI_OBJECT_TYPE_SWITCH, strSwitchId, 1, attrs), SAI_STATUS_NOT_IMPLEMENTED);

    attrs[0].id = SAI_SWITCH_ATTR_POE_DEVICE_LIST;
    attrs[0].value.objlist.count = oidListLength;
    attrs[0].value.objlist.list = oidList;
    EXPECT_EQ(sw.get(SAI_OBJECT_TYPE_SWITCH, strSwitchId, 1, attrs), SAI_STATUS_SUCCESS);
    EXPECT_EQ(attrs[0].value.objlist.count, 0);

    // create POE device, PSE, and 2 POE ports

    // device
    sai_object_id_t devId = mgr->allocateNewObjectId(SAI_OBJECT_TYPE_POE_DEVICE, switchId);
    auto strDevId = sai_serialize_object_id(devId);
    attrs[0].id = SAI_POE_DEVICE_ATTR_HARDWARE_INFO;
    strncpy(attrs[0].value.chardata, "hw info", sizeof(attrs[0].value.chardata) - 1);
    EXPECT_EQ(sw.create(SAI_OBJECT_TYPE_POE_DEVICE, strDevId, switchId, 1, attrs), SAI_STATUS_SUCCESS);

    // PSE
    sai_object_id_t pseId = mgr->allocateNewObjectId(SAI_OBJECT_TYPE_POE_PSE, switchId);
    auto strPseId = sai_serialize_object_id(pseId);
    attrs[0].id = SAI_POE_PSE_ATTR_ID;
    attrs[0].value.u32 = 1;
    attrs[1].id = SAI_POE_PSE_ATTR_DEVICE_ID;
    attrs[1].value.oid = devId;
    EXPECT_EQ(sw.create(SAI_OBJECT_TYPE_POE_PSE, strPseId, switchId, 2, attrs), SAI_STATUS_SUCCESS);

    // port 1
    sai_object_id_t firstPortId = mgr->allocateNewObjectId(SAI_OBJECT_TYPE_POE_PORT, switchId);
    auto strFirstPortId = sai_serialize_object_id(firstPortId);
    attrs[0].id = SAI_POE_PORT_ATTR_FRONT_PANEL_ID;
    attrs[0].value.u32 = 1;
    attrs[1].id = SAI_POE_PORT_ATTR_DEVICE_ID;
    attrs[1].value.oid = devId;
    EXPECT_EQ(sw.create(SAI_OBJECT_TYPE_POE_PORT, strFirstPortId, switchId, 2, attrs), SAI_STATUS_SUCCESS);

    // port 2
    sai_object_id_t secondPortId = mgr->allocateNewObjectId(SAI_OBJECT_TYPE_POE_PORT, switchId);
    auto strSecondPortId = sai_serialize_object_id(secondPortId);
    attrs[0].id = SAI_POE_PORT_ATTR_FRONT_PANEL_ID;
    attrs[0].value.u32 = 2;
    EXPECT_EQ(sw.create(SAI_OBJECT_TYPE_POE_PORT, strSecondPortId, switchId, 2, attrs), SAI_STATUS_SUCCESS);

    // verify number of created objects
    attrs[0].id = SAI_SWITCH_ATTR_POE_DEVICE_LIST;
    attrs[0].value.objlist.count = oidListLength;
    attrs[0].value.objlist.list = oidList;
    EXPECT_EQ(sw.get(SAI_OBJECT_TYPE_SWITCH, strSwitchId, 1, attrs), SAI_STATUS_SUCCESS);
    EXPECT_EQ(attrs[0].value.objlist.count, 1);

    attrs[0].id = SAI_POE_DEVICE_ATTR_POE_PSE_LIST;
    attrs[0].value.objlist.count = oidListLength;
    EXPECT_EQ(sw.get(SAI_OBJECT_TYPE_POE_DEVICE, strDevId, 1, attrs), SAI_STATUS_SUCCESS);
    EXPECT_EQ(attrs[0].value.objlist.count, 1);

    attrs[0].id = SAI_POE_DEVICE_ATTR_POE_PORT_LIST;
    attrs[0].value.objlist.count = oidListLength;
    EXPECT_EQ(sw.get(SAI_OBJECT_TYPE_POE_DEVICE, strDevId, 1, attrs), SAI_STATUS_SUCCESS);
    EXPECT_EQ(attrs[0].value.objlist.count, 2);

    // verify readonly values
    attrs[0].id = SAI_POE_DEVICE_ATTR_HARDWARE_INFO;
    EXPECT_EQ(sw.get(SAI_OBJECT_TYPE_POE_DEVICE, strDevId, 1, attrs), SAI_STATUS_SUCCESS);
    attrs[0].id = SAI_POE_DEVICE_ATTR_TOTAL_POWER;
    EXPECT_EQ(sw.get(SAI_OBJECT_TYPE_POE_DEVICE, strDevId, 1, attrs), SAI_STATUS_SUCCESS);
    attrs[0].id = SAI_POE_DEVICE_ATTR_POWER_CONSUMPTION;
    EXPECT_EQ(sw.get(SAI_OBJECT_TYPE_POE_DEVICE, strDevId, 1, attrs), SAI_STATUS_SUCCESS);
    attrs[0].id = SAI_POE_DEVICE_ATTR_VERSION;
    EXPECT_EQ(sw.get(SAI_OBJECT_TYPE_POE_DEVICE, strDevId, 1, attrs), SAI_STATUS_SUCCESS);

    attrs[0].id = SAI_POE_PSE_ATTR_TEMPERATURE;
    EXPECT_EQ(sw.get(SAI_OBJECT_TYPE_POE_PSE, strPseId, 1, attrs), SAI_STATUS_SUCCESS);
    attrs[0].id = SAI_POE_PSE_ATTR_STATUS;
    EXPECT_EQ(sw.get(SAI_OBJECT_TYPE_POE_PSE, strPseId, 1, attrs), SAI_STATUS_SUCCESS);
    attrs[0].id = SAI_POE_PSE_ATTR_SOFTWARE_VERSION;
    EXPECT_EQ(sw.get(SAI_OBJECT_TYPE_POE_PSE, strPseId, 1, attrs), SAI_STATUS_SUCCESS);
    attrs[0].id = SAI_POE_PSE_ATTR_HARDWARE_VERSION;
    EXPECT_EQ(sw.get(SAI_OBJECT_TYPE_POE_PSE, strPseId, 1, attrs), SAI_STATUS_SUCCESS);
    attrs[0].id = SAI_POE_PSE_ATTR_ID;
    EXPECT_EQ(sw.get(SAI_OBJECT_TYPE_POE_PSE, strPseId, 1, attrs), SAI_STATUS_SUCCESS);
    attrs[0].id = SAI_POE_PSE_ATTR_DEVICE_ID;
    EXPECT_EQ(sw.get(SAI_OBJECT_TYPE_POE_PSE, strPseId, 1, attrs), SAI_STATUS_SUCCESS);

    attrs[0].id = SAI_POE_PORT_ATTR_STANDARD;
    EXPECT_EQ(sw.get(SAI_OBJECT_TYPE_POE_PORT, strFirstPortId, 1, attrs), SAI_STATUS_SUCCESS);
    attrs[0].id = SAI_POE_PORT_ATTR_CONSUMPTION;
    EXPECT_EQ(sw.get(SAI_OBJECT_TYPE_POE_PORT, strFirstPortId, 1, attrs), SAI_STATUS_SUCCESS);
    attrs[0].id = SAI_POE_PORT_ATTR_STATUS;
    EXPECT_EQ(sw.get(SAI_OBJECT_TYPE_POE_PORT, strFirstPortId, 1, attrs), SAI_STATUS_SUCCESS);
    attrs[0].id = SAI_POE_PORT_ATTR_FRONT_PANEL_ID;
    EXPECT_EQ(sw.get(SAI_OBJECT_TYPE_POE_PORT, strFirstPortId, 1, attrs), SAI_STATUS_SUCCESS);
    attrs[0].id = SAI_POE_PORT_ATTR_DEVICE_ID;
    EXPECT_EQ(sw.get(SAI_OBJECT_TYPE_POE_PORT, strFirstPortId, 1, attrs), SAI_STATUS_SUCCESS);
}
