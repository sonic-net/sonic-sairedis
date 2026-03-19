#include "SwitchNokiaVS.h"

#include "meta/sai_serialize.h"

#include <gtest/gtest.h>

#include <memory>
#include <cstring>
#include <fstream>
#include <sstream>

using namespace saivs;

static std::map<sai_object_id_t, WarmBootState> g_warmBootState;

static bool getWarmBootState(
        _In_ const char* warmBootFile,
        _In_ std::shared_ptr<RealObjectIdManager> roidm)
{
    SWSS_LOG_ENTER();

    std::ifstream ifs;

    ifs.open(warmBootFile);

    if (!ifs.is_open())
    {
        SWSS_LOG_ERROR("failed to open: %s", warmBootFile);

        return false;
    }

    std::string line;

    while (std::getline(ifs, line))
    {
        SWSS_LOG_DEBUG("line: %s", line.c_str());

        std::istringstream iss(line);

        std::string strObjectType;
        std::string strObjectId;
        std::string strAttrId;
        std::string strAttrValue;

        iss >> strObjectType >> strObjectId;

        if (strObjectType == SAI_VS_FDB_INFO)
        {
            FdbInfo fi = FdbInfo::deserialize(strObjectId);

            auto switchId = roidm->switchIdQuery(fi.m_portId);

            if (switchId == SAI_NULL_OBJECT_ID)
            {
                SWSS_LOG_ERROR("switchIdQuery returned NULL on fi.m_port = %s",
                        sai_serialize_object_id(fi.m_portId).c_str());

                g_warmBootState.clear();
                return false;
            }

            g_warmBootState[switchId].m_switchId = switchId;

            g_warmBootState[switchId].m_fdbInfoSet.insert(fi);

            continue;
        }

        iss >> strAttrId >> strAttrValue;

        sai_object_meta_key_t metaKey;
        sai_deserialize_object_meta_key(strObjectType + ":" + strObjectId, metaKey);

        roidm->updateWarmBootObjectIndex(metaKey.objectkey.key.object_id);

        auto switchId = roidm->switchIdQuery(metaKey.objectkey.key.object_id);

        if (switchId == SAI_NULL_OBJECT_ID)
        {
            SWSS_LOG_ERROR("switchIdQuery returned NULL on oid = %s",
                    sai_serialize_object_id(metaKey.objectkey.key.object_id).c_str());

            g_warmBootState.clear();
            return false;
        }

        g_warmBootState[switchId].m_switchId = switchId;

        auto &objectHash = g_warmBootState[switchId].m_objectHash[metaKey.objecttype];

        if (objectHash.find(strObjectId) == objectHash.end())
        {
            objectHash[strObjectId] = {};
        }

        if (strAttrId == "NULL")
        {
            continue;
        }

        objectHash[strObjectId][strAttrId] =
            std::make_shared<SaiAttrWrap>(strAttrId, strAttrValue);
    }

    ifs.close();

    return true;
}

static std::shared_ptr<SwitchNokiaVS> createNokiaSwitch(bool useTapDevice = false)
{
    SWSS_LOG_ENTER();

    auto sc = std::make_shared<SwitchConfig>(0, "");
    auto signal = std::make_shared<Signal>();
    auto eventQueue = std::make_shared<EventQueue>(signal);

    sc->m_saiSwitchType = SAI_SWITCH_TYPE_NPU;
    sc->m_switchType = SAI_VS_SWITCH_TYPE_NOKIA_VS;
    sc->m_bootType = SAI_VS_BOOT_TYPE_COLD;
    sc->m_useTapDevice = useTapDevice;
    sc->m_laneMap = LaneMap::getDefaultLaneMap(0);
    sc->m_eventQueue = eventQueue;

    auto scc = std::make_shared<SwitchConfigContainer>();
    scc->insert(sc);

    auto sw = std::make_shared<SwitchNokiaVS>(
            0x2100000000,
            std::make_shared<RealObjectIdManager>(0, scc),
            sc);

    sai_attribute_t attr;
    attr.id = SAI_SWITCH_ATTR_INIT_SWITCH;
    attr.value.booldata = true;

    if (sw->initialize_default_objects(1, &attr) != SAI_STATUS_SUCCESS)
        return nullptr;

    return sw;
}

