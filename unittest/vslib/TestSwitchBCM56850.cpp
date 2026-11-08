#include "SwitchBCM56850.h"

#include "meta/sai_serialize.h"

#include <gtest/gtest.h>

#include <memory>

using namespace saivs;

TEST(SwitchBCM56850, ctr)
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

    SwitchBCM56850 sw(
            0x2100000000,
            std::make_shared<RealObjectIdManager>(0, scc),
            sc);

    SwitchBCM56850 sw2(
            0x2100000000,
            std::make_shared<RealObjectIdManager>(0, scc),
            sc,
            nullptr);

    sai_attribute_t attr;

    attr.id = SAI_SWITCH_ATTR_INIT_SWITCH;
    attr.value.booldata = true;

    EXPECT_EQ(sw.initialize_default_objects(1, &attr), SAI_STATUS_SUCCESS);
}

TEST(SwitchBCM56850, refresh_bridge_port_list)
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

    SwitchBCM56850 sw(
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

    //std::cout << sw.dump_switch_database_for_warm_restart();
}

static std::map<sai_object_id_t, WarmBootState> g_warmBootState;
// TODO move to utils
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

        // line format: OBJECT_TYPE OBJECT_ID ATTR_ID ATTR_VALUE
        std::istringstream iss(line);

        std::string strObjectType;
        std::string strObjectId;
        std::string strAttrId;
        std::string strAttrValue;

        iss >> strObjectType >> strObjectId;

        if (strObjectType == SAI_VS_FDB_INFO)
        {
            /*
             * If we read line from fdb info set and use tap device is enabled
             * just parse line and repopulate fdb info set.
             */

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

        /*
         * Since all objects we are creating, then during warm boot we need to
         * get the biggest object index, so after warm boot we can start
         * generating new objects with index value not colliding with objects
         * loaded from warm boot scenario. We only need to consider OID
         * objects.
         */

        roidm->updateWarmBootObjectIndex(metaKey.objectkey.key.object_id);

        // query each object for switch id

        auto switchId = roidm->switchIdQuery(metaKey.objectkey.key.object_id);

        if (switchId == SAI_NULL_OBJECT_ID)
        {
            SWSS_LOG_ERROR("switchIdQuery returned NULL on oid = %s",
                    sai_serialize_object_id(metaKey.objectkey.key.object_id).c_str());

            g_warmBootState.clear();
            return false;
        }

        g_warmBootState[switchId].m_switchId = switchId;

        auto &objectHash = g_warmBootState[switchId].m_objectHash[metaKey.objecttype]; // will create if not exist

        if (objectHash.find(strObjectId) == objectHash.end())
        {
            objectHash[strObjectId] = {};
        }

        if (strAttrId == "NULL")
        {
            // skip empty attributes
            continue;
        }

        objectHash[strObjectId][strAttrId] =
            std::make_shared<SaiAttrWrap>(strAttrId, strAttrValue);
    }

    // NOTE notification pointers should be restored by attr_list when creating switch

    ifs.close();

    return true;
}

