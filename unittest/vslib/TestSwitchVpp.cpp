#include "MockVppXlate.h"
#include "vpp/vppxlate/SaiIntfStats.h"
#include "ContextConfigContainer.h"
#include "meta/DummySaiInterface.h"
#include "meta/Meta.h"
#include "meta/sai_serialize.h"
#include "saivs.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <deque>
#include <map>
#include <memory>
#include <string>
#include <set>
#include <vector>
#include <unistd.h>

using namespace saivs;

namespace
{
    struct GlobalTrimCall
    {
        bool enabled;
        uint16_t size;
        uint8_t dscp;
        uint8_t tc;
        uint8_t queue;
    };

    struct QueueTrimCall
    {
        uint8_t queue;
        bool eligible;
        uint64_t rate;
        uint64_t capacity;
    };

    std::vector<GlobalTrimCall> global_calls;
    std::vector<QueueTrimCall> queue_calls;
    std::vector<std::array<uint8_t, 64>> map_calls;
    std::deque<int> global_results;
    std::deque<int> map_results;

    int nextResult(std::deque<int> &results)
    {
        if (results.empty())
        {
            return 0;
        }
        int result = results.front();
        results.pop_front();
        return result;
    }
}

extern "C" int __wrap_vpp_sonic_ext_trim_global_set(
        bool enabled, uint16_t size, uint8_t, uint8_t dscp, uint8_t tc, uint8_t queue)
{
    global_calls.push_back({enabled, size, dscp, tc, queue});
    return nextResult(global_results);
}

extern "C" int __wrap_vpp_sonic_ext_trim_queue_set(
        const char *, uint8_t queue, bool eligible, uint64_t rate, uint64_t capacity)
{
    queue_calls.push_back({queue, eligible, rate, capacity});
    return 0;
}

extern "C" int __wrap_vpp_sonic_ext_trim_dscp_map_set(const uint8_t map[64])
{
    std::array<uint8_t, 64> copy;
    std::copy(map, map + copy.size(), copy.begin());
    map_calls.push_back(copy);
    return nextResult(map_results);
}

TEST(SwitchVpp, getLagMemberEgressDisableAction)
{
    using Action = SwitchVpp::LagMemberEgressDisableAction;

    EXPECT_EQ(Action::NONE, SwitchVpp::getLagMemberEgressDisableAction(false, true, false));
    EXPECT_EQ(Action::DISABLE, SwitchVpp::getLagMemberEgressDisableAction(true, true, false));
    EXPECT_EQ(Action::ENABLE, SwitchVpp::getLagMemberEgressDisableAction(false, true, true));
    EXPECT_EQ(Action::NONE, SwitchVpp::getLagMemberEgressDisableAction(true, true, true));
    EXPECT_EQ(Action::NONE, SwitchVpp::getLagMemberEgressDisableAction(false, false, false));
    EXPECT_EQ(Action::DISABLE, SwitchVpp::getLagMemberEgressDisableAction(true, false, false));
}

TEST(SwitchVpp, getTrimEnumValuesCapability)
{
    sai_int32_t value = -1;

    // The DSCP resolution mode advertises only DSCP_VALUE (symmetric trim); the
    // asymmetric FROM_TC mode is intentionally not offered on VPP.
    ASSERT_TRUE(SwitchVpp::getTrimEnumValuesCapability(
        SAI_OBJECT_TYPE_SWITCH, SAI_SWITCH_ATTR_PACKET_TRIM_DSCP_RESOLUTION_MODE, &value));
    EXPECT_EQ((sai_int32_t)SAI_PACKET_TRIM_DSCP_RESOLUTION_MODE_DSCP_VALUE, value);

    // The queue resolution mode advertises only STATIC.
    value = -1;
    ASSERT_TRUE(SwitchVpp::getTrimEnumValuesCapability(
        SAI_OBJECT_TYPE_SWITCH, SAI_SWITCH_ATTR_PACKET_TRIM_QUEUE_RESOLUTION_MODE, &value));
    EXPECT_EQ((sai_int32_t)SAI_PACKET_TRIM_QUEUE_RESOLUTION_MODE_STATIC, value);

    // Unrelated switch enum attributes fall through to the base metadata.
    EXPECT_FALSE(SwitchVpp::getTrimEnumValuesCapability(
        SAI_OBJECT_TYPE_SWITCH, SAI_SWITCH_ATTR_ECMP_HASH, &value));

    // A trim switch attr queried against the wrong object type must not match.
    EXPECT_FALSE(SwitchVpp::getTrimEnumValuesCapability(
        SAI_OBJECT_TYPE_BUFFER_PROFILE, SAI_SWITCH_ATTR_PACKET_TRIM_DSCP_RESOLUTION_MODE, &value));

    // A null out-pointer is tolerated (predicate still resolves).
    EXPECT_TRUE(SwitchVpp::getTrimEnumValuesCapability(
        SAI_OBJECT_TYPE_SWITCH, SAI_SWITCH_ATTR_PACKET_TRIM_QUEUE_RESOLUTION_MODE, nullptr));
}

TEST(SwitchVpp, isTrimDataplaneAttr)
{
    // Queue buffer-profile / scheduler bindings drive per-queue admission.
    EXPECT_TRUE(SwitchVpp::isTrimDataplaneAttr(
        SAI_OBJECT_TYPE_QUEUE, SAI_QUEUE_ATTR_BUFFER_PROFILE_ID));
    EXPECT_TRUE(SwitchVpp::isTrimDataplaneAttr(
        SAI_OBJECT_TYPE_QUEUE, SAI_QUEUE_ATTR_SCHEDULER_PROFILE_ID));
    EXPECT_TRUE(SwitchVpp::isTrimDataplaneAttr(
        SAI_OBJECT_TYPE_BUFFER_PROFILE, SAI_BUFFER_PROFILE_ATTR_PACKET_ADMISSION_FAIL_ACTION));
    EXPECT_TRUE(SwitchVpp::isTrimDataplaneAttr(
        SAI_OBJECT_TYPE_SCHEDULER_GROUP, SAI_SCHEDULER_GROUP_ATTR_SCHEDULER_PROFILE_ID));
    EXPECT_TRUE(SwitchVpp::isTrimDataplaneAttr(
        SAI_OBJECT_TYPE_SCHEDULER, SAI_SCHEDULER_ATTR_MAX_BANDWIDTH_RATE));

    // Port QoS-map (re)bindings and in-place QoS-map edits re-resolve the
    // switch-global DSCP->queue trim table.
    EXPECT_TRUE(SwitchVpp::isTrimDataplaneAttr(
        SAI_OBJECT_TYPE_PORT, SAI_PORT_ATTR_QOS_DSCP_TO_TC_MAP));
    EXPECT_TRUE(SwitchVpp::isTrimDataplaneAttr(
        SAI_OBJECT_TYPE_PORT, SAI_PORT_ATTR_QOS_TC_TO_QUEUE_MAP));
    EXPECT_TRUE(SwitchVpp::isTrimDataplaneAttr(
        SAI_OBJECT_TYPE_QOS_MAP, SAI_QOS_MAP_ATTR_MAP_TO_VALUE_LIST));

    // Unrelated attributes must not trigger a trim refresh.
    EXPECT_FALSE(SwitchVpp::isTrimDataplaneAttr(
        SAI_OBJECT_TYPE_QUEUE, SAI_QUEUE_ATTR_TYPE));
    EXPECT_FALSE(SwitchVpp::isTrimDataplaneAttr(
        SAI_OBJECT_TYPE_PORT, SAI_PORT_ATTR_MTU));
    EXPECT_TRUE(SwitchVpp::isTrimDataplaneAttr(
        SAI_OBJECT_TYPE_SWITCH, SAI_SWITCH_ATTR_PACKET_TRIM_SIZE));
}

class SwitchVppTrimTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        vppMock().reset();
        auto context = ContextConfigContainer::getDefault()->get(0);
        config = context->m_scc->getConfig(0);
        config->m_useTapDevice = false;
        config->m_profileMap[SAI_KEY_VS_PORT_CONFIG_FILE] = "/dev/null";
        manager = std::make_shared<RealObjectIdManager>(context->m_guid, context->m_scc);
        switch_id = manager->allocateNewSwitchObjectId("");
        state = std::make_shared<SwitchVpp>(switch_id, manager, config);
        stopEvents(state);
        global_calls.clear();
        queue_calls.clear();
        map_calls.clear();
        global_results.clear();
        map_results.clear();
        vppMock().interfaceLookups = 0;
    }

    static void stopEvents(const std::shared_ptr<SwitchVpp> &sw)
    {
        sw->m_run_vpp_events_thread = false;
        sw->m_vpp_thread->join();
        sw->m_operResyncDue.store(false);
    }

    sai_object_id_t object(sai_object_type_t type)
    {
        auto oid = manager->allocateNewObjectId(type, switch_id);
        state->m_objectHash[type][sai_serialize_object_id(oid)] = {};
        return oid;
    }

    void put(sai_object_type_t type, sai_object_id_t oid, sai_attribute_t attr)
    {
        auto wrap = std::make_shared<SaiAttrWrap>(type, &attr);
        state->m_objectHash[type][sai_serialize_object_id(oid)][
                wrap->getAttrMetadata()->attridname] = wrap;
    }

    static sai_attribute_t oidAttr(sai_attr_id_t id, sai_object_id_t value)
    {
        sai_attribute_t attr = {};
        attr.id = id;
        attr.value.oid = value;
        return attr;
    }

    sai_status_t setSize(uint32_t value)
    {
        sai_attribute_t attr = {};
        attr.id = SAI_SWITCH_ATTR_PACKET_TRIM_SIZE;
        attr.value.u32 = value;
        return state->set(SAI_OBJECT_TYPE_SWITCH, sai_serialize_object_id(switch_id), &attr);
    }

    sai_status_t setSwitchByte(sai_attr_id_t id, uint8_t value)
    {
        sai_attribute_t attr = {};
        attr.id = id;
        attr.value.u8 = value;
        return state->set(SAI_OBJECT_TYPE_SWITCH, sai_serialize_object_id(switch_id), &attr);
    }

    sai_status_t setOid(sai_object_type_t type, sai_object_id_t oid,
            sai_attr_id_t id, sai_object_id_t value)
    {
        auto attr = oidAttr(id, value);
        return state->set_internal(type, sai_serialize_object_id(oid), &attr);
    }

    sai_object_id_t port(const std::vector<sai_object_id_t> &queues = {})
    {
        auto oid = object(SAI_OBJECT_TYPE_PORT);
        state->m_ifaceRegistry.addPhysicalPort("pg0", "Ethernet0", oid);
        sai_attribute_t attr = {};
        attr.id = SAI_PORT_ATTR_TYPE;
        attr.value.s32 = SAI_PORT_TYPE_LOGICAL;
        put(SAI_OBJECT_TYPE_PORT, oid, attr);
        attr.id = SAI_PORT_ATTR_QOS_QUEUE_LIST;
        std::vector<sai_object_id_t> copy = queues;
        attr.value.objlist.count = static_cast<uint32_t>(copy.size());
        attr.value.objlist.list = copy.data();
        put(SAI_OBJECT_TYPE_PORT, oid, attr);
        return oid;
    }

    void qosMaps(sai_object_id_t &port_id, sai_object_id_t &dscp_map,
            sai_object_id_t &queue_map)
    {
        port_id = port();
        dscp_map = object(SAI_OBJECT_TYPE_QOS_MAP);
        queue_map = object(SAI_OBJECT_TYPE_QOS_MAP);
        sai_qos_map_t dscp_entry = {};
        dscp_entry.key.dscp = 10;
        dscp_entry.value.tc = 2;
        sai_attribute_t attr = {};
        attr.id = SAI_QOS_MAP_ATTR_MAP_TO_VALUE_LIST;
        attr.value.qosmap = {1, &dscp_entry};
        put(SAI_OBJECT_TYPE_QOS_MAP, dscp_map, attr);
        sai_qos_map_t queue_entries[2] = {};
        queue_entries[0].key.tc = 2;
        queue_entries[0].value.queue_index = 5;
        queue_entries[1].key.tc = 3;
        queue_entries[1].value.queue_index = 6;
        attr.value.qosmap = {2, queue_entries};
        put(SAI_OBJECT_TYPE_QOS_MAP, queue_map, attr);
        put(SAI_OBJECT_TYPE_PORT, port_id, oidAttr(SAI_PORT_ATTR_QOS_DSCP_TO_TC_MAP, dscp_map));
        put(SAI_OBJECT_TYPE_PORT, port_id, oidAttr(SAI_PORT_ATTR_QOS_TC_TO_QUEUE_MAP, queue_map));
        attr = {};
        attr.id = SAI_PORT_ATTR_QOS_DEFAULT_TC;
        attr.value.u8 = 3;
        put(SAI_OBJECT_TYPE_PORT, port_id, attr);
    }

    std::shared_ptr<SwitchConfig> config;
    std::shared_ptr<RealObjectIdManager> manager;
    std::shared_ptr<SwitchVpp> state;
    sai_object_id_t switch_id;
};