TEST(SwitchNokiaVS, ctr)
{
    auto sc = std::make_shared<SwitchConfig>(0, "");
    auto signal = std::make_shared<Signal>();
    auto eventQueue = std::make_shared<EventQueue>(signal);

    sc->m_saiSwitchType = SAI_SWITCH_TYPE_NPU;
    sc->m_switchType = SAI_VS_SWITCH_TYPE_NOKIA_VS;
    sc->m_bootType = SAI_VS_BOOT_TYPE_COLD;
    sc->m_useTapDevice = false;
    sc->m_laneMap = LaneMap::getDefaultLaneMap(0);
    sc->m_eventQueue = eventQueue;

    auto scc = std::make_shared<SwitchConfigContainer>();

    scc->insert(sc);

    SwitchNokiaVS sw(
            0x2100000000,
            std::make_shared<RealObjectIdManager>(0, scc),
            sc);

    SwitchNokiaVS sw2(
            0x2100000000,
            std::make_shared<RealObjectIdManager>(0, scc),
            sc,
            nullptr);

    sai_attribute_t attr;

    attr.id = SAI_SWITCH_ATTR_INIT_SWITCH;
    attr.value.booldata = true;

    EXPECT_EQ(sw.initialize_default_objects(1, &attr), SAI_STATUS_SUCCESS);
}

TEST(SwitchNokiaVS, switchQueueNumberGet)
{
    auto sc = std::make_shared<SwitchConfig>(0, "");
    auto signal = std::make_shared<Signal>();
    auto eventQueue = std::make_shared<EventQueue>(signal);

    sc->m_saiSwitchType = SAI_SWITCH_TYPE_NPU;
    sc->m_switchType = SAI_VS_SWITCH_TYPE_NOKIA_VS;
    sc->m_bootType = SAI_VS_BOOT_TYPE_COLD;
    sc->m_useTapDevice = false;
    sc->m_laneMap = LaneMap::getDefaultLaneMap(0);
    sc->m_eventQueue = eventQueue;

    auto scc = std::make_shared<SwitchConfigContainer>();

    scc->insert(sc);

    SwitchNokiaVS sw(
            0x2100000000,
            std::make_shared<RealObjectIdManager>(0, scc),
            sc);

    ASSERT_EQ(sw.initialize_default_objects(0, nullptr), SAI_STATUS_SUCCESS);

    const sai_uint32_t uqNum = 10;
    const sai_uint32_t mqNum = 10;
    const sai_uint32_t qNum = uqNum + mqNum;

    sai_attribute_t attr;

    attr.id = SAI_SWITCH_ATTR_NUMBER_OF_UNICAST_QUEUES;
    ASSERT_EQ(sw.get(SAI_OBJECT_TYPE_SWITCH, "oid:0x2100000000", 1, &attr), SAI_STATUS_SUCCESS);
    ASSERT_EQ(attr.value.u32, uqNum);

    attr.id = SAI_SWITCH_ATTR_NUMBER_OF_MULTICAST_QUEUES;
    ASSERT_EQ(sw.get(SAI_OBJECT_TYPE_SWITCH, "oid:0x2100000000", 1, &attr), SAI_STATUS_SUCCESS);
    ASSERT_EQ(attr.value.u32, mqNum);

    attr.id = SAI_SWITCH_ATTR_NUMBER_OF_QUEUES;
    ASSERT_EQ(sw.get(SAI_OBJECT_TYPE_SWITCH, "oid:0x2100000000", 1, &attr), SAI_STATUS_SUCCESS);
    ASSERT_EQ(attr.value.u32, qNum);
}

TEST(SwitchNokiaVS, refresh_bridge_port_list)
{
    auto sc = std::make_shared<SwitchConfig>(0, "");
    auto signal = std::make_shared<Signal>();
    auto eventQueue = std::make_shared<EventQueue>(signal);

    sc->m_saiSwitchType = SAI_SWITCH_TYPE_NPU;
    sc->m_switchType = SAI_VS_SWITCH_TYPE_NOKIA_VS;
    sc->m_bootType = SAI_VS_BOOT_TYPE_COLD;
    sc->m_useTapDevice = false;
    sc->m_laneMap = LaneMap::getDefaultLaneMap(0);
    sc->m_eventQueue = eventQueue;

    auto scc = std::make_shared<SwitchConfigContainer>();

    scc->insert(sc);

    SwitchNokiaVS sw(
            0x2100000000,
            std::make_shared<RealObjectIdManager>(0, scc),
            sc);

    sai_attribute_t attr;

    attr.id = SAI_SWITCH_ATTR_INIT_SWITCH;
    attr.value.booldata = true;

    EXPECT_EQ(sw.initialize_default_objects(1, &attr), SAI_STATUS_SUCCESS);

    attr.id = SAI_SWITCH_ATTR_DEFAULT_1Q_BRIDGE_ID;
    attr.value.oid = SAI_NULL_OBJECT_ID;

    EXPECT_EQ(sw.get(SAI_OBJECT_TYPE_SWITCH, "oid:0x2100000000", 1, &attr), SAI_STATUS_SUCCESS);

    EXPECT_NE(attr.value.oid, SAI_NULL_OBJECT_ID);

    auto boid = attr.value.oid;
    auto sboid = sai_serialize_object_id(boid);

    sai_object_id_t list[128];

    attr.id = SAI_BRIDGE_ATTR_PORT_LIST;
    attr.value.objlist.count = 128;
    attr.value.objlist.list = list;

    EXPECT_EQ(sw.get(SAI_OBJECT_TYPE_BRIDGE, sboid, 1, &attr), SAI_STATUS_SUCCESS);
}

