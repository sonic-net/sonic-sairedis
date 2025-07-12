#include <cstdint>

#include <memory>
#include <string>
#include <vector>
#include <array>

#include <gtest/gtest.h>

#include "meta/Globals.h"
#include "meta/sai_serialize.h"

#include "ContextConfigContainer.h"
#include "SwitchMLNX2700.h"

using namespace saimeta;
using namespace saivs;

class SwitchMLNX2700Test : public ::testing::Test
{
public:
    SwitchMLNX2700Test() = default;
    virtual ~SwitchMLNX2700Test() = default;

public:
    virtual void SetUp() override
    {
        m_ccc = ContextConfigContainer::getDefault();
        m_cc = m_ccc->get(m_guid);
        m_scc = m_cc->m_scc;
        m_sc = m_scc->getConfig(m_scid);

        m_sc->m_saiSwitchType = SAI_SWITCH_TYPE_NPU;
        m_sc->m_switchType = SAI_VS_SWITCH_TYPE_MLNX2700;
        m_sc->m_bootType = SAI_VS_BOOT_TYPE_COLD;
        m_sc->m_useTapDevice = false;
        m_sc->m_laneMap = LaneMap::getDefaultLaneMap();
        m_sc->m_eventQueue = std::make_shared<EventQueue>(std::make_shared<Signal>());

        m_ridmgr = std::make_shared<RealObjectIdManager>(m_cc->m_guid, m_cc->m_scc);
        m_swid = m_ridmgr->allocateNewSwitchObjectId(Globals::getHardwareInfo(0, nullptr));
        m_ss = std::make_shared<SwitchMLNX2700>(m_swid, m_ridmgr, m_sc);
    }

    virtual void TearDown() override
    {
        // Empty
    }

protected:
    std::shared_ptr<ContextConfigContainer> m_ccc;
    std::shared_ptr<ContextConfig> m_cc;
    std::shared_ptr<SwitchConfigContainer> m_scc;
    std::shared_ptr<SwitchConfig> m_sc;
    std::shared_ptr<RealObjectIdManager> m_ridmgr;
    std::shared_ptr<SwitchStateBase> m_ss;

    sai_object_id_t m_swid = SAI_NULL_OBJECT_ID;

    const std::uint32_t m_guid = 0; // default context config id
    const std::uint32_t m_scid = 0; // default switch config id
};

TEST_F(SwitchMLNX2700Test, portBulkAddRemove)
{
    const std::uint32_t portCount = 32;
    const std::uint32_t laneCount = 4;

    // Generate port object ids
    std::vector<sai_object_id_t> oidList(portCount, SAI_NULL_OBJECT_ID);

    for (std::uint32_t idx = 0; idx < portCount; idx++)
    {
        oidList[idx] = m_ridmgr->allocateNewObjectId(SAI_OBJECT_TYPE_PORT, m_swid);
    }

    // Serialize port object ids
    std::vector<std::string> serializedOidList;

    for (std::uint32_t idx = 0; idx < portCount; idx++)
    {
        serializedOidList.emplace_back(sai_serialize_object_id(oidList[idx]));
    }

    // Generate port config
    std::vector<std::array<std::uint32_t, laneCount>> laneDataList;
    std::vector<std::vector<sai_attribute_t>> attrDataList;
    std::vector<std::uint32_t> attrCountList;
    std::vector<const sai_attribute_t*> attrPtrList;

    std::vector<sai_status_t> statusList(portCount, SAI_STATUS_SUCCESS);

    for (std::uint32_t idx = 0; idx < portCount * 4; idx += 4)
    {
        sai_attribute_t attr;
        std::vector<sai_attribute_t> attrList;

        std::array<std::uint32_t, laneCount> laneList = { idx, idx + 1, idx + 2, idx + 3 };
        laneDataList.push_back(laneList);

        attr.id = SAI_PORT_ATTR_HW_LANE_LIST;
        attr.value.u32list.count = static_cast<std::uint32_t>(laneDataList.back().size());
        attr.value.u32list.list = laneDataList.back().data();
        attrList.push_back(attr);

        attr.id = SAI_PORT_ATTR_SPEED;
        attr.value.u32 = 1000;
        attrList.push_back(attr);

        attrDataList.push_back(attrList);
        attrCountList.push_back(static_cast<std::uint32_t>(attrDataList.back().size()));
        attrPtrList.push_back(attrDataList.back().data());
    }

    // Verify port bulk add
    auto status = m_ss->bulkCreate(
        m_swid, SAI_OBJECT_TYPE_PORT, serializedOidList, attrCountList.data(), attrPtrList.data(),
        SAI_BULK_OP_ERROR_MODE_IGNORE_ERROR,
        statusList.data()
    );
    ASSERT_EQ(status, SAI_STATUS_SUCCESS);

    for (std::uint32_t i = 0; i < portCount; i++)
    {
        ASSERT_EQ(statusList.at(i), SAI_STATUS_SUCCESS);
    }

    // Verify port bulk remove
    status = m_ss->bulkRemove(
        SAI_OBJECT_TYPE_PORT, serializedOidList,
        SAI_BULK_OP_ERROR_MODE_IGNORE_ERROR,
        statusList.data()
    );
    ASSERT_EQ(status, SAI_STATUS_SUCCESS);

    for (std::uint32_t i = 0; i < portCount; i++)
    {
        ASSERT_EQ(statusList.at(i), SAI_STATUS_SUCCESS);
    }
}