TEST_F(SwitchVppTrimTest, createAccumulatesPolicyBeforeDscpOnlySet)
{
    sai_attribute_t attrs[5] = {};
    attrs[0].id = SAI_SWITCH_ATTR_PACKET_TRIM_SIZE;
    attrs[0].value.u32 = 256;
    attrs[1].id = SAI_SWITCH_ATTR_PACKET_TRIM_DSCP_VALUE;
    attrs[1].value.u8 = 48;
    attrs[2].id = SAI_SWITCH_ATTR_PACKET_TRIM_TC_VALUE;
    attrs[2].value.u8 = 2;
    attrs[3].id = SAI_SWITCH_ATTR_PACKET_TRIM_QUEUE_INDEX;
    attrs[3].value.u8 = 6;
    attrs[4].id = SAI_SWITCH_ATTR_PACKET_TRIM_QUEUE_RESOLUTION_MODE;
    attrs[4].value.s32 = SAI_PACKET_TRIM_QUEUE_RESOLUTION_MODE_STATIC;
    ASSERT_EQ(SAI_STATUS_SUCCESS, state->create(SAI_OBJECT_TYPE_SWITCH,
            sai_serialize_object_id(switch_id), switch_id, 5, attrs));
    ASSERT_EQ(1u, global_calls.size());
    EXPECT_TRUE(global_calls.back().enabled);
    EXPECT_EQ(256u, global_calls.back().size);
    EXPECT_EQ(48u, global_calls.back().dscp);
    EXPECT_EQ(2u, global_calls.back().tc);
    EXPECT_EQ(6u, global_calls.back().queue);
    ASSERT_EQ(SAI_STATUS_SUCCESS, setSwitchByte(SAI_SWITCH_ATTR_PACKET_TRIM_DSCP_VALUE, 49));
    EXPECT_EQ(256u, global_calls.back().size);
    EXPECT_EQ(49u, global_calls.back().dscp);
}

TEST_F(SwitchVppTrimTest, invalidCreatePreservesAttributesWithoutProgramming)
{
    const auto before = state->m_objectHash.at(SAI_OBJECT_TYPE_SWITCH).at(
            sai_serialize_object_id(switch_id));
    sai_attribute_t attrs[2] = {};
    attrs[0].id = SAI_SWITCH_ATTR_PACKET_TRIM_DSCP_VALUE;
    attrs[0].value.u8 = 48;
    attrs[1].id = SAI_SWITCH_ATTR_PACKET_TRIM_SIZE;
    attrs[1].value.u32 = 65536;
    EXPECT_EQ(SAI_STATUS_INVALID_ATTR_VALUE_0 - 1, state->create(SAI_OBJECT_TYPE_SWITCH,
            sai_serialize_object_id(switch_id), switch_id, 2, attrs));
    EXPECT_TRUE(global_calls.empty());
    EXPECT_FALSE(state->m_trim_policy.enabled);
    EXPECT_EQ(before, state->m_objectHash.at(SAI_OBJECT_TYPE_SWITCH).at(
            sai_serialize_object_id(switch_id)));
}

TEST_F(SwitchVppTrimTest, failedCreateRestoresPartialGlobalProgramming)
{
    const auto before = state->m_objectHash.at(SAI_OBJECT_TYPE_SWITCH).at(
            sai_serialize_object_id(switch_id));
    sai_attribute_t attr = {};
    attr.id = SAI_SWITCH_ATTR_PACKET_TRIM_SIZE;
    attr.value.u32 = 256;
    map_results = {-1};
    EXPECT_EQ(SAI_STATUS_FAILURE, state->create(SAI_OBJECT_TYPE_SWITCH,
            sai_serialize_object_id(switch_id), switch_id, 1, &attr));
    ASSERT_EQ(2u, global_calls.size());
    EXPECT_TRUE(global_calls.front().enabled);
    EXPECT_FALSE(global_calls.back().enabled);
    EXPECT_FALSE(state->m_trim_policy.enabled);
    EXPECT_FALSE(state->m_trim_reconcile_pending);
    EXPECT_EQ(before, state->m_objectHash.at(SAI_OBJECT_TYPE_SWITCH).at(
            sai_serialize_object_id(switch_id)));
    EXPECT_EQ(SAI_STATUS_SUCCESS, state->create(SAI_OBJECT_TYPE_SWITCH,
            sai_serialize_object_id(switch_id), switch_id, 1, &attr));
}

TEST_F(SwitchVppTrimTest, dynamicModeIsRejectedOnCreateAndSet)
{
    sai_attribute_t attr = {};
    attr.id = SAI_SWITCH_ATTR_PACKET_TRIM_QUEUE_RESOLUTION_MODE;
    attr.value.s32 = SAI_PACKET_TRIM_QUEUE_RESOLUTION_MODE_DYNAMIC;
    EXPECT_EQ(SAI_STATUS_NOT_SUPPORTED, state->create(SAI_OBJECT_TYPE_SWITCH,
            sai_serialize_object_id(switch_id), switch_id, 1, &attr));
    EXPECT_TRUE(global_calls.empty());
    attr.value.s32 = SAI_PACKET_TRIM_QUEUE_RESOLUTION_MODE_STATIC;
    ASSERT_EQ(SAI_STATUS_SUCCESS, state->set(SAI_OBJECT_TYPE_SWITCH,
            sai_serialize_object_id(switch_id), &attr));
    size_t calls = global_calls.size();
    attr.value.s32 = SAI_PACKET_TRIM_QUEUE_RESOLUTION_MODE_DYNAMIC;
    EXPECT_EQ(SAI_STATUS_NOT_SUPPORTED, state->set(SAI_OBJECT_TYPE_SWITCH,
            sai_serialize_object_id(switch_id), &attr));
    EXPECT_EQ(calls, global_calls.size());
    ASSERT_EQ(SAI_STATUS_SUCCESS, state->get(SAI_OBJECT_TYPE_SWITCH, switch_id, 1, &attr));
    EXPECT_EQ(SAI_PACKET_TRIM_QUEUE_RESOLUTION_MODE_STATIC, attr.value.s32);
}

TEST_F(SwitchVppTrimTest, freshCreateResetsPolicyAfterDiscardedFailedRollback)
{
    sai_attribute_t attr = {};
    attr.id = SAI_SWITCH_ATTR_PACKET_TRIM_SIZE;
    attr.value.u32 = 256;
    global_results = {0, -1};
    map_results = {-1};
    ASSERT_EQ(SAI_STATUS_FAILURE, state->create(SAI_OBJECT_TYPE_SWITCH,
            sai_serialize_object_id(switch_id), switch_id, 1, &attr));
    ASSERT_TRUE(state->m_trim_reconcile_pending);
    state.reset();
    state = std::make_shared<SwitchVpp>(switch_id, manager, config);
    stopEvents(state);
    ASSERT_FALSE(state->m_trim_reconcile_pending);
    size_t calls = global_calls.size();
    ASSERT_EQ(SAI_STATUS_SUCCESS, state->create(SAI_OBJECT_TYPE_SWITCH,
            sai_serialize_object_id(switch_id), switch_id, 0, nullptr));
    ASSERT_EQ(calls + 1, global_calls.size());
    EXPECT_FALSE(global_calls.back().enabled);
}

