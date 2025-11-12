#include <cstdint>

#include <memory>
#include <vector>
#include <array>

extern "C" {
#include "sai.h"
#include "saimetadatautils.h"
}

#include <gtest/gtest.h>

#include "ContextConfigContainer.h"
#include "VirtualSwitchSaiInterface.h"

using namespace saivs;

class VirtualSwitchSaiInterfaceTest : public ::testing::Test
{
public:
    VirtualSwitchSaiInterfaceTest() = default;
    virtual ~VirtualSwitchSaiInterfaceTest() = default;

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

        m_vssai = std::make_shared<VirtualSwitchSaiInterface>(m_cc);

        sai_attribute_t attr;
        attr.id = SAI_SWITCH_ATTR_INIT_SWITCH;
        attr.value.booldata = true;

        auto status = m_vssai->create(SAI_OBJECT_TYPE_SWITCH, &m_swid, SAI_NULL_OBJECT_ID, 1, &attr);
        ASSERT_EQ(status, SAI_STATUS_SUCCESS);
    }

    virtual void TearDown() override
    {
        auto status = m_vssai->remove(SAI_OBJECT_TYPE_SWITCH, m_swid);
        ASSERT_EQ(status, SAI_STATUS_SUCCESS);
    }

protected:
    std::shared_ptr<ContextConfigContainer> m_ccc;
    std::shared_ptr<ContextConfig> m_cc;
    std::shared_ptr<SwitchConfigContainer> m_scc;
    std::shared_ptr<SwitchConfig> m_sc;
    std::shared_ptr<VirtualSwitchSaiInterface> m_vssai;

    sai_object_id_t m_swid = SAI_NULL_OBJECT_ID;

    const std::uint32_t m_guid = 0; // default context config id
    const std::uint32_t m_scid = 0; // default switch config id
};

TEST_F(VirtualSwitchSaiInterfaceTest, portBulkAddRemove)
{
    const std::uint32_t portCount = 1;
    const std::uint32_t laneCount = 4;

    // Generate port config
    std::array<std::uint32_t, laneCount> laneList = { 0, 1, 2, 3 };

    sai_attribute_t attr;
    std::vector<sai_attribute_t> attrList;

    attr.id = SAI_PORT_ATTR_HW_LANE_LIST;
    attr.value.u32list.count = static_cast<std::uint32_t>(laneList.size());
    attr.value.u32list.list = laneList.data();
    attrList.push_back(attr);

    attr.id = SAI_PORT_ATTR_SPEED;
    attr.value.u32 = 1000;
    attrList.push_back(attr);

    std::array<std::uint32_t, portCount> attrCountList = { static_cast<std::uint32_t>(attrList.size()) };
    std::array<const sai_attribute_t*, portCount> attrPtrList = { attrList.data() };

    std::array<sai_object_id_t, portCount> oidList = { SAI_NULL_OBJECT_ID };
    std::array<sai_status_t, portCount> statusList = { SAI_STATUS_SUCCESS };

    // Validate port bulk add
    auto status = m_vssai->bulkCreate(
        SAI_OBJECT_TYPE_PORT, m_swid, portCount, attrCountList.data(), attrPtrList.data(),
        SAI_BULK_OP_ERROR_MODE_IGNORE_ERROR,
        oidList.data(), statusList.data()
    );
    ASSERT_EQ(status, SAI_STATUS_SUCCESS);

    for (std::uint32_t i = 0; i < portCount; i++)
    {
        ASSERT_EQ(statusList.at(i), SAI_STATUS_SUCCESS);
    }

    // Validate port bulk remove
    status = m_vssai->bulkRemove(
        SAI_OBJECT_TYPE_PORT, portCount, oidList.data(),
        SAI_BULK_OP_ERROR_MODE_IGNORE_ERROR,
        statusList.data()
    );
    ASSERT_EQ(status, SAI_STATUS_SUCCESS);

    for (std::uint32_t i = 0; i < portCount; i++)
    {
        ASSERT_EQ(statusList.at(i), SAI_STATUS_SUCCESS);
    }
}