TEST_F(SwitchMLNX2700Test, switchQueueNumberGet)
{
    // Initialize switch state
    ASSERT_EQ(m_ss->initialize_default_objects(0, nullptr), SAI_STATUS_SUCCESS);

    const sai_uint32_t uqNum = 8;
    const sai_uint32_t mqNum = 8;
    const sai_uint32_t qNum = uqNum + mqNum;

    sai_attribute_t attr;

    // Verify unicast queue number
    attr.id = SAI_SWITCH_ATTR_NUMBER_OF_UNICAST_QUEUES;
    ASSERT_EQ(m_ss->get(SAI_OBJECT_TYPE_SWITCH, m_swid, 1, &attr), SAI_STATUS_SUCCESS);
    ASSERT_EQ(attr.value.u32, uqNum);

    // Verify multicast queue number
    attr.id = SAI_SWITCH_ATTR_NUMBER_OF_MULTICAST_QUEUES;
    ASSERT_EQ(m_ss->get(SAI_OBJECT_TYPE_SWITCH, m_swid, 1, &attr), SAI_STATUS_SUCCESS);
    ASSERT_EQ(attr.value.u32, mqNum);

    // Verify total queue number
    attr.id = SAI_SWITCH_ATTR_NUMBER_OF_QUEUES;
    ASSERT_EQ(m_ss->get(SAI_OBJECT_TYPE_SWITCH, m_swid, 1, &attr), SAI_STATUS_SUCCESS);
    ASSERT_EQ(attr.value.u32, qNum);
}

TEST(SwitchMLNX2700, ctr)
{
    auto sc = std::make_shared<SwitchConfig>(0, "");
    auto signal = std::make_shared<Signal>();
    auto eventQueue = std::make_shared<EventQueue>(signal);

    sc->m_saiSwitchType = SAI_SWITCH_TYPE_NPU;
    sc->m_switchType = SAI_VS_SWITCH_TYPE_MLNX2700;
    sc->m_bootType = SAI_VS_BOOT_TYPE_COLD;
    sc->m_useTapDevice = false;
    sc->m_laneMap = LaneMap::getDefaultLaneMap(0);
    sc->m_eventQueue = eventQueue;

    auto scc = std::make_shared<SwitchConfigContainer>();

    scc->insert(sc);

    SwitchMLNX2700 sw(
            0x2100000000,
            std::make_shared<RealObjectIdManager>(0, scc),
            sc);

    SwitchMLNX2700 sw2(
            0x2100000000,
            std::make_shared<RealObjectIdManager>(0, scc),
            sc,
            nullptr);

    sai_attribute_t attr;

    attr.id = SAI_SWITCH_ATTR_INIT_SWITCH;
    attr.value.booldata = true;

    EXPECT_EQ(sw.initialize_default_objects(1, &attr), SAI_STATUS_SUCCESS);
}