TEST_F(SwitchVppTrimTest, defaultCreateFailsIfDataplaneResetIsUnavailable)
{
    global_results = {-1, -1};
    EXPECT_EQ(SAI_STATUS_FAILURE, state->create(SAI_OBJECT_TYPE_SWITCH,
            sai_serialize_object_id(switch_id), switch_id, 0, nullptr));
    EXPECT_TRUE(state->m_trim_reconcile_pending);
    EXPECT_EQ(SAI_STATUS_SUCCESS, state->create(SAI_OBJECT_TYPE_SWITCH,
            sai_serialize_object_id(switch_id), switch_id, 0, nullptr));
    EXPECT_FALSE(global_calls.back().enabled);
    EXPECT_FALSE(state->m_trim_reconcile_pending);
}

TEST_F(SwitchVppTrimTest, fromTcModeIsRejectedOnCreateAndSet)
{
    sai_attribute_t attr = {};
    attr.id = SAI_SWITCH_ATTR_PACKET_TRIM_DSCP_RESOLUTION_MODE;
    attr.value.s32 = SAI_PACKET_TRIM_DSCP_RESOLUTION_MODE_FROM_TC;
    EXPECT_EQ(SAI_STATUS_NOT_SUPPORTED, state->create(SAI_OBJECT_TYPE_SWITCH,
            sai_serialize_object_id(switch_id), switch_id, 1, &attr));
    EXPECT_TRUE(global_calls.empty());
    attr.value.s32 = SAI_PACKET_TRIM_DSCP_RESOLUTION_MODE_DSCP_VALUE;
    ASSERT_EQ(SAI_STATUS_SUCCESS, state->set(SAI_OBJECT_TYPE_SWITCH,
            sai_serialize_object_id(switch_id), &attr));
    size_t calls = global_calls.size();
    attr.value.s32 = SAI_PACKET_TRIM_DSCP_RESOLUTION_MODE_FROM_TC;
    EXPECT_EQ(SAI_STATUS_NOT_SUPPORTED, state->set(SAI_OBJECT_TYPE_SWITCH,
            sai_serialize_object_id(switch_id), &attr));
    EXPECT_EQ(calls, global_calls.size());
    ASSERT_EQ(SAI_STATUS_SUCCESS, state->get(SAI_OBJECT_TYPE_SWITCH, switch_id, 1, &attr));
    EXPECT_EQ(SAI_PACKET_TRIM_DSCP_RESOLUTION_MODE_DSCP_VALUE, attr.value.s32);
}

TEST_F(SwitchVppTrimTest, unresolvedFrontPanelPortIsResolvedAndInternalPortsAreExcluded)
{
    sai_object_id_t port_id, dscp_map, queue_map;
    qosMaps(port_id, dscp_map, queue_map);
    state->m_ifaceRegistry.removeByOid(port_id);
    state->m_ifaceRegistry.m_ifMapLoaded = true;
    state->m_ifaceRegistry.m_osIfToHwIf["Ethernet0"] = "pg0";
    ASSERT_TRUE(state->m_portConfigMap->add({0}, "Ethernet0"));
    uint32_t lane = 0;
    sai_attribute_t attr = {};
    attr.id = SAI_PORT_ATTR_HW_LANE_LIST;
    attr.value.u32list = {1, &lane};
    put(SAI_OBJECT_TYPE_PORT, port_id, attr);
    for (auto type : {SAI_PORT_TYPE_CPU, SAI_PORT_TYPE_FABRIC, SAI_PORT_TYPE_RECYCLE})
    {
        auto internal = object(SAI_OBJECT_TYPE_PORT);
        attr = {};
        attr.id = SAI_PORT_ATTR_TYPE;
        attr.value.s32 = type;
        put(SAI_OBJECT_TYPE_PORT, internal, attr);
    }
    ASSERT_EQ(SAI_STATUS_SUCCESS, setSize(256));
    EXPECT_EQ("pg0", state->m_ifaceRegistry.resolveHwIfName(port_id, 0));
    EXPECT_EQ(1u, vppMock().interfaceLookups);
    EXPECT_EQ(5u, map_calls.back()[10]);
}

TEST_F(SwitchVppTrimTest, createdPortsExposeTheirStoredType)
{
    auto port_id = manager->allocateNewObjectId(SAI_OBJECT_TYPE_PORT, switch_id);
    ASSERT_EQ(SAI_STATUS_SUCCESS, state->createPort(port_id, switch_id, 0, nullptr));
    sai_attribute_t attr = {};
    attr.id = SAI_PORT_ATTR_TYPE;
    ASSERT_EQ(SAI_STATUS_SUCCESS, state->get(SAI_OBJECT_TYPE_PORT, port_id, 1, &attr));
    EXPECT_EQ(SAI_PORT_TYPE_LOGICAL, attr.value.s32);

    auto internal_id = manager->allocateNewObjectId(SAI_OBJECT_TYPE_PORT, switch_id);
    attr.value.s32 = SAI_PORT_TYPE_FABRIC;
    ASSERT_EQ(SAI_STATUS_SUCCESS, state->createPort(internal_id, switch_id, 1, &attr));
    attr.value.s32 = -1;
    ASSERT_EQ(SAI_STATUS_SUCCESS, state->get(SAI_OBJECT_TYPE_PORT, internal_id, 1, &attr));
    EXPECT_EQ(SAI_PORT_TYPE_FABRIC, attr.value.s32);
}

TEST_F(SwitchVppTrimTest, missingFrontPanelMappingFailsInsteadOfSilentlySkipping)
{
    auto port_id = object(SAI_OBJECT_TYPE_PORT);
    sai_attribute_t attr = {};
    attr.id = SAI_PORT_ATTR_TYPE;
    attr.value.s32 = SAI_PORT_TYPE_LOGICAL;
    put(SAI_OBJECT_TYPE_PORT, port_id, attr);
    EXPECT_EQ(SAI_STATUS_FAILURE, setSize(256));
    EXPECT_FALSE(state->m_trim_policy.enabled);
    EXPECT_TRUE(map_calls.empty());
}

TEST_F(SwitchVppTrimTest, sizeValidationPreservesCommittedPolicy)
{
    for (uint32_t size : {0u, 256u, 65535u})
    {
        ASSERT_EQ(SAI_STATUS_SUCCESS, setSize(size));
        EXPECT_EQ(size, global_calls.back().size);
    }
    size_t calls = global_calls.size();
    for (uint32_t size : {65536u, 65792u})
    {
        EXPECT_EQ(SAI_STATUS_INVALID_ATTR_VALUE_0, setSize(size));
        EXPECT_EQ(calls, global_calls.size());
        EXPECT_EQ(65535u, state->m_trim_policy.trim_size);
        sai_attribute_t attr = {};
        attr.id = SAI_SWITCH_ATTR_PACKET_TRIM_SIZE;
        ASSERT_EQ(SAI_STATUS_SUCCESS, state->get(SAI_OBJECT_TYPE_SWITCH, switch_id, 1, &attr));
        EXPECT_EQ(65535u, attr.value.u32);
    }
}