TEST(SwitchBCM56850, test_queue_number_get)
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

    SwitchBCM56850 sw(
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

TEST(SwitchBCM56850, warm_update_queues)
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

    auto roidm = std::make_shared<RealObjectIdManager>(0, scc);

    EXPECT_TRUE(getWarmBootState("files/mlnx2700.warm.bin", roidm));

    auto warmBootState = std::make_shared<WarmBootState>(g_warmBootState.at(0x2100000000)); // copy ctr

    SwitchBCM56850 sw(
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

TEST(SwitchBCM56850, test_tunnel_term_capability)
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

    SwitchBCM56850 sw(
            0x2100000000,
            std::make_shared<RealObjectIdManager>(0, scc),
            sc);


    sai_s32_list_t enum_val_cap;
    int32_t list[2];

    enum_val_cap.count = 2;
    enum_val_cap.list = list;

    EXPECT_EQ(sw.queryAttrEnumValuesCapability(0x2100000000,
                                               SAI_OBJECT_TYPE_TUNNEL,
                                               SAI_TUNNEL_ATTR_PEER_MODE,
                                               &enum_val_cap),
                                               SAI_STATUS_SUCCESS);

    EXPECT_EQ(enum_val_cap.count, 2);

    int modes_found = 0;

    for (uint32_t i = 0; i < enum_val_cap.count; i++)
    {
        if (enum_val_cap.list[i] == SAI_TUNNEL_PEER_MODE_P2MP || enum_val_cap.list[i] == SAI_TUNNEL_PEER_MODE_P2P)
        {
            modes_found++;
        }
    }

    EXPECT_EQ(modes_found, 2);
}

TEST(SwitchBCM56850, test_vlan_flood_capability)
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

    SwitchBCM56850 sw(
            0x2100000000,
            std::make_shared<RealObjectIdManager>(0, scc),
            sc);

    sai_s32_list_t enum_val_cap;
    int32_t list[4];

    enum_val_cap.count = 4;
    enum_val_cap.list = list;

    EXPECT_EQ(sw.queryAttrEnumValuesCapability(0x2100000000,
                                               SAI_OBJECT_TYPE_VLAN,
                                               SAI_VLAN_ATTR_UNKNOWN_UNICAST_FLOOD_CONTROL_TYPE,
                                               &enum_val_cap),
                                               SAI_STATUS_SUCCESS);

    EXPECT_EQ(enum_val_cap.count, 3);

    int flood_types_found = 0;
    for (uint32_t i = 0; i < enum_val_cap.count; i++)
    {
        if (enum_val_cap.list[i] == SAI_VLAN_FLOOD_CONTROL_TYPE_ALL ||
            enum_val_cap.list[i] == SAI_VLAN_FLOOD_CONTROL_TYPE_NONE ||
            enum_val_cap.list[i] == SAI_VLAN_FLOOD_CONTROL_TYPE_L2MC_GROUP)
        {
            flood_types_found++;
        }
    }

    EXPECT_EQ(flood_types_found, 3);

    memset(list, 0, sizeof(list));
    flood_types_found = 0;
    enum_val_cap.count = 4;
    enum_val_cap.list = list;

    EXPECT_EQ(sw.queryAttrEnumValuesCapability(0x2100000000,
                                               SAI_OBJECT_TYPE_VLAN,
                                               SAI_VLAN_ATTR_UNKNOWN_MULTICAST_FLOOD_CONTROL_TYPE,
                                               &enum_val_cap),
                                               SAI_STATUS_SUCCESS);

    EXPECT_EQ(enum_val_cap.count, 3);

    for (uint32_t i = 0; i < enum_val_cap.count; i++)
    {
        if (enum_val_cap.list[i] == SAI_VLAN_FLOOD_CONTROL_TYPE_ALL ||
            enum_val_cap.list[i] == SAI_VLAN_FLOOD_CONTROL_TYPE_NONE ||
            enum_val_cap.list[i] == SAI_VLAN_FLOOD_CONTROL_TYPE_L2MC_GROUP)
        {
            flood_types_found++;
        }
    }

    EXPECT_EQ(flood_types_found, 3);

    memset(list, 0, sizeof(list));
    flood_types_found = 0;
    enum_val_cap.count = 4;
    enum_val_cap.list = list;

    EXPECT_EQ(sw.queryAttrEnumValuesCapability(0x2100000000,
                                               SAI_OBJECT_TYPE_VLAN,
                                               SAI_VLAN_ATTR_BROADCAST_FLOOD_CONTROL_TYPE,
                                               &enum_val_cap),
                                               SAI_STATUS_SUCCESS);

    EXPECT_EQ(enum_val_cap.count, 3);

    for (uint32_t i = 0; i < enum_val_cap.count; i++)
    {
        if (enum_val_cap.list[i] == SAI_VLAN_FLOOD_CONTROL_TYPE_ALL ||
            enum_val_cap.list[i] == SAI_VLAN_FLOOD_CONTROL_TYPE_NONE ||
            enum_val_cap.list[i] == SAI_VLAN_FLOOD_CONTROL_TYPE_L2MC_GROUP)
        {
            flood_types_found++;
        }
    }

    EXPECT_EQ(flood_types_found, 3);
}

TEST(SwitchBCM56850, test_nexthop_group_type_enum_values_capability)
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

    SwitchBCM56850 sw(
            0x2100000000,
            std::make_shared<RealObjectIdManager>(0, scc),
            sc);

    sai_s32_list_t enum_val_cap;
    int32_t list[5];
    memset(list, 0, sizeof(list));

    enum_val_cap.count = 4;
    enum_val_cap.list = list;

    EXPECT_EQ(sw.queryAttrEnumValuesCapability(0x2100000000,
                                             SAI_OBJECT_TYPE_NEXT_HOP_GROUP,
                                             SAI_NEXT_HOP_GROUP_ATTR_TYPE,
                                             &enum_val_cap),
                                             SAI_STATUS_BUFFER_OVERFLOW);
    enum_val_cap.count = 5;
    EXPECT_EQ(sw.queryAttrEnumValuesCapability(0x2100000000,
                                             SAI_OBJECT_TYPE_NEXT_HOP_GROUP,
                                             SAI_NEXT_HOP_GROUP_ATTR_TYPE,
                                             &enum_val_cap),
                                             SAI_STATUS_SUCCESS);


    EXPECT_EQ(enum_val_cap.count, 5);

    int nexthop_group_types_found = 0;

    for (uint32_t i = 0; i < enum_val_cap.count; i++)
    {
        if (enum_val_cap.list[i] == SAI_NEXT_HOP_GROUP_TYPE_DYNAMIC_UNORDERED_ECMP ||
            enum_val_cap.list[i] == SAI_NEXT_HOP_GROUP_TYPE_DYNAMIC_ORDERED_ECMP ||
            enum_val_cap.list[i] == SAI_NEXT_HOP_GROUP_TYPE_FINE_GRAIN_ECMP ||
            enum_val_cap.list[i] == SAI_NEXT_HOP_GROUP_TYPE_PROTECTION ||
            enum_val_cap.list[i] == SAI_NEXT_HOP_GROUP_TYPE_CLASS_BASED)
        {
            nexthop_group_types_found++;
        }
    }

    EXPECT_EQ(nexthop_group_types_found, 5);
}

TEST(SwitchBCM56850, test_port_autoneg_fec_override_support)
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

    SwitchBCM56850 sw(
            0x2100000000,
            std::make_shared<RealObjectIdManager>(0, scc),
            sc);

    sai_attr_capability_t attr_capability;

    EXPECT_EQ(sw.queryAttributeCapability(0x2100000000,
                                          SAI_OBJECT_TYPE_PORT,
                                          SAI_PORT_ATTR_AUTO_NEG_FEC_MODE_OVERRIDE,
                                          &attr_capability),
                                          SAI_STATUS_SUCCESS);

    EXPECT_EQ(attr_capability.create_implemented, false);
    EXPECT_EQ(attr_capability.set_implemented, false);
    EXPECT_EQ(attr_capability.get_implemented, false);
}