TEST(SwitchMLNX2700, refresh_bridge_port_list)
{
    auto sc = std::make_shared<SwitchConfig>(0, "");
    auto signal = std::make_shared<Signal>();
    auto eventQueue = std::make_shared<EventQueue>(signal);

    sc->m_saiSwitchType = SAI_SWITCH_TYPE_NPU;
    sc->m_switchType = SAI_VS_SWITCH_TYPE_MLNX2700;
    sc->m_bootType = SAI_VS_BOOT_TYPE_COLD;
    sc->m_useTapDevice = false;
    sc->m_laneMap = LaneMap::getDefaultLaneMap(0);
    sc->m_eventQueue = eventQueue;

    auto scc = std::make_shared<SwitchConfigContainer>();

    scc->insert(sc);

    SwitchMLNX2700 sw(
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

TEST(SwitchMLNX2700, warm_update_queues)
{
    auto sc = std::make_shared<SwitchConfig>(0, "");
    auto signal = std::make_shared<Signal>();
    auto eventQueue = std::make_shared<EventQueue>(signal);

    sc->m_saiSwitchType = SAI_SWITCH_TYPE_NPU;
    sc->m_switchType = SAI_VS_SWITCH_TYPE_MLNX2700;
    sc->m_bootType = SAI_VS_BOOT_TYPE_COLD;
    sc->m_useTapDevice = false;
    sc->m_laneMap = LaneMap::getDefaultLaneMap(0);
    sc->m_eventQueue = eventQueue;

    auto scc = std::make_shared<SwitchConfigContainer>();

    scc->insert(sc);

    auto roidm = std::make_shared<RealObjectIdManager>(0, scc);

    EXPECT_TRUE(getWarmBootState("files/mlnx2700.warm.bin", roidm));

    auto warmBootState = std::make_shared<WarmBootState>(g_warmBootState.at(0x2100000000)); // copy ctr

    SwitchMLNX2700 sw(
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

TEST(SwitchMLNX2700, test_tunnel_term_capability)
{
    auto sc = std::make_shared<SwitchConfig>(0, "");
    auto signal = std::make_shared<Signal>();
    auto eventQueue = std::make_shared<EventQueue>(signal);

    sc->m_saiSwitchType = SAI_SWITCH_TYPE_NPU;
    sc->m_switchType = SAI_VS_SWITCH_TYPE_MLNX2700;
    sc->m_bootType = SAI_VS_BOOT_TYPE_COLD;
    sc->m_useTapDevice = false;
    sc->m_laneMap = LaneMap::getDefaultLaneMap(0);
    sc->m_eventQueue = eventQueue;

    auto scc = std::make_shared<SwitchConfigContainer>();

    scc->insert(sc);

    SwitchMLNX2700 sw(
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

    EXPECT_EQ(enum_val_cap.count, 1);

    EXPECT_EQ(enum_val_cap.list[0], SAI_TUNNEL_PEER_MODE_P2MP);

}

TEST(SwitchMLNX2700, test_vlan_flood_capability)
{
    auto sc = std::make_shared<SwitchConfig>(0, "");
    auto signal = std::make_shared<Signal>();
    auto eventQueue = std::make_shared<EventQueue>(signal);

    sc->m_saiSwitchType = SAI_SWITCH_TYPE_NPU;
    sc->m_switchType = SAI_VS_SWITCH_TYPE_MLNX2700;
    sc->m_bootType = SAI_VS_BOOT_TYPE_COLD;
    sc->m_useTapDevice = false;
    sc->m_laneMap = LaneMap::getDefaultLaneMap(0);
    sc->m_eventQueue = eventQueue;

    auto scc = std::make_shared<SwitchConfigContainer>();

    scc->insert(sc);

    SwitchMLNX2700 sw(
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

    EXPECT_EQ(enum_val_cap.count, 4);

    int flood_types_found = 0;

    for (uint32_t i = 0; i < enum_val_cap.count; i++)
    {
        if (enum_val_cap.list[i] == SAI_VLAN_FLOOD_CONTROL_TYPE_ALL ||
            enum_val_cap.list[i] == SAI_VLAN_FLOOD_CONTROL_TYPE_NONE ||
            enum_val_cap.list[i] == SAI_VLAN_FLOOD_CONTROL_TYPE_L2MC_GROUP ||
            enum_val_cap.list[i] == SAI_VLAN_FLOOD_CONTROL_TYPE_COMBINED)
        {
            flood_types_found++;
        }
    }

    EXPECT_EQ(flood_types_found, 4);

    memset(list, 0, sizeof(list));
    flood_types_found = 0;
    enum_val_cap.count = 4;
    enum_val_cap.list = list;

    EXPECT_EQ(sw.queryAttrEnumValuesCapability(0x2100000000,
                                               SAI_OBJECT_TYPE_VLAN,
                                               SAI_VLAN_ATTR_UNKNOWN_MULTICAST_FLOOD_CONTROL_TYPE,
                                               &enum_val_cap),
                                               SAI_STATUS_SUCCESS);

    EXPECT_EQ(enum_val_cap.count, 4);

    for (uint32_t i = 0; i < enum_val_cap.count; i++)
    {
        if (enum_val_cap.list[i] == SAI_VLAN_FLOOD_CONTROL_TYPE_ALL ||
            enum_val_cap.list[i] == SAI_VLAN_FLOOD_CONTROL_TYPE_NONE ||
            enum_val_cap.list[i] == SAI_VLAN_FLOOD_CONTROL_TYPE_L2MC_GROUP ||
            enum_val_cap.list[i] == SAI_VLAN_FLOOD_CONTROL_TYPE_COMBINED)
        {
            flood_types_found++;
        }
    }

    EXPECT_EQ(flood_types_found, 4);

    memset(list, 0, sizeof(list));
    flood_types_found = 0;
    enum_val_cap.count = 4;
    enum_val_cap.list = list;

    EXPECT_EQ(sw.queryAttrEnumValuesCapability(0x2100000000,
                                               SAI_OBJECT_TYPE_VLAN,
                                               SAI_VLAN_ATTR_BROADCAST_FLOOD_CONTROL_TYPE,
                                               &enum_val_cap),
                                               SAI_STATUS_SUCCESS);

    EXPECT_EQ(enum_val_cap.count, 4);

    for (uint32_t i = 0; i < enum_val_cap.count; i++)
    {
        if (enum_val_cap.list[i] == SAI_VLAN_FLOOD_CONTROL_TYPE_ALL ||
            enum_val_cap.list[i] == SAI_VLAN_FLOOD_CONTROL_TYPE_NONE ||
            enum_val_cap.list[i] == SAI_VLAN_FLOOD_CONTROL_TYPE_L2MC_GROUP ||
            enum_val_cap.list[i] == SAI_VLAN_FLOOD_CONTROL_TYPE_COMBINED)
        {
            flood_types_found++;
        }
    }

    EXPECT_EQ(flood_types_found, 4);
}

TEST(SwitchMLNX2700, test_port_autoneg_fec_override_support)
{
    auto sc = std::make_shared<SwitchConfig>(0, "");
    auto signal = std::make_shared<Signal>();
    auto eventQueue = std::make_shared<EventQueue>(signal);

    sc->m_saiSwitchType = SAI_SWITCH_TYPE_NPU;
    sc->m_switchType = SAI_VS_SWITCH_TYPE_MLNX2700;
    sc->m_bootType = SAI_VS_BOOT_TYPE_COLD;
    sc->m_useTapDevice = false;
    sc->m_laneMap = LaneMap::getDefaultLaneMap(0);
    sc->m_eventQueue = eventQueue;

    auto scc = std::make_shared<SwitchConfigContainer>();

    scc->insert(sc);

    SwitchMLNX2700 sw(
            0x2100000000,
            std::make_shared<RealObjectIdManager>(0, scc),
            sc);

    sai_attr_capability_t attr_capability;

    EXPECT_EQ(sw.queryAttributeCapability(0x2100000000,
                                          SAI_OBJECT_TYPE_PORT,
                                          SAI_PORT_ATTR_AUTO_NEG_FEC_MODE_OVERRIDE,
                                          &attr_capability),
                                          SAI_STATUS_SUCCESS);

    EXPECT_EQ(attr_capability.create_implemented, true);
    EXPECT_EQ(attr_capability.set_implemented, true);
    EXPECT_EQ(attr_capability.get_implemented, true);
}

TEST(SwitchMLNX2700, test_stats_query_capability)
{
    auto sc = std::make_shared<SwitchConfig>(0, "");
    auto signal = std::make_shared<Signal>();
    auto eventQueue = std::make_shared<EventQueue>(signal);

    sc->m_saiSwitchType = SAI_SWITCH_TYPE_NPU;
    sc->m_switchType = SAI_VS_SWITCH_TYPE_MLNX2700;
    sc->m_bootType = SAI_VS_BOOT_TYPE_COLD;
    sc->m_useTapDevice = false;
    sc->m_laneMap = LaneMap::getDefaultLaneMap(0);
    sc->m_eventQueue = eventQueue;

    auto scc = std::make_shared<SwitchConfigContainer>();

    scc->insert(sc);

    SwitchMLNX2700 sw(
            0x2100000000,
            std::make_shared<RealObjectIdManager>(0, scc),
            sc);

    std::vector<sai_stat_capability_t> capability_list;
    sai_stat_capability_list_t stats_capability;

    /* Get queue stats capability */
    stats_capability.count = 0;
    stats_capability.list = nullptr;

    EXPECT_EQ(sw.queryStatsCapability(0x2100000000,
                                          SAI_OBJECT_TYPE_QUEUE,
                                          &stats_capability),
                                          SAI_STATUS_BUFFER_OVERFLOW);

    capability_list.resize(stats_capability.count);
    stats_capability.list = capability_list.data();

    EXPECT_EQ(sw.queryStatsCapability(0x2100000000,
                                          SAI_OBJECT_TYPE_QUEUE,
                                          &stats_capability),
                                          SAI_STATUS_SUCCESS);

    /* Get port stats capability */
    stats_capability.count = 0;
    stats_capability.list = nullptr;

    EXPECT_EQ(sw.queryStatsCapability(0x2100000000,
                                          SAI_OBJECT_TYPE_PORT,
                                          &stats_capability),
                                          SAI_STATUS_BUFFER_OVERFLOW);

    capability_list.resize(stats_capability.count);
    stats_capability.list = capability_list.data();

    EXPECT_EQ(sw.queryStatsCapability(0x2100000000,
                                          SAI_OBJECT_TYPE_PORT,
                                          &stats_capability),
                                          SAI_STATUS_SUCCESS);
}
