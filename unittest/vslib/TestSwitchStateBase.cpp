#include <gtest/gtest.h>

#include <cstdint>
#include <vector>
#include <set>
#include <algorithm>

#include "Globals.h"
#include "sai_serialize.h"

#include "MACsecAttr.h"
#include "RealObjectIdManager.h"
#include "ContextConfigContainer.h"

#include "SwitchStateBase.h"

using namespace saimeta;
using namespace saivs;

class SwitchStateBaseTest : public ::testing::Test
{
public:
    SwitchStateBaseTest() = default;
    virtual ~SwitchStateBaseTest() = default;

public:
    virtual void SetUp() override
    {
        m_ccc = ContextConfigContainer::getDefault();
        m_cc = m_ccc->get(m_guid);
        m_scc = m_cc->m_scc;
        m_sc = m_scc->getConfig(m_scid);

        m_ridmgr = std::make_shared<RealObjectIdManager>(m_cc->m_guid, m_cc->m_scc);
        m_swid = m_ridmgr->allocateNewSwitchObjectId(Globals::getHardwareInfo(0, nullptr));
        m_ss = std::make_shared<SwitchStateBase>(m_swid, m_ridmgr, m_sc);
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

TEST_F(SwitchStateBaseTest, switchHashGet)
{
    ASSERT_EQ(m_ss->create_default_hash(), SAI_STATUS_SUCCESS);

    sai_attribute_t attr;
    attr.id = SAI_SWITCH_ATTR_ECMP_HASH;
    ASSERT_EQ(m_ss->get(SAI_OBJECT_TYPE_SWITCH, sai_serialize_object_id(m_swid), 1, &attr), SAI_STATUS_SUCCESS);

    const auto ecmpHashOid = attr.value.oid;
    ASSERT_NE(ecmpHashOid, SAI_NULL_OBJECT_ID);

    attr.id = SAI_HASH_ATTR_NATIVE_HASH_FIELD_LIST;
    attr.value.s32list.list = nullptr;
    attr.value.s32list.count = 0;
    ASSERT_EQ(m_ss->get(SAI_OBJECT_TYPE_HASH, sai_serialize_object_id(ecmpHashOid), 1, &attr), SAI_STATUS_SUCCESS);

    std::vector<sai_int32_t> hfList(attr.value.s32list.count);
    attr.value.s32list.list = hfList.data();
    ASSERT_EQ(m_ss->get(SAI_OBJECT_TYPE_HASH, sai_serialize_object_id(ecmpHashOid), 1, &attr), SAI_STATUS_SUCCESS);

    const std::set<sai_native_hash_field_t> hfSet1 = {
        SAI_NATIVE_HASH_FIELD_DST_MAC,
        SAI_NATIVE_HASH_FIELD_SRC_MAC,
        SAI_NATIVE_HASH_FIELD_ETHERTYPE,
        SAI_NATIVE_HASH_FIELD_IN_PORT
    };

    std::set<sai_native_hash_field_t> hfSet2;

    std::transform(
        hfList.cbegin(), hfList.cend(), std::inserter(hfSet2, hfSet2.begin()),
        [](sai_int32_t value) { return static_cast<sai_native_hash_field_t>(value); }
    );
    ASSERT_EQ(hfSet1, hfSet2);
}

TEST_F(SwitchStateBaseTest, switchHashCapabilitiesGet)
{
    sai_s32_list_t data = { .count = 0, .list = nullptr };

    auto status = m_ss->queryAttrEnumValuesCapability(
        m_swid, SAI_OBJECT_TYPE_HASH, SAI_HASH_ATTR_NATIVE_HASH_FIELD_LIST, &data
    );
    ASSERT_EQ(status, SAI_STATUS_BUFFER_OVERFLOW);

    std::vector<sai_int32_t> hfList(data.count);
    data.list = hfList.data();

    status = m_ss->queryAttrEnumValuesCapability(
        m_swid, SAI_OBJECT_TYPE_HASH, SAI_HASH_ATTR_NATIVE_HASH_FIELD_LIST, &data
    );
    ASSERT_EQ(status, SAI_STATUS_SUCCESS);

    const std::set<sai_native_hash_field_t> hfSet1 = {
        SAI_NATIVE_HASH_FIELD_IN_PORT,
        SAI_NATIVE_HASH_FIELD_DST_MAC,
        SAI_NATIVE_HASH_FIELD_SRC_MAC,
        SAI_NATIVE_HASH_FIELD_ETHERTYPE,
        SAI_NATIVE_HASH_FIELD_VLAN_ID,
        SAI_NATIVE_HASH_FIELD_IP_PROTOCOL,
        SAI_NATIVE_HASH_FIELD_DST_IP,
        SAI_NATIVE_HASH_FIELD_SRC_IP,
        SAI_NATIVE_HASH_FIELD_L4_DST_PORT,
        SAI_NATIVE_HASH_FIELD_L4_SRC_PORT,
        SAI_NATIVE_HASH_FIELD_INNER_DST_MAC,
        SAI_NATIVE_HASH_FIELD_INNER_SRC_MAC,
        SAI_NATIVE_HASH_FIELD_INNER_ETHERTYPE,
        SAI_NATIVE_HASH_FIELD_INNER_IP_PROTOCOL,
        SAI_NATIVE_HASH_FIELD_INNER_DST_IP,
        SAI_NATIVE_HASH_FIELD_INNER_SRC_IP,
        SAI_NATIVE_HASH_FIELD_INNER_L4_DST_PORT,
        SAI_NATIVE_HASH_FIELD_INNER_L4_SRC_PORT,
        SAI_NATIVE_HASH_FIELD_IPV6_FLOW_LABEL
    };

    std::set<sai_native_hash_field_t> hfSet2;

    std::transform(
        hfList.cbegin(), hfList.cend(), std::inserter(hfSet2, hfSet2.begin()),
        [](sai_int32_t value) { return static_cast<sai_native_hash_field_t>(value); }
    );
    ASSERT_EQ(hfSet1, hfSet2);
}

TEST_F(SwitchStateBaseTest, switchHashAlgorithmCapabilitiesGet)
{
    sai_s32_list_t data = { .count = 0, .list = nullptr };

    auto status = m_ss->queryAttrEnumValuesCapability(
        m_swid, SAI_OBJECT_TYPE_SWITCH, SAI_SWITCH_ATTR_ECMP_DEFAULT_HASH_ALGORITHM, &data
    );
    ASSERT_EQ(status, SAI_STATUS_BUFFER_OVERFLOW);

    std::vector<sai_int32_t> haList(data.count);
    data.list = haList.data();

    status = m_ss->queryAttrEnumValuesCapability(
        m_swid, SAI_OBJECT_TYPE_SWITCH, SAI_SWITCH_ATTR_ECMP_DEFAULT_HASH_ALGORITHM, &data
    );
    ASSERT_EQ(status, SAI_STATUS_SUCCESS);

    const std::set<sai_hash_algorithm_t> haSet1 = {
        SAI_HASH_ALGORITHM_CRC,
        SAI_HASH_ALGORITHM_XOR,
        SAI_HASH_ALGORITHM_RANDOM,
        SAI_HASH_ALGORITHM_CRC_32LO,
        SAI_HASH_ALGORITHM_CRC_32HI,
        SAI_HASH_ALGORITHM_CRC_CCITT,
        SAI_HASH_ALGORITHM_CRC_XOR
    };

    std::set<sai_hash_algorithm_t> haSet2;

    std::transform(
        haList.cbegin(), haList.cend(), std::inserter(haSet2, haSet2.begin()),
        [](sai_int32_t value) { return static_cast<sai_hash_algorithm_t>(value); }
    );
    ASSERT_EQ(haSet1, haSet2);
}

TEST_F(SwitchStateBaseTest, switchPacketTrimmingDscpModeCapabilitiesGet)
{
    sai_s32_list_t data = { .count = 0, .list = nullptr };

    auto status = m_ss->queryAttrEnumValuesCapability(
        m_swid, SAI_OBJECT_TYPE_SWITCH, SAI_SWITCH_ATTR_PACKET_TRIM_DSCP_RESOLUTION_MODE, &data
    );
    ASSERT_EQ(status, SAI_STATUS_BUFFER_OVERFLOW);

    std::vector<sai_int32_t> qmList(data.count);
    data.list = qmList.data();

    status = m_ss->queryAttrEnumValuesCapability(
        m_swid, SAI_OBJECT_TYPE_SWITCH, SAI_SWITCH_ATTR_PACKET_TRIM_DSCP_RESOLUTION_MODE, &data
    );
    ASSERT_EQ(status, SAI_STATUS_SUCCESS);

    const std::set<sai_packet_trim_dscp_resolution_mode_t> qmSet1 = {
        SAI_PACKET_TRIM_DSCP_RESOLUTION_MODE_DSCP_VALUE,
        SAI_PACKET_TRIM_DSCP_RESOLUTION_MODE_FROM_TC
    };

    std::set<sai_packet_trim_dscp_resolution_mode_t> qmSet2;

    std::transform(
        qmList.cbegin(), qmList.cend(), std::inserter(qmSet2, qmSet2.begin()),
        [](sai_int32_t value) { return static_cast<sai_packet_trim_dscp_resolution_mode_t>(value); }
    );
    ASSERT_EQ(qmSet1, qmSet2);
}

TEST_F(SwitchStateBaseTest, switchPacketTrimmingQueueModeCapabilitiesGet)
{
    sai_s32_list_t data = { .count = 0, .list = nullptr };

    auto status = m_ss->queryAttrEnumValuesCapability(
        m_swid, SAI_OBJECT_TYPE_SWITCH, SAI_SWITCH_ATTR_PACKET_TRIM_QUEUE_RESOLUTION_MODE, &data
    );
    ASSERT_EQ(status, SAI_STATUS_BUFFER_OVERFLOW);

    std::vector<sai_int32_t> qmList(data.count);
    data.list = qmList.data();

    status = m_ss->queryAttrEnumValuesCapability(
        m_swid, SAI_OBJECT_TYPE_SWITCH, SAI_SWITCH_ATTR_PACKET_TRIM_QUEUE_RESOLUTION_MODE, &data
    );
    ASSERT_EQ(status, SAI_STATUS_SUCCESS);

    const std::set<sai_packet_trim_queue_resolution_mode_t> qmSet1 = {
        SAI_PACKET_TRIM_QUEUE_RESOLUTION_MODE_STATIC,
        SAI_PACKET_TRIM_QUEUE_RESOLUTION_MODE_DYNAMIC
    };

    std::set<sai_packet_trim_queue_resolution_mode_t> qmSet2;

    std::transform(
        qmList.cbegin(), qmList.cend(), std::inserter(qmSet2, qmSet2.begin()),
        [](sai_int32_t value) { return static_cast<sai_packet_trim_queue_resolution_mode_t>(value); }
    );
    ASSERT_EQ(qmSet1, qmSet2);
}

TEST_F(SwitchStateBaseTest, bufferProfilePacketAdmissionFailActionCapabilitiesGet)
{
    sai_s32_list_t data = { .count = 0, .list = nullptr };

    auto status = m_ss->queryAttrEnumValuesCapability(
        m_swid, SAI_OBJECT_TYPE_BUFFER_PROFILE, SAI_BUFFER_PROFILE_ATTR_PACKET_ADMISSION_FAIL_ACTION, &data
    );
    ASSERT_EQ(status, SAI_STATUS_BUFFER_OVERFLOW);

    std::vector<sai_int32_t> paList(data.count);
    data.list = paList.data();

    status = m_ss->queryAttrEnumValuesCapability(
        m_swid, SAI_OBJECT_TYPE_BUFFER_PROFILE, SAI_BUFFER_PROFILE_ATTR_PACKET_ADMISSION_FAIL_ACTION, &data
    );
    ASSERT_EQ(status, SAI_STATUS_SUCCESS);

    const std::set<sai_buffer_profile_packet_admission_fail_action_t> paSet1 = {
        SAI_BUFFER_PROFILE_PACKET_ADMISSION_FAIL_ACTION_DROP,
        SAI_BUFFER_PROFILE_PACKET_ADMISSION_FAIL_ACTION_DROP_AND_TRIM
    };

    std::set<sai_buffer_profile_packet_admission_fail_action_t> paSet2;

    std::transform(
        paList.cbegin(), paList.cend(), std::inserter(paSet2, paSet2.begin()),
        [](sai_int32_t value) { return static_cast<sai_buffer_profile_packet_admission_fail_action_t>(value); }
    );
    ASSERT_EQ(paSet1, paSet2);
}

TEST_F(SwitchStateBaseTest, switchQoSMaxNumOfTrafficClasses)
{
    ASSERT_EQ(m_ss->set_maximum_number_of_traffic_classes(), SAI_STATUS_SUCCESS);

    sai_attribute_t attr;
    attr.id = SAI_SWITCH_ATTR_QOS_MAX_NUMBER_OF_TRAFFIC_CLASSES;
    ASSERT_EQ(m_ss->get(SAI_OBJECT_TYPE_SWITCH, sai_serialize_object_id(m_swid), 1, &attr), SAI_STATUS_SUCCESS);

    const sai_uint8_t maxTcNum = 16;
    ASSERT_EQ(attr.value.u8, maxTcNum);
}

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

TEST(SwitchStateBase, initialize_voq_switch)
{
    auto sc = std::make_shared<SwitchConfig>(0, "");
    auto scc = std::make_shared<SwitchConfigContainer>();

    SwitchStateBase ss(
            0x2100000000,
            std::make_shared<RealObjectIdManager>(0, scc),
            sc);

    std::vector<sai_attribute_t> attrs;
    sai_attribute_t attr;
    const u_int32_t numSysPorts = 2;
    sai_system_port_config_t sysports[ numSysPorts ] = {
       { .port_id = 0, .attached_switch_id = 2, .attached_core_index = 0,
         .attached_core_port_index = 0, .speed=40000, .num_voq = 8 },
       { .port_id = 1, .attached_switch_id = 2, .attached_core_index = 0,
         .attached_core_port_index = 1, .speed=40000, .num_voq = 8 }
    };

    u_int32_t switchId = 2;
    u_int32_t maxSystemCores = 16;

    attr.id = SAI_SWITCH_ATTR_TYPE;
    attr.value.u32 = SAI_SWITCH_TYPE_VOQ;
    attrs.push_back(attr);

    attr.id = SAI_SWITCH_ATTR_SWITCH_ID;
    attr.value.u32 = switchId;
    attrs.push_back(attr);

    attr.id = SAI_SWITCH_ATTR_MAX_SYSTEM_CORES;
    attr.value.u32 = maxSystemCores;
    attrs.push_back(attr);

    attr.id = SAI_SWITCH_ATTR_SYSTEM_PORT_CONFIG_LIST;
    attr.value.sysportconfiglist.count = numSysPorts;
    attr.value.sysportconfiglist.list = sysports;
    attrs.push_back(attr);

    // Check the result of the initialize_voq_switch_objects
    EXPECT_EQ(SAI_STATUS_SUCCESS,
              ss.initialize_voq_switch_objects((uint32_t)attrs.size(), attrs.data()));
}

TEST(SwitchStateBase, query_stats_st_capability)
{
    auto sc = std::make_shared<SwitchConfig>(0, "");
    auto scc = std::make_shared<SwitchConfigContainer>();

    SwitchStateBase ss(
        0x2100000000,
        std::make_shared<RealObjectIdManager>(0, scc),
        sc);

    sai_stat_st_capability_list_t stats_capability;
    std::vector<sai_stat_st_capability_t> buffer;
    buffer.resize(96);
    stats_capability.count = static_cast<uint32_t>(buffer.size());
    stats_capability.list = buffer.data();

    EXPECT_EQ(SAI_STATUS_SUCCESS,
              ss.queryStatsStCapability(0,
                                        SAI_OBJECT_TYPE_PORT,
                                        &stats_capability));

    EXPECT_EQ(SAI_STATUS_SUCCESS,
              static_cast<SwitchState&>(ss).queryStatsStCapability(0,
                                        SAI_OBJECT_TYPE_PORT,
                                        &stats_capability));
}