TEST_F(SwitchVppTrimTest, failedDisableAndPartialGlobalUpdateRestorePolicy)
{
    ASSERT_EQ(SAI_STATUS_SUCCESS, setSize(256));
    global_results = {-99, 0};
    EXPECT_EQ(SAI_STATUS_FAILURE, setSize(0));
    EXPECT_TRUE(state->m_trim_policy.enabled);
    EXPECT_EQ(256u, state->m_trim_policy.trim_size);
    EXPECT_TRUE(global_calls.back().enabled);
    map_results = {-1, 0};
    EXPECT_EQ(SAI_STATUS_FAILURE, setSwitchByte(SAI_SWITCH_ATTR_PACKET_TRIM_DSCP_VALUE, 48));
    EXPECT_EQ(0u, state->m_trim_policy.dscp_value);
    EXPECT_EQ(0u, global_calls.back().dscp);
    EXPECT_FALSE(state->m_trim_reconcile_pending);
}

TEST_F(SwitchVppTrimTest, failedQueueRebindRestoresAttributeAndPartialDataplane)
{
    auto old_profile = object(SAI_OBJECT_TYPE_BUFFER_PROFILE);
    auto new_profile = object(SAI_OBJECT_TYPE_BUFFER_PROFILE);
    sai_attribute_t action = {};
    action.id = SAI_BUFFER_PROFILE_ATTR_PACKET_ADMISSION_FAIL_ACTION;
    action.value.s32 = SAI_BUFFER_PROFILE_PACKET_ADMISSION_FAIL_ACTION_DROP_AND_TRIM;
    put(SAI_OBJECT_TYPE_BUFFER_PROFILE, old_profile, action);
    auto queue = object(SAI_OBJECT_TYPE_QUEUE);
    put(SAI_OBJECT_TYPE_QUEUE, queue, oidAttr(SAI_QUEUE_ATTR_BUFFER_PROFILE_ID, old_profile));
    sai_attribute_t attr = {};
    attr.id = SAI_QUEUE_ATTR_TYPE;
    attr.value.s32 = SAI_QUEUE_TYPE_UNICAST;
    put(SAI_OBJECT_TYPE_QUEUE, queue, attr);
    attr = {};
    attr.id = SAI_QUEUE_ATTR_INDEX;
    attr.value.u8 = 1;
    put(SAI_OBJECT_TYPE_QUEUE, queue, attr);
    port({queue});
    ASSERT_EQ(SAI_STATUS_SUCCESS, setSize(256));
    queue_calls.clear();
    map_results = {-1, 0};
    EXPECT_EQ(SAI_STATUS_FAILURE,
            setOid(SAI_OBJECT_TYPE_QUEUE, queue, SAI_QUEUE_ATTR_BUFFER_PROFILE_ID, new_profile));
    ASSERT_GE(queue_calls.size(), 2u);
    EXPECT_FALSE(queue_calls.front().eligible);
    EXPECT_TRUE(queue_calls.back().eligible);
    attr = {};
    attr.id = SAI_QUEUE_ATTR_BUFFER_PROFILE_ID;
    ASSERT_EQ(SAI_STATUS_SUCCESS, state->get(SAI_OBJECT_TYPE_QUEUE, queue, 1, &attr));
    EXPECT_EQ(old_profile, attr.value.oid);
    EXPECT_EQ(SAI_STATUS_SUCCESS,
            setOid(SAI_OBJECT_TYPE_QUEUE, queue, SAI_QUEUE_ATTR_BUFFER_PROFILE_ID, new_profile));
}

TEST_F(SwitchVppTrimTest, binarySchedulerAdmissionUsesEffectiveBinding)
{
    auto queue = object(SAI_OBJECT_TYPE_QUEUE);
    sai_attribute_t attr = {};
    attr.id = SAI_QUEUE_ATTR_TYPE;
    attr.value.s32 = SAI_QUEUE_TYPE_UNICAST;
    put(SAI_OBJECT_TYPE_QUEUE, queue, attr);
    attr.id = SAI_QUEUE_ATTR_INDEX;
    attr.value.u8 = 1;
    put(SAI_OBJECT_TYPE_QUEUE, queue, attr);
    auto profile = object(SAI_OBJECT_TYPE_BUFFER_PROFILE);
    attr.id = SAI_BUFFER_PROFILE_ATTR_PACKET_ADMISSION_FAIL_ACTION;
    attr.value.s32 = SAI_BUFFER_PROFILE_PACKET_ADMISSION_FAIL_ACTION_DROP_AND_TRIM;
    put(SAI_OBJECT_TYPE_BUFFER_PROFILE, profile, attr);
    put(SAI_OBJECT_TYPE_QUEUE, queue, oidAttr(SAI_QUEUE_ATTR_BUFFER_PROFILE_ID, profile));
    port({queue});
    ASSERT_EQ(SAI_STATUS_SUCCESS, setSize(256));
    auto expectBucket = [](uint64_t expected) {
        ASSERT_FALSE(queue_calls.empty());
        EXPECT_TRUE(queue_calls.back().eligible);
        EXPECT_EQ(expected, queue_calls.back().rate);
        EXPECT_EQ(expected, queue_calls.back().capacity);
    };
    expectBucket(UINT64_MAX);

    auto direct = object(SAI_OBJECT_TYPE_SCHEDULER);
    attr = {};
    attr.id = SAI_SCHEDULER_ATTR_MAX_BANDWIDTH_RATE;
    attr.value.u64 = 1;
    put(SAI_OBJECT_TYPE_SCHEDULER, direct, attr);
    ASSERT_EQ(SAI_STATUS_SUCCESS,
            setOid(SAI_OBJECT_TYPE_QUEUE, queue, SAI_QUEUE_ATTR_SCHEDULER_PROFILE_ID, direct));
    expectBucket(0);
    for (uint64_t pir : {uint64_t(0), uint64_t(1000)})
    {
        attr.value.u64 = pir;
        ASSERT_EQ(SAI_STATUS_SUCCESS,
                state->set_internal(SAI_OBJECT_TYPE_SCHEDULER, sai_serialize_object_id(direct), &attr));
        expectBucket(pir ? 0 : UINT64_MAX);
    }

    auto leaf_scheduler = object(SAI_OBJECT_TYPE_SCHEDULER);
    attr.value.u64 = 0;
    put(SAI_OBJECT_TYPE_SCHEDULER, leaf_scheduler, attr);
    auto leaf = object(SAI_OBJECT_TYPE_SCHEDULER_GROUP);
    attr = {};
    attr.id = SAI_SCHEDULER_GROUP_ATTR_CHILD_COUNT;
    attr.value.u32 = 1;
    put(SAI_OBJECT_TYPE_SCHEDULER_GROUP, leaf, attr);
    attr.id = SAI_SCHEDULER_GROUP_ATTR_CHILD_LIST;
    attr.value.objlist = {1, &queue};
    put(SAI_OBJECT_TYPE_SCHEDULER_GROUP, leaf, attr);
    put(SAI_OBJECT_TYPE_SCHEDULER_GROUP, leaf,
            oidAttr(SAI_SCHEDULER_GROUP_ATTR_SCHEDULER_PROFILE_ID, leaf_scheduler));
    ASSERT_EQ(SAI_STATUS_SUCCESS,
            setOid(SAI_OBJECT_TYPE_QUEUE, queue, SAI_QUEUE_ATTR_SCHEDULER_PROFILE_ID, direct));
    expectBucket(0);
    ASSERT_EQ(SAI_STATUS_SUCCESS,
            setOid(SAI_OBJECT_TYPE_QUEUE, queue, SAI_QUEUE_ATTR_SCHEDULER_PROFILE_ID, SAI_NULL_OBJECT_ID));
    expectBucket(UINT64_MAX);
    attr = {};
    attr.id = SAI_SCHEDULER_ATTR_MAX_BANDWIDTH_RATE;
    attr.value.u64 = 1;
    ASSERT_EQ(SAI_STATUS_SUCCESS, state->set_internal(
            SAI_OBJECT_TYPE_SCHEDULER, sai_serialize_object_id(leaf_scheduler), &attr));
    expectBucket(0);
    ASSERT_EQ(SAI_STATUS_SUCCESS, setOid(SAI_OBJECT_TYPE_SCHEDULER_GROUP, leaf,
            SAI_SCHEDULER_GROUP_ATTR_SCHEDULER_PROFILE_ID, SAI_NULL_OBJECT_ID));
    expectBucket(UINT64_MAX);
}