TEST_F(VirtualSwitchSaiInterfaceTest, queryApiVersion)
{
    sai_api_version_t version;

    EXPECT_EQ(m_vssai->queryApiVersion(NULL), SAI_STATUS_INVALID_PARAMETER);
    EXPECT_EQ(m_vssai->queryApiVersion(&version), SAI_STATUS_SUCCESS);
}

TEST_F(VirtualSwitchSaiInterfaceTest, bulkGet)
{
    sai_attribute_t attr;

    attr.id = SAI_SWITCH_ATTR_PORT_NUMBER;
    EXPECT_EQ(m_vssai->get(SAI_OBJECT_TYPE_SWITCH, m_swid, 1, &attr), SAI_STATUS_SUCCESS);

    auto portNum = attr.value.u32;

    std::vector<sai_object_id_t> oids(portNum);

    attr.id = SAI_SWITCH_ATTR_PORT_LIST;
    attr.value.objlist.count = portNum;
    attr.value.objlist.list = oids.data();
    EXPECT_EQ(m_vssai->get(SAI_OBJECT_TYPE_SWITCH, m_swid, 1, &attr), SAI_STATUS_SUCCESS);

    std::vector<sai_attribute_t> attrs(portNum);
    std::vector<uint32_t> attrCounts(portNum, 1);
    std::vector<sai_status_t> statuses(portNum);
    std::vector<sai_attribute_t*> pattrs(portNum);
    for (size_t i = 0; i < portNum; i++)
    {
        attrs[i].id = SAI_PORT_ATTR_ADMIN_STATE;
        pattrs[i] = &attrs[i];
    }

    EXPECT_EQ(SAI_STATUS_SUCCESS,
            m_vssai->bulkGet(
                SAI_OBJECT_TYPE_PORT,
                portNum,
                oids.data(),
                attrCounts.data(),
                pattrs.data(),
                SAI_BULK_OP_ERROR_MODE_STOP_ON_ERROR,
                statuses.data()));
}

TEST_F(VirtualSwitchSaiInterfaceTest, queryStatsCapability)
{
    std::vector<sai_stat_capability_t> capability_list;
    sai_stat_capability_list_t stats_capability;

    /* Switch stats capability get */
    stats_capability.count = 0;
    stats_capability.list = nullptr;

    EXPECT_EQ(SAI_STATUS_BUFFER_OVERFLOW,
            m_vssai->queryStatsCapability(
                m_swid,
                SAI_OBJECT_TYPE_SWITCH,
                &stats_capability));

    capability_list.resize(stats_capability.count);
    stats_capability.list = capability_list.data();

    EXPECT_EQ(SAI_STATUS_SUCCESS,
            m_vssai->queryStatsCapability(
                m_swid,
                SAI_OBJECT_TYPE_SWITCH,
                &stats_capability));

    /* Queue stats capability get */
    stats_capability.count = 0;
    stats_capability.list = nullptr;

    EXPECT_EQ(SAI_STATUS_BUFFER_OVERFLOW,
            m_vssai->queryStatsCapability(
                m_swid,
                SAI_OBJECT_TYPE_QUEUE,
                &stats_capability));

    capability_list.resize(stats_capability.count);
    stats_capability.list = capability_list.data();

    EXPECT_EQ(SAI_STATUS_SUCCESS,
            m_vssai->queryStatsCapability(
                m_swid,
                SAI_OBJECT_TYPE_QUEUE,
                &stats_capability));

    /* Port stats capability get */
    stats_capability.count = 0;
    stats_capability.list = nullptr;

    EXPECT_EQ(SAI_STATUS_BUFFER_OVERFLOW,
            m_vssai->queryStatsCapability(
                m_swid,
                SAI_OBJECT_TYPE_PORT,
                &stats_capability));

    capability_list.resize(stats_capability.count);
    stats_capability.list = capability_list.data();

    EXPECT_EQ(SAI_STATUS_SUCCESS,
            m_vssai->queryStatsCapability(
                m_swid,
                SAI_OBJECT_TYPE_PORT,
                &stats_capability));
}