TEST(SwitchNokiaVS, vs_create_hostif_tap_genetlink)
{
    auto sw = createNokiaSwitch(false);

    ASSERT_NE(sw, nullptr);

    sai_attribute_t attrs[1];

    attrs[0].id = SAI_HOSTIF_ATTR_TYPE;
    attrs[0].value.s32 = SAI_HOSTIF_TYPE_GENETLINK;

    EXPECT_EQ(sw->vs_create_hostif_tap_interface(1, attrs), SAI_STATUS_SUCCESS);
}

TEST(SwitchNokiaVS, vs_create_hostif_tap_missing_type)
{
    auto sw = createNokiaSwitch(false);

    ASSERT_NE(sw, nullptr);

    sai_attribute_t attrs[1];
    attrs[0].id = SAI_HOSTIF_ATTR_NAME;
    strncpy(attrs[0].value.chardata, "Ethernet0", sizeof(attrs[0].value.chardata));

    EXPECT_EQ(sw->vs_create_hostif_tap_interface(1, attrs), SAI_STATUS_FAILURE);
}

TEST(SwitchNokiaVS, vs_create_hostif_tap_unsupported_type)
{
    auto sw = createNokiaSwitch(false);

    ASSERT_NE(sw, nullptr);

    sai_attribute_t attrs[1];

    attrs[0].id = SAI_HOSTIF_ATTR_TYPE;
    attrs[0].value.s32 = SAI_HOSTIF_TYPE_FD;

    EXPECT_EQ(sw->vs_create_hostif_tap_interface(1, attrs), SAI_STATUS_FAILURE);
}

TEST(SwitchNokiaVS, vs_create_hostif_tap_netdev_missing_obj_id)
{
    auto sw = createNokiaSwitch(false);

    ASSERT_NE(sw, nullptr);

    sai_attribute_t attrs[1];

    attrs[0].id = SAI_HOSTIF_ATTR_TYPE;
    attrs[0].value.s32 = SAI_HOSTIF_TYPE_NETDEV;

    EXPECT_EQ(sw->vs_create_hostif_tap_interface(1, attrs), SAI_STATUS_FAILURE);
}

TEST(SwitchNokiaVS, vs_create_hostif_tap_netdev_no_interface)
{
    auto sw = createNokiaSwitch(false);

    ASSERT_NE(sw, nullptr);

    sai_attribute_t port_attr;
    sai_object_id_t port_list[64];
    port_attr.id = SAI_SWITCH_ATTR_PORT_LIST;
    port_attr.value.objlist.count = 64;
    port_attr.value.objlist.list = port_list;

    ASSERT_EQ(sw->get(SAI_OBJECT_TYPE_SWITCH, "oid:0x2100000000", 1, &port_attr), SAI_STATUS_SUCCESS);
    ASSERT_GT(port_attr.value.objlist.count, (uint32_t)1);

    sai_object_id_t port_id = port_list[1];

    sai_attribute_t attrs[3];

    attrs[0].id = SAI_HOSTIF_ATTR_TYPE;
    attrs[0].value.s32 = SAI_HOSTIF_TYPE_NETDEV;

    attrs[1].id = SAI_HOSTIF_ATTR_OBJ_ID;
    attrs[1].value.oid = port_id;

    attrs[2].id = SAI_HOSTIF_ATTR_NAME;
    strncpy(attrs[2].value.chardata, "Ethernet0", sizeof(attrs[2].value.chardata));

    EXPECT_EQ(sw->vs_create_hostif_tap_interface(3, attrs), SAI_STATUS_FAILURE);
}

TEST(SwitchNokiaVS, vs_remove_hostif_tap_nonexistent)
{
    auto sw = createNokiaSwitch(false);

    ASSERT_NE(sw, nullptr);

    EXPECT_NE(sw->vs_remove_hostif_tap_interface(0xDEAD), SAI_STATUS_SUCCESS);
}