TEST_F(SwitchVppTrimTest, failedRollbackRequiresReconciliationBeforeNextSet)
{
    ASSERT_EQ(SAI_STATUS_SUCCESS, setSize(256));
    global_results = {-99, -99};
    EXPECT_EQ(SAI_STATUS_FAILURE, setSize(0));
    EXPECT_TRUE(state->m_trim_reconcile_pending);
    global_results = {-99};
    EXPECT_EQ(SAI_STATUS_FAILURE, setSwitchByte(SAI_SWITCH_ATTR_PACKET_TRIM_DSCP_VALUE, 48));
    EXPECT_EQ(0u, state->m_trim_policy.dscp_value);
    EXPECT_TRUE(state->m_trim_reconcile_pending);
    EXPECT_EQ(SAI_STATUS_SUCCESS, setSwitchByte(SAI_SWITCH_ATTR_PACKET_TRIM_DSCP_VALUE, 48));
    EXPECT_FALSE(state->m_trim_reconcile_pending);
    EXPECT_EQ(48u, state->m_trim_policy.dscp_value);
}

TEST_F(SwitchVppTrimTest, unbindingMapsProgramsRemainingBindingsAndDefaults)
{
    sai_object_id_t port_id, dscp_map, queue_map;
    qosMaps(port_id, dscp_map, queue_map);
    ASSERT_EQ(SAI_STATUS_SUCCESS, setSize(256));
    EXPECT_EQ(5u, map_calls.back()[10]);
    EXPECT_EQ(6u, map_calls.back()[0]);
    ASSERT_EQ(SAI_STATUS_SUCCESS, setOid(SAI_OBJECT_TYPE_PORT, port_id,
            SAI_PORT_ATTR_QOS_DSCP_TO_TC_MAP, SAI_NULL_OBJECT_ID));
    for (uint8_t queue : map_calls.back())
    {
        EXPECT_EQ(6u, queue);
    }
    ASSERT_EQ(SAI_STATUS_SUCCESS, setOid(SAI_OBJECT_TYPE_PORT, port_id,
            SAI_PORT_ATTR_QOS_TC_TO_QUEUE_MAP, SAI_NULL_OBJECT_ID));
    for (uint8_t queue : map_calls.back())
    {
        EXPECT_EQ(0u, queue);
    }
    ASSERT_EQ(SAI_STATUS_SUCCESS, setOid(SAI_OBJECT_TYPE_PORT, port_id,
            SAI_PORT_ATTR_QOS_DSCP_TO_TC_MAP, dscp_map));
    EXPECT_EQ(0u, map_calls.back()[10]);
    ASSERT_EQ(SAI_STATUS_SUCCESS, setOid(SAI_OBJECT_TYPE_PORT, port_id,
            SAI_PORT_ATTR_QOS_TC_TO_QUEUE_MAP, queue_map));
    EXPECT_EQ(5u, map_calls.back()[10]);
}

TEST_F(SwitchVppTrimTest, unsupportedQueueMapDoesNotCommitOrReachVpp)
{
    sai_object_id_t port_id, dscp_map, queue_map;
    qosMaps(port_id, dscp_map, queue_map);
    ASSERT_EQ(SAI_STATUS_SUCCESS, setSize(256));
    sai_qos_map_t entry = {};
    entry.key.tc = 2;
    entry.value.queue_index = 8;
    sai_attribute_t attr = {};
    attr.id = SAI_QOS_MAP_ATTR_MAP_TO_VALUE_LIST;
    attr.value.qosmap = {1, &entry};
    EXPECT_EQ(SAI_STATUS_NOT_SUPPORTED,
            state->set_internal(SAI_OBJECT_TYPE_QOS_MAP, sai_serialize_object_id(queue_map), &attr));
    for (const auto &map : map_calls)
    {
        for (uint8_t queue : map)
        {
            EXPECT_LT(queue, 8u);
        }
    }
    EXPECT_EQ(5u, map_calls.back()[10]);
}

TEST_F(SwitchVppTrimTest, warmCachePreservesPolicyOnDscpOnlyUpdate)
{
    ASSERT_EQ(SAI_STATUS_SUCCESS, setSize(256));
    ASSERT_EQ(SAI_STATUS_SUCCESS, setSwitchByte(SAI_SWITCH_ATTR_PACKET_TRIM_DSCP_VALUE, 48));
    ASSERT_EQ(SAI_STATUS_SUCCESS, setSwitchByte(SAI_SWITCH_ATTR_PACKET_TRIM_TC_VALUE, 2));
    ASSERT_EQ(SAI_STATUS_SUCCESS, setSwitchByte(SAI_SWITCH_ATTR_PACKET_TRIM_QUEUE_INDEX, 6));
    for (bool enabled : {true, false})
    {
        if (!enabled)
        {
            ASSERT_EQ(SAI_STATUS_SUCCESS, setSize(0));
        }
        auto warm = std::make_shared<WarmBootState>();
        warm->m_switchId = switch_id;
        warm->m_objectHash = state->m_objectHash;
        auto restored = std::make_shared<SwitchVpp>(switch_id, manager, config, warm);
        stopEvents(restored);
        sai_attribute_t attr = {};
        attr.id = SAI_SWITCH_ATTR_PACKET_TRIM_DSCP_VALUE;
        attr.value.u8 = 49;
        ASSERT_EQ(SAI_STATUS_SUCCESS,
                restored->set(SAI_OBJECT_TYPE_SWITCH, sai_serialize_object_id(switch_id), &attr));
        EXPECT_EQ(enabled, global_calls.back().enabled);
        EXPECT_EQ(enabled ? 256u : 0u, global_calls.back().size);
        EXPECT_EQ(49u, global_calls.back().dscp);
        EXPECT_EQ(2u, global_calls.back().tc);
        EXPECT_EQ(6u, global_calls.back().queue);
    }
}