TEST_F(VirtualSwitchSaiInterfaceTest, queryStatsStCapability)
{
    std::vector<sai_stat_st_capability_t> capability_list;
    sai_stat_st_capability_list_t stats_capability;

    /* Switch stats capability get */
    stats_capability.count = 0;
    stats_capability.list = nullptr;

    EXPECT_EQ(SAI_STATUS_BUFFER_OVERFLOW,
              m_vssai->queryStatsStCapability(
                  m_swid,
                  SAI_OBJECT_TYPE_SWITCH,
                  &stats_capability));

    capability_list.resize(stats_capability.count);
    stats_capability.list = capability_list.data();

    EXPECT_EQ(SAI_STATUS_SUCCESS,
              m_vssai->queryStatsStCapability(
                  m_swid,
                  SAI_OBJECT_TYPE_SWITCH,
                  &stats_capability));

    /* Queue stats capability get */
    stats_capability.count = 0;
    stats_capability.list = nullptr;

    EXPECT_EQ(SAI_STATUS_BUFFER_OVERFLOW,
              m_vssai->queryStatsStCapability(
                  m_swid,
                  SAI_OBJECT_TYPE_QUEUE,
                  &stats_capability));

    capability_list.resize(stats_capability.count);
    stats_capability.list = capability_list.data();

    EXPECT_EQ(SAI_STATUS_SUCCESS,
              m_vssai->queryStatsStCapability(
                  m_swid,
                  SAI_OBJECT_TYPE_QUEUE,
                  &stats_capability));

    /* Port stats capability get */
    stats_capability.count = 0;
    stats_capability.list = nullptr;

    EXPECT_EQ(SAI_STATUS_BUFFER_OVERFLOW,
              m_vssai->queryStatsStCapability(
                  m_swid,
                  SAI_OBJECT_TYPE_PORT,
                  &stats_capability));

    capability_list.resize(stats_capability.count);
    stats_capability.list = capability_list.data();

    EXPECT_EQ(SAI_STATUS_SUCCESS,
              m_vssai->queryStatsStCapability(
                  m_swid,
                  SAI_OBJECT_TYPE_PORT,
                  &stats_capability));
    EXPECT_EQ(stats_capability.list[0].minimal_polling_interval, static_cast<uint64_t>(1e6 * 100));
}

TEST_F(VirtualSwitchSaiInterfaceTest, switchHostifTrapCapabilityGet)
{
    sai_s32_list_t enum_values_capability = { .count = 0, .list = nullptr };

    const auto* meta = sai_metadata_get_attr_metadata(SAI_OBJECT_TYPE_HOSTIF_TRAP,
        SAI_HOSTIF_TRAP_ATTR_TRAP_TYPE);
    assert(meta != NULL && meta->isenum);

    size_t expected_enum_count = meta->enummetadata->valuescount;

    sai_status_t status = m_vssai->queryAttributeEnumValuesCapability(
        m_swid,
        SAI_OBJECT_TYPE_HOSTIF_TRAP,
        SAI_HOSTIF_TRAP_ATTR_TRAP_TYPE,
        &enum_values_capability);

    ASSERT_EQ(status, SAI_STATUS_BUFFER_OVERFLOW);
    ASSERT_EQ(enum_values_capability.count, expected_enum_count);

    std::vector<int32_t> values_list(expected_enum_count);
    enum_values_capability.count = static_cast<uint32_t>(values_list.size());
    enum_values_capability.list = values_list.data();

    status = m_vssai->queryAttributeEnumValuesCapability(
        m_swid,
        SAI_OBJECT_TYPE_HOSTIF_TRAP,
        SAI_HOSTIF_TRAP_ATTR_TRAP_TYPE,
        &enum_values_capability);

    ASSERT_EQ(status, SAI_STATUS_SUCCESS);
    ASSERT_EQ(enum_values_capability.count, expected_enum_count);

    for (uint32_t i = 0; i < enum_values_capability.count; ++i)
    {
        int32_t value = enum_values_capability.list[i];
        EXPECT_GE(value, SAI_HOSTIF_TRAP_TYPE_START);
        EXPECT_LE(value, SAI_HOSTIF_TRAP_TYPE_END);
    }
}