TEST(SwitchNokiaVS, restoreEthInterfaces_no_ethernet)
{
    auto sw = createNokiaSwitch(false);

    ASSERT_NE(sw, nullptr);

    sw.reset();
}

TEST(SwitchNokiaVS, hostif_create_tap_veth_forwarding)
{
    auto sw = createNokiaSwitch(false);

    ASSERT_NE(sw, nullptr);

    EXPECT_TRUE(sw->hostif_create_tap_veth_forwarding("Ethernet0", -1, 0x1234));
}

TEST(SwitchNokiaVS, warm_update_queues)
{
    auto sc = std::make_shared<SwitchConfig>(0, "");
    auto signal = std::make_shared<Signal>();
    auto eventQueue = std::make_shared<EventQueue>(signal);

    sc->m_saiSwitchType = SAI_SWITCH_TYPE_NPU;
    sc->m_switchType = SAI_VS_SWITCH_TYPE_NOKIA_VS;
    sc->m_bootType = SAI_VS_BOOT_TYPE_COLD;
    sc->m_useTapDevice = false;
    sc->m_laneMap = LaneMap::getDefaultLaneMap(0);
    sc->m_eventQueue = eventQueue;

    auto scc = std::make_shared<SwitchConfigContainer>();

    scc->insert(sc);

    auto roidm = std::make_shared<RealObjectIdManager>(0, scc);

    EXPECT_TRUE(getWarmBootState("files/mlnx2700.warm.bin", roidm));

    auto warmBootState = std::make_shared<WarmBootState>(g_warmBootState.at(0x2100000000));

    SwitchNokiaVS sw(
            0x2100000000,
            roidm,
            sc,
            warmBootState);

    sai_attribute_t attr;

    attr.id = SAI_SWITCH_ATTR_INIT_SWITCH;
    attr.value.booldata = true;

    EXPECT_EQ(sw.warm_boot_initialize_objects(), SAI_STATUS_SUCCESS);

    attr.id = SAI_SWITCH_ATTR_DEFAULT_1Q_BRIDGE_ID;
    attr.value.oid = SAI_NULL_OBJECT_ID;

    EXPECT_EQ(sw.get(SAI_OBJECT_TYPE_SWITCH, "oid:0x2100000000", 1, &attr), SAI_STATUS_SUCCESS);

    EXPECT_NE(attr.value.oid, SAI_NULL_OBJECT_ID);

    auto boid = attr.value.oid;

    auto sboid = sai_serialize_object_id(boid);

    sai_object_id_t list[128];

    attr.id = SAI_BRIDGE_ATTR_PORT_LIST;

    attr.value.objlist.count = 128;
    attr.value.objlist.list = list;

    EXPECT_EQ(sw.get(SAI_OBJECT_TYPE_BRIDGE, sboid, 1, &attr), SAI_STATUS_SUCCESS);
}

TEST(SwitchNokiaVS, setPort_non_autoneg)
{
    auto sw = createNokiaSwitch(false);

    ASSERT_NE(sw, nullptr);

    sai_attribute_t port_attr;
    sai_object_id_t port_list[64];
    port_attr.id = SAI_SWITCH_ATTR_PORT_LIST;
    port_attr.value.objlist.count = 64;
    port_attr.value.objlist.list = port_list;

    ASSERT_EQ(sw->get(SAI_OBJECT_TYPE_SWITCH, "oid:0x2100000000", 1, &port_attr), SAI_STATUS_SUCCESS);
    ASSERT_GT(port_attr.value.objlist.count, (uint32_t)0);

    sai_attribute_t attr;
    attr.id = SAI_PORT_ATTR_ADMIN_STATE;
    attr.value.booldata = true;

    EXPECT_EQ(sw->setPort(port_list[0], &attr), SAI_STATUS_SUCCESS);
}

TEST(SwitchNokiaVS, setPort_autoneg_no_tap)
{
    auto sw = createNokiaSwitch(false);

    ASSERT_NE(sw, nullptr);

    sai_attribute_t port_attr;
    sai_object_id_t port_list[64];
    port_attr.id = SAI_SWITCH_ATTR_PORT_LIST;
    port_attr.value.objlist.count = 64;
    port_attr.value.objlist.list = port_list;

    ASSERT_EQ(sw->get(SAI_OBJECT_TYPE_SWITCH, "oid:0x2100000000", 1, &port_attr), SAI_STATUS_SUCCESS);
    ASSERT_GT(port_attr.value.objlist.count, (uint32_t)0);

    sai_attribute_t attr;
    attr.id = SAI_PORT_ATTR_AUTO_NEG_MODE;
    attr.value.booldata = true;

    EXPECT_EQ(sw->setPort(port_list[0], &attr), SAI_STATUS_SUCCESS);
}