/*
 * Two front panel ports, SONiC's Ethernet4 and Ethernet40 on VPP interfaces
 * PORT_HWIF and OTHER_PORT_HWIF, the first name a prefix of the second, each
 * behind a bridge port, and VLAN 100. A tagged member of the VLAN
 * is VPP sub-interface <port>.100 in bridge domain 100. A sub-port router
 * interface on VLAN 200 is <port>.200. Both ports start admin down in VPP, as
 * they do before orchagent sets SAI_PORT_ATTR_ADMIN_STATE.
 *
 * The unit tests are built with access control checking disabled, so the
 * fixture reaches the protected and private SwitchVpp members directly.
 */
class SwitchVppSubIfAdminState : public ::testing::Test
{
    protected:

        void SetUp() override
        {
            vppMock().reset();
            auto sc = std::make_shared<SwitchConfig>(0, "");

            sc->m_saiSwitchType = SAI_SWITCH_TYPE_NPU;
            sc->m_switchType = SAI_VS_SWITCH_TYPE_VPP;
            sc->m_bootType = SAI_VS_BOOT_TYPE_COLD;
            sc->m_useTapDevice = false;
            sc->m_profileMap[SAI_KEY_VS_PORT_CONFIG_FILE] = "/dev/null";
            sc->m_laneMap = LaneMap::getDefaultLaneMap(0);
            sc->m_eventQueue = std::make_shared<EventQueue>(std::make_shared<Signal>());

            auto scc = std::make_shared<SwitchConfigContainer>();

            scc->insert(sc);

            m_mgr = std::make_shared<RealObjectIdManager>(0, scc);
            m_sw = std::make_shared<TestableSwitchVpp>(m_switchId, m_mgr, sc);

            sc->m_useTapDevice = true;

            m_vlan = m_mgr->allocateNewObjectId(SAI_OBJECT_TYPE_VLAN, m_switchId);

            sai_attribute_t vattr;

            vattr.id = SAI_VLAN_ATTR_VLAN_ID;
            vattr.value.u16 = VLAN_ID;

            ASSERT_EQ(SAI_STATUS_SUCCESS,
                    m_sw->create_internal(SAI_OBJECT_TYPE_VLAN, sai_serialize_object_id(m_vlan), m_switchId, 1, &vattr));

            addPort(PORT_HWIF, "Ethernet4", m_port, m_bridgePort);
            addPort(OTHER_PORT_HWIF, "Ethernet40", m_otherPort, m_otherBridgePort);

            vppMock().calls.clear();
            vppMock().adminUp.clear();
            vppMock().interfaces.clear();
        }

        void TearDown() override
        {
            m_sw->m_switchConfig->m_useTapDevice = false;
            m_sw.reset();
        }

        void addPort(
                const std::string& hwif,
                const std::string& osif,
                sai_object_id_t& port,
                sai_object_id_t& bridgePort)
        {
            SWSS_LOG_ENTER();

            port = m_mgr->allocateNewObjectId(SAI_OBJECT_TYPE_PORT, m_switchId);

            sai_attribute_t pattr;

            pattr.id = SAI_PORT_ATTR_ADMIN_STATE;
            pattr.value.booldata = false;

            ASSERT_EQ(SAI_STATUS_SUCCESS,
                    m_sw->create_internal(SAI_OBJECT_TYPE_PORT, sai_serialize_object_id(port), m_switchId, 1, &pattr));

            // what resolving the port by its lanes records
            ASSERT_NE(nullptr, m_sw->m_ifaceRegistry.addPhysicalPort(hwif, osif, port));

            bridgePort = m_mgr->allocateNewObjectId(SAI_OBJECT_TYPE_BRIDGE_PORT, m_switchId);

            sai_attribute_t battrs[2];

            battrs[0].id = SAI_BRIDGE_PORT_ATTR_TYPE;
            battrs[0].value.s32 = SAI_BRIDGE_PORT_TYPE_PORT;
            battrs[1].id = SAI_BRIDGE_PORT_ATTR_PORT_ID;
            battrs[1].value.oid = port;

            ASSERT_EQ(SAI_STATUS_SUCCESS,
                    m_sw->create_internal(SAI_OBJECT_TYPE_BRIDGE_PORT, sai_serialize_object_id(bridgePort), m_switchId, 2, battrs));
        }

        // orchagent adds the port to the VLAN as a tagged member
        sai_object_id_t addTaggedMember(
                sai_object_id_t bridgePort)
        {
            SWSS_LOG_ENTER();

            auto member = m_mgr->allocateNewObjectId(SAI_OBJECT_TYPE_VLAN_MEMBER, m_switchId);

            sai_attribute_t attrs[3];

            attrs[0].id = SAI_VLAN_MEMBER_ATTR_VLAN_ID;
            attrs[0].value.oid = m_vlan;
            attrs[1].id = SAI_VLAN_MEMBER_ATTR_BRIDGE_PORT_ID;
            attrs[1].value.oid = bridgePort;
            attrs[2].id = SAI_VLAN_MEMBER_ATTR_VLAN_TAGGING_MODE;
            attrs[2].value.s32 = SAI_VLAN_TAGGING_MODE_TAGGED;

            EXPECT_EQ(SAI_STATUS_SUCCESS, m_sw->createVlanMember(member, m_switchId, 3, attrs));

            return member;
        }

        // orchagent sets SAI_PORT_ATTR_ADMIN_STATE
        void setPortAdminState(
                sai_object_id_t port,
                bool is_up)
        {
            SWSS_LOG_ENTER();

            sai_attribute_t attr;

            attr.id = SAI_PORT_ATTR_ADMIN_STATE;
            attr.value.booldata = is_up;

            EXPECT_EQ(SAI_STATUS_SUCCESS, m_sw->setPort(port, &attr));
        }

        // a sub-port router interface on VLAN 200 sets its admin state
        void setSubPortAdminState(
                bool is_up)
        {
            SWSS_LOG_ENTER();

            EXPECT_EQ(SAI_STATUS_SUCCESS, m_sw->vpp_set_interface_state(m_port, SUB_PORT_VLAN_ID, is_up));
        }