TEST_F(VirtualSwitchSaiInterfaceTest, switchDebugCounterCapabilityGet)
{
    sai_s32_list_t enum_values_capability = { .count = 0, .list = nullptr };

    // Query the capability for IN_DROP_REASON_LIST with no allocated buffer
    ASSERT_EQ(SAI_STATUS_BUFFER_OVERFLOW,
            m_vssai->queryAttributeEnumValuesCapability(
                m_swid,
                SAI_OBJECT_TYPE_DEBUG_COUNTER,
                SAI_DEBUG_COUNTER_ATTR_IN_DROP_REASON_LIST,
                &enum_values_capability));
    ASSERT_EQ(enum_values_capability.count, 3);

    // Allocate the required buffer and query again
    std::vector<sai_int32_t> haList(enum_values_capability.count);
    enum_values_capability.list = haList.data();

    ASSERT_EQ(SAI_STATUS_SUCCESS,
            m_vssai->queryAttributeEnumValuesCapability(
                m_swid,
                SAI_OBJECT_TYPE_DEBUG_COUNTER,
                SAI_DEBUG_COUNTER_ATTR_IN_DROP_REASON_LIST,
                &enum_values_capability));
    ASSERT_EQ(enum_values_capability.count, 3);

    const std::set<sai_in_drop_reason_t> expectedInDropReasons = {
        SAI_IN_DROP_REASON_L2_ANY,
        SAI_IN_DROP_REASON_L3_ANY,
        SAI_IN_DROP_REASON_ACL_ANY
    };

    // Transform the returned list into a set for comparison
    std::set<sai_in_drop_reason_t> actualInDropReasons;
    std::transform(
        haList.cbegin(), haList.cend(),
        std::inserter(actualInDropReasons, actualInDropReasons.begin()),
        [](sai_int32_t value) { return static_cast<sai_in_drop_reason_t>(value); }
    );

    // Verify the returned values match the expected values
    ASSERT_EQ(expectedInDropReasons, actualInDropReasons);

    // Set count to a smaller value to trigger SAI_STATUS_BUFFER_OVERFLOW
    enum_values_capability.count = 1;

    // Query the capability for OUT_DROP_REASON_LIST
    ASSERT_EQ(SAI_STATUS_BUFFER_OVERFLOW,
            m_vssai->queryAttributeEnumValuesCapability(
                m_swid,
                SAI_OBJECT_TYPE_DEBUG_COUNTER,
                SAI_DEBUG_COUNTER_ATTR_OUT_DROP_REASON_LIST,
                &enum_values_capability));
    ASSERT_EQ(enum_values_capability.count, 2);

    // Resize the buffer and query again
    haList.resize(enum_values_capability.count);
    enum_values_capability.list = haList.data();

    ASSERT_EQ(SAI_STATUS_SUCCESS,
            m_vssai->queryAttributeEnumValuesCapability(
                m_swid,
                SAI_OBJECT_TYPE_DEBUG_COUNTER,
                SAI_DEBUG_COUNTER_ATTR_OUT_DROP_REASON_LIST,
                &enum_values_capability));
    ASSERT_EQ(enum_values_capability.count, 2);

    const std::set<sai_out_drop_reason_t> expectedOutDropReasons = {
        SAI_OUT_DROP_REASON_L2_ANY,
        SAI_OUT_DROP_REASON_L3_ANY
    };

    std::set<sai_out_drop_reason_t> actualOutDropReasons;
    std::transform(
        haList.cbegin(), haList.cend(),
        std::inserter(actualOutDropReasons, actualOutDropReasons.begin()),
        [](sai_int32_t value) { return static_cast<sai_out_drop_reason_t>(value); }
    );
    ASSERT_EQ(expectedOutDropReasons, actualOutDropReasons);

    // Query the capability for DEBUG_COUNTER_ATTR_TYPE
    enum_values_capability.count = 3;
    ASSERT_EQ(SAI_STATUS_BUFFER_OVERFLOW,
            m_vssai->queryAttributeEnumValuesCapability(
                m_swid,
                SAI_OBJECT_TYPE_DEBUG_COUNTER,
                SAI_DEBUG_COUNTER_ATTR_TYPE,
                &enum_values_capability));
    ASSERT_EQ(enum_values_capability.count, 4);

    haList.resize(enum_values_capability.count);
    enum_values_capability.list = haList.data();

    ASSERT_EQ(SAI_STATUS_SUCCESS,
            m_vssai->queryAttributeEnumValuesCapability(
                m_swid,
                SAI_OBJECT_TYPE_DEBUG_COUNTER,
                SAI_DEBUG_COUNTER_ATTR_TYPE,
                &enum_values_capability));

    // Verify the required count is 4
    ASSERT_EQ(enum_values_capability.count, 4);

    // Define the expected DEBUG_COUNTER_ATTR_TYPE values
    const std::set<sai_debug_counter_type_t> expectedDebugCounterTypes = {
        SAI_DEBUG_COUNTER_TYPE_PORT_IN_DROP_REASONS,
        SAI_DEBUG_COUNTER_TYPE_PORT_OUT_DROP_REASONS,
        SAI_DEBUG_COUNTER_TYPE_SWITCH_IN_DROP_REASONS,
        SAI_DEBUG_COUNTER_TYPE_SWITCH_OUT_DROP_REASONS
    };

    std::set<sai_debug_counter_type_t> actualDebugCounterTypes;
    std::transform(
        haList.cbegin(), haList.cend(),
        std::inserter(actualDebugCounterTypes, actualDebugCounterTypes.begin()),
        [](sai_int32_t value) { return static_cast<sai_debug_counter_type_t>(value); }
    );
    ASSERT_EQ(expectedDebugCounterTypes, actualDebugCounterTypes);
}

TEST_F(VirtualSwitchSaiInterfaceTest, objectTypeGetAvailability_MySidEntry)
{
    uint64_t count = 0;

    // Test objectTypeGetAvailability for SAI_OBJECT_TYPE_MY_SID_ENTRY
    sai_status_t status = m_vssai->objectTypeGetAvailability(
        m_swid,
        SAI_OBJECT_TYPE_MY_SID_ENTRY,
        0, // attr_count
        nullptr, // attr_list
        &count);

    EXPECT_EQ(status, SAI_STATUS_SUCCESS);
    // The availability should be returned based on the switch state implementation
    // For base implementation, it should return 0
    EXPECT_EQ(count, 0);
}

TEST_F(VirtualSwitchSaiInterfaceTest, objectTypeGetAvailability_MySidEntry_InvalidSwitch)
{
    uint64_t count = 0;
    sai_object_id_t invalid_switch_id = 0x123456789;

    // Test with invalid switch ID - should return failure
    sai_status_t status = m_vssai->objectTypeGetAvailability(
        invalid_switch_id,
        SAI_OBJECT_TYPE_MY_SID_ENTRY,
        0,
        nullptr,
        &count);

    EXPECT_EQ(status, SAI_STATUS_FAILURE);
}