TEST(SwitchNokiaVS, vs_create_hostif_tap_vlan_obj)
{
    auto sw = createNokiaSwitch(false);

    ASSERT_NE(sw, nullptr);

    sai_attribute_t vattr;
    vattr.id = SAI_SWITCH_ATTR_DEFAULT_VLAN_ID;

    ASSERT_EQ(sw->get(SAI_OBJECT_TYPE_SWITCH, "oid:0x2100000000", 1, &vattr), SAI_STATUS_SUCCESS);

    sai_attribute_t attrs[3];

    attrs[0].id = SAI_HOSTIF_ATTR_TYPE;
    attrs[0].value.s32 = SAI_HOSTIF_TYPE_NETDEV;

    attrs[1].id = SAI_HOSTIF_ATTR_OBJ_ID;
    attrs[1].value.oid = vattr.value.oid;

    attrs[2].id = SAI_HOSTIF_ATTR_NAME;
    strncpy(attrs[2].value.chardata, "Vlan1000", sizeof(attrs[2].value.chardata));

    EXPECT_EQ(sw->vs_create_hostif_tap_interface(3, attrs), SAI_STATUS_SUCCESS);
}

TEST(SwitchNokiaVS, vs_create_hostif_tap_non_port_obj)
{
    auto sw = createNokiaSwitch(false);

    ASSERT_NE(sw, nullptr);

    sai_attribute_t attrs[2];

    attrs[0].id = SAI_HOSTIF_ATTR_TYPE;
    attrs[0].value.s32 = SAI_HOSTIF_TYPE_NETDEV;

    attrs[1].id = SAI_HOSTIF_ATTR_OBJ_ID;
    attrs[1].value.oid = 0x2100000000;

    EXPECT_EQ(sw->vs_create_hostif_tap_interface(2, attrs), SAI_STATUS_FAILURE);
}

TEST(SwitchNokiaVS, vs_create_hostif_tap_missing_name)
{
    auto sw = createNokiaSwitch(false);

    ASSERT_NE(sw, nullptr);

    sai_attribute_t port_attr;
    sai_object_id_t port_list[64];
    port_attr.id = SAI_SWITCH_ATTR_PORT_LIST;
    port_attr.value.objlist.count = 64;
    port_attr.value.objlist.list = port_list;

    ASSERT_EQ(sw->get(SAI_OBJECT_TYPE_SWITCH, "oid:0x2100000000", 1, &port_attr), SAI_STATUS_SUCCESS);
    ASSERT_GT(port_attr.value.objlist.count, (uint32_t)1);

    sai_attribute_t attrs[2];

    attrs[0].id = SAI_HOSTIF_ATTR_TYPE;
    attrs[0].value.s32 = SAI_HOSTIF_TYPE_NETDEV;

    attrs[1].id = SAI_HOSTIF_ATTR_OBJ_ID;
    attrs[1].value.oid = port_list[1];

    EXPECT_EQ(sw->vs_create_hostif_tap_interface(2, attrs), SAI_STATUS_FAILURE);
}

TEST(SwitchNokiaVS, vs_create_hostif_tap_long_name)
{
    auto sw = createNokiaSwitch(false);

    ASSERT_NE(sw, nullptr);

    sai_attribute_t port_attr;
    sai_object_id_t port_list[64];
    port_attr.id = SAI_SWITCH_ATTR_PORT_LIST;
    port_attr.value.objlist.count = 64;
    port_attr.value.objlist.list = port_list;

    ASSERT_EQ(sw->get(SAI_OBJECT_TYPE_SWITCH, "oid:0x2100000000", 1, &port_attr), SAI_STATUS_SUCCESS);
    ASSERT_GT(port_attr.value.objlist.count, (uint32_t)1);

    sai_attribute_t attrs[3];

    attrs[0].id = SAI_HOSTIF_ATTR_TYPE;
    attrs[0].value.s32 = SAI_HOSTIF_TYPE_NETDEV;

    attrs[1].id = SAI_HOSTIF_ATTR_OBJ_ID;
    attrs[1].value.oid = port_list[1];

    attrs[2].id = SAI_HOSTIF_ATTR_NAME;
    strncpy(attrs[2].value.chardata, "EthernetXXXXXXXXXXXXXXXX", sizeof(attrs[2].value.chardata));

    EXPECT_EQ(sw->vs_create_hostif_tap_interface(3, attrs), SAI_STATUS_FAILURE);
}