        static constexpr uint16_t VLAN_ID = 100;
        static constexpr uint16_t SUB_PORT_VLAN_ID = 200;
        static constexpr const char *PORT_HWIF = "bobm1";
        static constexpr const char *OTHER_PORT_HWIF = "bobm10";
        static constexpr const char *MEMBER_HWIF = "bobm1.100";
        static constexpr const char *OTHER_MEMBER_HWIF = "bobm10.100";
        static constexpr const char *SUB_PORT_HWIF = "bobm1.200";

        const sai_object_id_t m_switchId = 0x2100000000;

        std::shared_ptr<RealObjectIdManager> m_mgr;

        std::shared_ptr<SwitchVpp> m_sw;

        sai_object_id_t m_vlan = SAI_NULL_OBJECT_ID;
        sai_object_id_t m_port = SAI_NULL_OBJECT_ID;
        sai_object_id_t m_bridgePort = SAI_NULL_OBJECT_ID;
        sai_object_id_t m_otherPort = SAI_NULL_OBJECT_ID;
        sai_object_id_t m_otherBridgePort = SAI_NULL_OBJECT_ID;
};

constexpr uint16_t SwitchVppSubIfAdminState::VLAN_ID;
constexpr uint16_t SwitchVppSubIfAdminState::SUB_PORT_VLAN_ID;
constexpr const char *SwitchVppSubIfAdminState::PORT_HWIF;
constexpr const char *SwitchVppSubIfAdminState::OTHER_PORT_HWIF;
constexpr const char *SwitchVppSubIfAdminState::MEMBER_HWIF;
constexpr const char *SwitchVppSubIfAdminState::OTHER_MEMBER_HWIF;
constexpr const char *SwitchVppSubIfAdminState::SUB_PORT_HWIF;

/*
 * A tagged member added while its port is still admin down: VPP refuses the
 * sub-interface's admin up, and nothing brought it up when the port came up,
 * so the member stayed down in VPP while CONFIG_DB, APPL_DB and STATE_DB all
 * said up.
 */
TEST_F(SwitchVppSubIfAdminState, TaggedMemberAddedWhileThePortIsDownComesUpWithThePort)
{
    addTaggedMember(m_bridgePort);

    auto calls = vppMock().callsTo("interface_set_state", MEMBER_HWIF);

    ASSERT_EQ(1u, calls.size());
    EXPECT_TRUE(calls[0].flag);
    EXPECT_NE(0u, calls[0].sub_id);
    EXPECT_FALSE(vppMock().isAdminUp(MEMBER_HWIF));

    setPortAdminState(m_port, true);

    EXPECT_TRUE(vppMock().isAdminUp(PORT_HWIF));
    EXPECT_TRUE(vppMock().isAdminUp(MEMBER_HWIF));

    calls = vppMock().callsTo("interface_set_state", MEMBER_HWIF);

    ASSERT_EQ(2u, calls.size());
    EXPECT_TRUE(calls[1].flag);
    EXPECT_EQ(0u, calls[1].sub_id);

    // applied once: a later port flap doesn't set it again
    setPortAdminState(m_port, false);
    setPortAdminState(m_port, true);

    EXPECT_EQ(2u, vppMock().callsTo("interface_set_state", MEMBER_HWIF).size());
}

TEST_F(SwitchVppSubIfAdminState, TaggedMemberAddedWhileThePortIsUpIsSetUpOnce)
{
    setPortAdminState(m_port, true);

    addTaggedMember(m_bridgePort);

    auto calls = vppMock().callsTo("interface_set_state", MEMBER_HWIF);

    ASSERT_EQ(1u, calls.size());
    EXPECT_TRUE(calls[0].flag);
    EXPECT_EQ(0u, calls[0].sub_id);
    EXPECT_TRUE(vppMock().isAdminUp(MEMBER_HWIF));

    // VPP keeps the sub-interface admin up across a port flap on its own
    setPortAdminState(m_port, false);
    setPortAdminState(m_port, true);

    EXPECT_EQ(1u, vppMock().callsTo("interface_set_state", MEMBER_HWIF).size());
    EXPECT_TRUE(vppMock().isAdminUp(MEMBER_HWIF));
}

TEST_F(SwitchVppSubIfAdminState, TaggedMemberRemovedBeforeThePortComesUpIsNotSetUp)
{
    auto member = addTaggedMember(m_bridgePort);

    EXPECT_FALSE(vppMock().isAdminUp(MEMBER_HWIF));

    EXPECT_EQ(SAI_STATUS_SUCCESS, m_sw->removeVlanMember(member));

    ASSERT_EQ(1u, vppMock().callsTo("delete_sub_interface", PORT_HWIF).size());

    vppMock().calls.clear();

    setPortAdminState(m_port, true);

    EXPECT_TRUE(vppMock().isAdminUp(PORT_HWIF));
    EXPECT_TRUE(vppMock().callsTo("interface_set_state", MEMBER_HWIF).empty());
}

/*
 * A port coming up must not touch the sub-interfaces of another port whose
 * name it is a prefix of.
 */
TEST_F(SwitchVppSubIfAdminState, OnlyThePortThatComesUpBringsItsMembersUp)
{
    addTaggedMember(m_bridgePort);
    addTaggedMember(m_otherBridgePort);

    setPortAdminState(m_port, true);

    EXPECT_TRUE(vppMock().isAdminUp(MEMBER_HWIF));
    EXPECT_FALSE(vppMock().isAdminUp(OTHER_MEMBER_HWIF));
    EXPECT_EQ(1u, vppMock().callsTo("interface_set_state", OTHER_MEMBER_HWIF).size());

    setPortAdminState(m_otherPort, true);

    EXPECT_TRUE(vppMock().isAdminUp(OTHER_MEMBER_HWIF));
}

/*
 * The router interface path: SAI_ROUTER_INTERFACE_ATTR_ADMIN_V4_STATE or
 * ADMIN_V6_STATE on a sub-port.
 */
TEST_F(SwitchVppSubIfAdminState, SubPortSetUpWhileThePortIsDownComesUpWithThePort)
{
    setSubPortAdminState(true);

    EXPECT_FALSE(vppMock().isAdminUp(SUB_PORT_HWIF));

    setPortAdminState(m_port, true);

    EXPECT_TRUE(vppMock().isAdminUp(SUB_PORT_HWIF));
}

TEST_F(SwitchVppSubIfAdminState, SubPortSetDownBeforeThePortComesUpStaysDown)
{
    setSubPortAdminState(true);
    setSubPortAdminState(false);

    auto calls = vppMock().callsTo("interface_set_state", SUB_PORT_HWIF);

    ASSERT_EQ(2u, calls.size());
    EXPECT_FALSE(calls[1].flag);
    EXPECT_EQ(0u, calls[1].sub_id);

    setPortAdminState(m_port, true);

    EXPECT_EQ(2u, vppMock().callsTo("interface_set_state", SUB_PORT_HWIF).size());
    EXPECT_FALSE(vppMock().isAdminUp(SUB_PORT_HWIF));
}
