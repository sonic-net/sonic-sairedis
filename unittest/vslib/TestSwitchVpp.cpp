#include "vpp/SwitchVpp.h"
#include "ContextConfigContainer.h"
#include "sai_serialize.h"
#include "saivs.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <deque>

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
    };

    std::vector<GlobalTrimCall> global_calls;
    std::vector<QueueTrimCall> queue_calls;
    std::vector<std::array<uint8_t, 64>> map_calls;
    std::deque<int> global_results;
    std::deque<int> map_results;
    unsigned int interface_lookups = 0;

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

extern "C" int __wrap_init_vpp_client()
{
    return 0;
}

extern "C" int __wrap_vpp_sync_for_events()
{
    return 0;
}

extern "C" vpp_event_info_t *__wrap_vpp_ev_dequeue()
{
    return nullptr;
}

extern "C" int __wrap_vpp_want_l2_macs_events2(bool, vpp_mac_event_cb_fn, void *)
{
    return 0;
}

extern "C" uint32_t __wrap_vpp_get_swif_idx_by_name(const char *)
{
    ++interface_lookups;
    return 1;
}

extern "C" int __wrap_vpp_sonic_ext_trim_global_set(
        bool enabled, uint16_t size, uint8_t, uint8_t dscp, uint8_t tc, uint8_t queue)
{
    global_calls.push_back({enabled, size, dscp, tc, queue});
    return nextResult(global_results);
}

extern "C" int __wrap_vpp_sonic_ext_trim_queue_set(
        const char *, uint8_t queue, bool eligible, uint64_t, uint64_t)
{
    queue_calls.push_back({queue, eligible});
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
        interface_lookups = 0;
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
    EXPECT_EQ(1u, interface_lookups);
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