TEST(SwitchNokiaVS, vs_create_hostif_tap_netdev_bad_name)
{
    auto sw = createNokiaSwitch(false);

    ASSERT_NE(sw, nullptr);

    sai_attribute_t port_attr;
    sai_object_id_t port_list[64];
    port_attr.id = SAI_SWITCH_ATTR_PORT_LIST;
    port_attr.value.objlist.count = 64;
    port_attr.value.objlist.list = port_list;

    ASSERT_EQ(sw->get(SAI_OBJECT_TYPE_SWITCH, "oid:0x2100000000", 1, &port_attr), SAI_STATUS_SUCCESS);
    ASSERT_GT(port_attr.value.objlist.count, (uint32_t)1);

    sai_attribute_t attrs[3];

    attrs[0].id = SAI_HOSTIF_ATTR_TYPE;
    attrs[0].value.s32 = SAI_HOSTIF_TYPE_NETDEV;

    attrs[1].id = SAI_HOSTIF_ATTR_OBJ_ID;
    attrs[1].value.oid = port_list[1];

    attrs[2].id = SAI_HOSTIF_ATTR_NAME;
    strncpy(attrs[2].value.chardata, "myport0", sizeof(attrs[2].value.chardata));

    EXPECT_EQ(sw->vs_create_hostif_tap_interface(3, attrs), SAI_STATUS_FAILURE);
}

TEST(SwitchNokiaVS, vs_remove_hostif_tap_genetlink)
{
    auto sw = createNokiaSwitch(false);

    ASSERT_NE(sw, nullptr);

    sai_object_id_t hostif_id;
    sai_attribute_t attrs[3];

    attrs[0].id = SAI_HOSTIF_ATTR_TYPE;
    attrs[0].value.s32 = SAI_HOSTIF_TYPE_GENETLINK;

    attrs[1].id = SAI_HOSTIF_ATTR_NAME;
    strncpy(attrs[1].value.chardata, "psample", sizeof(attrs[1].value.chardata));

    attrs[2].id = SAI_HOSTIF_ATTR_GENETLINK_MCGRP_NAME;
    strncpy(attrs[2].value.chardata, "packets", sizeof(attrs[2].value.chardata));

    ASSERT_EQ(sw->create(SAI_OBJECT_TYPE_HOSTIF, &hostif_id, 0x2100000000, 3, attrs), SAI_STATUS_SUCCESS);

    EXPECT_EQ(sw->vs_remove_hostif_tap_interface(hostif_id), SAI_STATUS_SUCCESS);
}

TEST(SwitchNokiaVS, vs_remove_hostif_tap_vlan)
{
    auto sw = createNokiaSwitch(false);

    ASSERT_NE(sw, nullptr);

    sai_attribute_t vattr;
    vattr.id = SAI_SWITCH_ATTR_DEFAULT_VLAN_ID;

    ASSERT_EQ(sw->get(SAI_OBJECT_TYPE_SWITCH, "oid:0x2100000000", 1, &vattr), SAI_STATUS_SUCCESS);

    sai_object_id_t hostif_id;
    sai_attribute_t attrs[3];

    attrs[0].id = SAI_HOSTIF_ATTR_TYPE;
    attrs[0].value.s32 = SAI_HOSTIF_TYPE_NETDEV;

    attrs[1].id = SAI_HOSTIF_ATTR_OBJ_ID;
    attrs[1].value.oid = vattr.value.oid;

    attrs[2].id = SAI_HOSTIF_ATTR_NAME;
    strncpy(attrs[2].value.chardata, "Vlan1000", sizeof(attrs[2].value.chardata));

    ASSERT_EQ(sw->create(SAI_OBJECT_TYPE_HOSTIF, &hostif_id, 0x2100000000, 3, attrs), SAI_STATUS_SUCCESS);

    EXPECT_EQ(sw->vs_remove_hostif_tap_interface(hostif_id), SAI_STATUS_SUCCESS);
}

TEST(SwitchNokiaVS, renameNetIntf_nonexistent)
{
    auto sw = createNokiaSwitch(false);

    ASSERT_NE(sw, nullptr);

    EXPECT_FALSE(sw->renameNetIntf("nonexistent_src", "nonexistent_dst"));
}

TEST(SwitchNokiaVS, create_qos_queues_per_port_verify)
{
    auto sw = createNokiaSwitch(false);

    ASSERT_NE(sw, nullptr);

    sai_attribute_t port_attr;
    sai_object_id_t port_list[64];
    port_attr.id = SAI_SWITCH_ATTR_PORT_LIST;
    port_attr.value.objlist.count = 64;
    port_attr.value.objlist.list = port_list;

    ASSERT_EQ(sw->get(SAI_OBJECT_TYPE_SWITCH, "oid:0x2100000000", 1, &port_attr), SAI_STATUS_SUCCESS);
    ASSERT_GT(port_attr.value.objlist.count, (uint32_t)0);

    sai_object_id_t port_id = port_list[0];

    sai_attribute_t attr;
    attr.id = SAI_PORT_ATTR_QOS_NUMBER_OF_QUEUES;

    ASSERT_EQ(sw->get(SAI_OBJECT_TYPE_PORT, port_id, 1, &attr), SAI_STATUS_SUCCESS);
    ASSERT_EQ(attr.value.u32, (uint32_t)20);

    sai_object_id_t queue_list[32];
    attr.id = SAI_PORT_ATTR_QOS_QUEUE_LIST;
    attr.value.objlist.count = 32;
    attr.value.objlist.list = queue_list;

    ASSERT_EQ(sw->get(SAI_OBJECT_TYPE_PORT, port_id, 1, &attr), SAI_STATUS_SUCCESS);
    ASSERT_EQ(attr.value.objlist.count, (uint32_t)20);

    sai_attribute_t qattr;

    qattr.id = SAI_QUEUE_ATTR_TYPE;
    ASSERT_EQ(sw->get(SAI_OBJECT_TYPE_QUEUE, queue_list[0], 1, &qattr), SAI_STATUS_SUCCESS);
    EXPECT_EQ(qattr.value.s32, SAI_QUEUE_TYPE_UNICAST);

    qattr.id = SAI_QUEUE_ATTR_INDEX;
    ASSERT_EQ(sw->get(SAI_OBJECT_TYPE_QUEUE, queue_list[0], 1, &qattr), SAI_STATUS_SUCCESS);
    EXPECT_EQ(qattr.value.u8, 0);

    qattr.id = SAI_QUEUE_ATTR_PORT;
    ASSERT_EQ(sw->get(SAI_OBJECT_TYPE_QUEUE, queue_list[0], 1, &qattr), SAI_STATUS_SUCCESS);
    EXPECT_EQ(qattr.value.oid, port_id);

    qattr.id = SAI_QUEUE_ATTR_TYPE;
    ASSERT_EQ(sw->get(SAI_OBJECT_TYPE_QUEUE, queue_list[10], 1, &qattr), SAI_STATUS_SUCCESS);
    EXPECT_EQ(qattr.value.s32, SAI_QUEUE_TYPE_MULTICAST);

    qattr.id = SAI_QUEUE_ATTR_INDEX;
    ASSERT_EQ(sw->get(SAI_OBJECT_TYPE_QUEUE, queue_list[10], 1, &qattr), SAI_STATUS_SUCCESS);
    EXPECT_EQ(qattr.value.u8, 10);
}

TEST(SwitchNokiaVS, vs_create_hostif_tap_interface_lo)
{
    auto sw = createNokiaSwitch(false);

    ASSERT_NE(sw, nullptr);

    sai_attribute_t port_attr;
    sai_object_id_t port_list[64];
    port_attr.id = SAI_SWITCH_ATTR_PORT_LIST;
    port_attr.value.objlist.count = 64;
    port_attr.value.objlist.list = port_list;

    ASSERT_EQ(sw->get(SAI_OBJECT_TYPE_SWITCH, "oid:0x2100000000", 1, &port_attr), SAI_STATUS_SUCCESS);
    ASSERT_GT(port_attr.value.objlist.count, (uint32_t)1);

    sai_object_id_t port_id = port_list[1];

    sai_attribute_t admin_attr;
    admin_attr.id = SAI_PORT_ATTR_ADMIN_STATE;
    admin_attr.value.booldata = true;

    ASSERT_EQ(sw->set(SAI_OBJECT_TYPE_PORT, port_id, &admin_attr), SAI_STATUS_SUCCESS);

    sai_attribute_t attrs[3];

    attrs[0].id = SAI_HOSTIF_ATTR_TYPE;
    attrs[0].value.s32 = SAI_HOSTIF_TYPE_NETDEV;

    attrs[1].id = SAI_HOSTIF_ATTR_OBJ_ID;
    attrs[1].value.oid = port_id;

    attrs[2].id = SAI_HOSTIF_ATTR_NAME;
    strncpy(attrs[2].value.chardata, "lo", sizeof(attrs[2].value.chardata));

    EXPECT_EQ(sw->vs_create_hostif_tap_interface(3, attrs), SAI_STATUS_SUCCESS);
}

static void dummyPortStateChangeCb(
        _In_ uint32_t count,
        _In_ const sai_port_oper_status_notification_t *data)
{
    SWSS_LOG_ENTER();
}

TEST(SwitchNokiaVS, vs_create_hostif_tap_interface_lo_with_callback)
{
    auto sw = createNokiaSwitch(false);

    ASSERT_NE(sw, nullptr);

    sai_attribute_t cb_attr;
    cb_attr.id = SAI_SWITCH_ATTR_PORT_STATE_CHANGE_NOTIFY;
    cb_attr.value.ptr = (sai_pointer_t)dummyPortStateChangeCb;

    ASSERT_EQ(sw->set(SAI_OBJECT_TYPE_SWITCH, 0x2100000000, &cb_attr), SAI_STATUS_SUCCESS);

    sai_attribute_t port_attr;
    sai_object_id_t port_list[64];
    port_attr.id = SAI_SWITCH_ATTR_PORT_LIST;
    port_attr.value.objlist.count = 64;
    port_attr.value.objlist.list = port_list;

    ASSERT_EQ(sw->get(SAI_OBJECT_TYPE_SWITCH, "oid:0x2100000000", 1, &port_attr), SAI_STATUS_SUCCESS);
    ASSERT_GT(port_attr.value.objlist.count, (uint32_t)1);

    sai_object_id_t port_id = port_list[1];

    sai_attribute_t admin_attr;
    admin_attr.id = SAI_PORT_ATTR_ADMIN_STATE;
    admin_attr.value.booldata = true;

    ASSERT_EQ(sw->set(SAI_OBJECT_TYPE_PORT, port_id, &admin_attr), SAI_STATUS_SUCCESS);

    sai_attribute_t attrs[3];

    attrs[0].id = SAI_HOSTIF_ATTR_TYPE;
    attrs[0].value.s32 = SAI_HOSTIF_TYPE_NETDEV;

    attrs[1].id = SAI_HOSTIF_ATTR_OBJ_ID;
    attrs[1].value.oid = port_id;

    attrs[2].id = SAI_HOSTIF_ATTR_NAME;
    strncpy(attrs[2].value.chardata, "lo", sizeof(attrs[2].value.chardata));

    EXPECT_EQ(sw->vs_create_hostif_tap_interface(3, attrs), SAI_STATUS_SUCCESS);
}

TEST(SwitchNokiaVS, vs_remove_hostif_tap_full_path)
{
    auto sw = createNokiaSwitch(false);

    ASSERT_NE(sw, nullptr);

    sai_attribute_t port_attr;
    sai_object_id_t port_list[64];
    port_attr.id = SAI_SWITCH_ATTR_PORT_LIST;
    port_attr.value.objlist.count = 64;
    port_attr.value.objlist.list = port_list;

    ASSERT_EQ(sw->get(SAI_OBJECT_TYPE_SWITCH, "oid:0x2100000000", 1, &port_attr), SAI_STATUS_SUCCESS);
    ASSERT_GT(port_attr.value.objlist.count, (uint32_t)1);

    sai_object_id_t port_id = port_list[1];

    sai_attribute_t admin_attr;
    admin_attr.id = SAI_PORT_ATTR_ADMIN_STATE;
    admin_attr.value.booldata = true;

    ASSERT_EQ(sw->set(SAI_OBJECT_TYPE_PORT, port_id, &admin_attr), SAI_STATUS_SUCCESS);

    sai_object_id_t hostif_id;
    sai_attribute_t attrs[3];

    attrs[0].id = SAI_HOSTIF_ATTR_TYPE;
    attrs[0].value.s32 = SAI_HOSTIF_TYPE_NETDEV;

    attrs[1].id = SAI_HOSTIF_ATTR_OBJ_ID;
    attrs[1].value.oid = port_id;

    attrs[2].id = SAI_HOSTIF_ATTR_NAME;
    strncpy(attrs[2].value.chardata, "lo", sizeof(attrs[2].value.chardata));

    ASSERT_EQ(sw->create(SAI_OBJECT_TYPE_HOSTIF, &hostif_id, 0x2100000000, 3, attrs), SAI_STATUS_SUCCESS);

    ASSERT_EQ(sw->vs_create_hostif_tap_interface(3, attrs), SAI_STATUS_SUCCESS);

    EXPECT_EQ(sw->vs_remove_hostif_tap_interface(hostif_id), SAI_STATUS_SUCCESS);
}
