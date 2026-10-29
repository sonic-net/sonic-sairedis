#include "MockVppXlate.h"
#include "vpp/EventPayloadVppEvent.h"
#include "vpp/SwitchVppUtils.h"
#include "vpp/vppxlate/SaiIntfStats.h"
#include "vpp/vppxlate/SaiVppXlate.h"
#include "ContextConfigContainer.h"
#include "EventPayloadNotification.h"
#include "HostInterfaceInfo.h"
#include "SwitchConfigContainer.h"
#include "meta/DummySaiInterface.h"
#include "meta/Meta.h"
#include "meta/sai_serialize.h"
#include "saivs.h"

#include <gtest/gtest.h>

#include <arpa/inet.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdlib>
#include <cstring>
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
        std::string hwif;
    };

    std::vector<GlobalTrimCall> global_calls;
    std::vector<QueueTrimCall> queue_calls;
    std::vector<std::array<uint8_t, 64>> map_calls;
    std::map<std::pair<std::string, uint8_t>, QueueTrimCall> queue_state;
    std::vector<std::string> trim_calls;
    std::deque<int> global_results;
    std::deque<int> queue_results;
    std::deque<int> map_results;

    int nextResult(std::deque<int> &results)
    {
        SWSS_LOG_ENTER();

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
    SWSS_LOG_ENTER();

    global_calls.push_back({enabled, size, dscp, tc, queue});
    trim_calls.push_back("global");
    return nextResult(global_results);
}

extern "C" int __wrap_vpp_sonic_ext_trim_queue_set(
        const char *hwif, uint8_t queue, bool eligible, uint64_t rate, uint64_t capacity)
{
    SWSS_LOG_ENTER();

    queue_calls.push_back({queue, eligible, rate, capacity, hwif});
    // Model a lost reply after the update reached VPP.
    queue_state[{hwif, queue}] = queue_calls.back();
    trim_calls.push_back("queue");
    return nextResult(queue_results);
}

extern "C" int __wrap_vpp_sonic_ext_trim_dscp_map_set(const uint8_t map[64])
{
    SWSS_LOG_ENTER();

    std::array<uint8_t, 64> copy;
    std::copy(map, map + copy.size(), copy.begin());
    map_calls.push_back(copy);
    trim_calls.push_back("map");
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

    // Port map (re)bindings and in-place map edits re-resolve the
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
        SWSS_LOG_ENTER();

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
        queue_state.clear();
        trim_calls.clear();
        global_results.clear();
        queue_results.clear();
        map_results.clear();
        vppMock().interfaceLookups = 0;
    }

    static void stopEvents(const std::shared_ptr<SwitchVpp> &sw)
    {
        SWSS_LOG_ENTER();

        sw->m_run_vpp_events_thread = false;
        sw->m_vpp_thread->join();
        sw->m_operResyncDue.store(false);
    }

    sai_object_id_t object(sai_object_type_t type)
    {
        SWSS_LOG_ENTER();

        auto oid = manager->allocateNewObjectId(type, switch_id);
        state->m_objectHash[type][sai_serialize_object_id(oid)] = {};
        return oid;
    }

    void put(sai_object_type_t type, sai_object_id_t oid, sai_attribute_t attr)
    {
        SWSS_LOG_ENTER();

        auto wrap = std::make_shared<SaiAttrWrap>(type, &attr);
        state->m_objectHash[type][sai_serialize_object_id(oid)][
                wrap->getAttrMetadata()->attridname] = wrap;
    }

    static sai_attribute_t oidAttr(sai_attr_id_t id, sai_object_id_t value)
    {
        SWSS_LOG_ENTER();

        sai_attribute_t attr = {};
        attr.id = id;
        attr.value.oid = value;
        return attr;
    }

    sai_status_t setSize(uint32_t value)
    {
        SWSS_LOG_ENTER();

        sai_attribute_t attr = {};
        attr.id = SAI_SWITCH_ATTR_PACKET_TRIM_SIZE;
        attr.value.u32 = value;
        return state->set(SAI_OBJECT_TYPE_SWITCH, sai_serialize_object_id(switch_id), &attr);
    }

    sai_status_t setSwitchByte(sai_attr_id_t id, uint8_t value)
    {
        SWSS_LOG_ENTER();

        sai_attribute_t attr = {};
        attr.id = id;
        attr.value.u8 = value;
        return state->set(SAI_OBJECT_TYPE_SWITCH, sai_serialize_object_id(switch_id), &attr);
    }

    sai_status_t setOid(sai_object_type_t type, sai_object_id_t oid,
            sai_attr_id_t id, sai_object_id_t value)
    {
        SWSS_LOG_ENTER();

        auto attr = oidAttr(id, value);
        return state->set_internal(type, sai_serialize_object_id(oid), &attr);
    }

    sai_object_id_t trimQueue(uint8_t index)
    {
        SWSS_LOG_ENTER();

        auto profile = object(SAI_OBJECT_TYPE_BUFFER_PROFILE);
        sai_attribute_t attr = {};
        attr.id = SAI_BUFFER_PROFILE_ATTR_PACKET_ADMISSION_FAIL_ACTION;
        attr.value.s32 = SAI_BUFFER_PROFILE_PACKET_ADMISSION_FAIL_ACTION_DROP_AND_TRIM;
        put(SAI_OBJECT_TYPE_BUFFER_PROFILE, profile, attr);
        auto queue = object(SAI_OBJECT_TYPE_QUEUE);
        put(SAI_OBJECT_TYPE_QUEUE, queue, oidAttr(SAI_QUEUE_ATTR_BUFFER_PROFILE_ID, profile));
        attr.id = SAI_QUEUE_ATTR_TYPE;
        attr.value.s32 = SAI_QUEUE_TYPE_UNICAST;
        put(SAI_OBJECT_TYPE_QUEUE, queue, attr);
        attr.id = SAI_QUEUE_ATTR_INDEX;
        attr.value.u8 = index;
        put(SAI_OBJECT_TYPE_QUEUE, queue, attr);
        return queue;
    }

    sai_object_id_t port(const std::vector<sai_object_id_t> &queues = {},
            const std::string &hwif = "pg0", const std::string &name = "Ethernet0")
    {
        SWSS_LOG_ENTER();

        auto oid = object(SAI_OBJECT_TYPE_PORT);
        state->m_ifaceRegistry.addPhysicalPort(hwif, name, oid);
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
        SWSS_LOG_ENTER();

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

    void pendingUnsupportedQosMap(sai_object_id_t &queue_map)
    {
        SWSS_LOG_ENTER();

        sai_object_id_t port_id, dscp_map;
        qosMaps(port_id, dscp_map, queue_map);
        auto queue = trimQueue(1);
        sai_attribute_t attr = {};
        attr.id = SAI_PORT_ATTR_QOS_QUEUE_LIST;
        attr.value.objlist = {1, &queue};
        put(SAI_OBJECT_TYPE_PORT, port_id, attr);
        ASSERT_EQ(SAI_STATUS_SUCCESS, setSize(256));

        // Replay can expose a port binding before its map CREATE.
        queue_map = manager->allocateNewObjectId(SAI_OBJECT_TYPE_QOS_MAP, switch_id);
        put(SAI_OBJECT_TYPE_PORT, port_id,
                oidAttr(SAI_PORT_ATTR_QOS_TC_TO_QUEUE_MAP, queue_map));
        sai_qos_map_t entry = {};
        entry.key.tc = 2;
        entry.value.queue_index = 8;
        sai_attribute_t attrs[2] = {};
        attrs[0].id = SAI_QOS_MAP_ATTR_TYPE;
        attrs[0].value.s32 = SAI_QOS_MAP_TYPE_TC_TO_QUEUE;
        attrs[1].id = SAI_QOS_MAP_ATTR_MAP_TO_VALUE_LIST;
        attrs[1].value.qosmap = {1, &entry};
        ASSERT_EQ(SAI_STATUS_SUCCESS, state->create_internal(SAI_OBJECT_TYPE_QOS_MAP,
                sai_serialize_object_id(queue_map), switch_id, 2, attrs));
        ASSERT_TRUE(state->m_trim_reconcile_pending);
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

TEST_F(SwitchVppTrimTest, invalidDscpCreatePreservesAttributesWithoutProgramming)
{
    const auto before = state->m_objectHash.at(SAI_OBJECT_TYPE_SWITCH).at(
            sai_serialize_object_id(switch_id));
    sai_attribute_t attrs[2] = {};
    attrs[0].id = SAI_SWITCH_ATTR_PACKET_TRIM_SIZE;
    attrs[0].value.u32 = 256;
    attrs[1].id = SAI_SWITCH_ATTR_PACKET_TRIM_DSCP_VALUE;
    for (uint8_t dscp : std::array<uint8_t, 2>{{64, 255}})
    {
        attrs[1].value.u8 = dscp;
        EXPECT_EQ(SAI_STATUS_INVALID_ATTR_VALUE_0 - 1, state->create(SAI_OBJECT_TYPE_SWITCH,
                sai_serialize_object_id(switch_id), switch_id, 2, attrs));
        EXPECT_TRUE(global_calls.empty());
        EXPECT_TRUE(map_calls.empty());
        EXPECT_FALSE(state->m_trim_policy.enabled);
        EXPECT_EQ(before, state->m_objectHash.at(SAI_OBJECT_TYPE_SWITCH).at(
                sai_serialize_object_id(switch_id)));
    }
    attrs[1].value.u8 = 63;
    ASSERT_EQ(SAI_STATUS_SUCCESS, state->create(SAI_OBJECT_TYPE_SWITCH,
            sai_serialize_object_id(switch_id), switch_id, 2, attrs));
    EXPECT_EQ(63u, global_calls.back().dscp);
}

TEST_F(SwitchVppTrimTest, dscpValidationPreservesEnabledAndDisabledPolicy)
{
    for (uint32_t size : {0u, 256u})
    {
        ASSERT_EQ(SAI_STATUS_SUCCESS, setSize(size));
        for (uint8_t dscp : std::array<uint8_t, 2>{{0, 63}})
        {
            ASSERT_EQ(SAI_STATUS_SUCCESS, setSwitchByte(SAI_SWITCH_ATTR_PACKET_TRIM_DSCP_VALUE, dscp));
        }
        const size_t global_count = global_calls.size();
        const size_t map_count = map_calls.size();
        for (uint8_t dscp : std::array<uint8_t, 2>{{64, 255}})
        {
            EXPECT_EQ(SAI_STATUS_INVALID_ATTR_VALUE_0,
                    setSwitchByte(SAI_SWITCH_ATTR_PACKET_TRIM_DSCP_VALUE, dscp));
            EXPECT_EQ(global_count, global_calls.size());
            EXPECT_EQ(map_count, map_calls.size());
            EXPECT_EQ(63u, state->m_trim_policy.dscp_value);
            EXPECT_EQ(size != 0, state->m_trim_policy.enabled);
            sai_attribute_t attr = {};
            attr.id = SAI_SWITCH_ATTR_PACKET_TRIM_DSCP_VALUE;
            ASSERT_EQ(SAI_STATUS_SUCCESS, state->get(SAI_OBJECT_TYPE_SWITCH, switch_id, 1, &attr));
            EXPECT_EQ(63u, attr.value.u8);
        }
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

TEST_F(SwitchVppTrimTest, pendingInvalidQosCanBeDisabledRepairedAndEnabled)
{
    sai_object_id_t queue_map;
    pendingUnsupportedQosMap(queue_map);
    ASSERT_TRUE(state->m_trim_reconcile_pending);
    sai_qos_map_t entry = {};
    entry.key.tc = 2;
    entry.value.queue_index = 5;
    sai_attribute_t attr = {};
    attr.id = SAI_QOS_MAP_ATTR_MAP_TO_VALUE_LIST;
    attr.value.qosmap = {1, &entry};
    EXPECT_EQ(SAI_STATUS_NOT_SUPPORTED, state->set_internal(
            SAI_OBJECT_TYPE_QOS_MAP, sai_serialize_object_id(queue_map), &attr));
    EXPECT_TRUE(state->m_trim_reconcile_pending);
    global_calls.clear();
    queue_calls.clear();
    map_calls.clear();

    ASSERT_EQ(SAI_STATUS_SUCCESS, setSize(0));
    ASSERT_EQ(1u, global_calls.size());
    EXPECT_FALSE(global_calls.back().enabled);
    EXPECT_FALSE(state->m_trim_policy.enabled);
    EXPECT_FALSE(state->m_trim_reconcile_pending);
    EXPECT_FALSE(state->m_trim_dataplane_armed);
    ASSERT_EQ(8u, queue_calls.size());
    for (const auto &call : queue_calls)
    {
        EXPECT_FALSE(call.eligible);
        EXPECT_EQ(UINT64_MAX, call.rate);
        EXPECT_EQ(UINT64_MAX, call.capacity);
    }
    EXPECT_TRUE(map_calls.empty());

    ASSERT_EQ(SAI_STATUS_SUCCESS, state->set_internal(
            SAI_OBJECT_TYPE_QOS_MAP, sai_serialize_object_id(queue_map), &attr));
    EXPECT_EQ(8u, queue_calls.size());
    ASSERT_EQ(SAI_STATUS_SUCCESS, setSize(256));
    EXPECT_FALSE(state->m_trim_reconcile_pending);
    EXPECT_TRUE(queue_state.at({"pg0", 1}).eligible);
    ASSERT_FALSE(map_calls.empty());
    EXPECT_EQ(5u, map_calls.back()[10]);
}

TEST_F(SwitchVppTrimTest, pendingDisableFailuresRetainCommittedPolicy)
{
    sai_object_id_t queue_map;
    pendingUnsupportedQosMap(queue_map);
    ASSERT_TRUE(state->m_trim_reconcile_pending);

    for (bool fail_global : {true, false})
    {
        SCOPED_TRACE(fail_global);
        global_calls.clear();
        if (fail_global)
        {
            global_results = {-99};
        }
        else
        {
            queue_results = {-99};
        }
        EXPECT_EQ(SAI_STATUS_FAILURE, setSize(0));
        ASSERT_FALSE(global_calls.empty());
        EXPECT_FALSE(global_calls.front().enabled);
        EXPECT_TRUE(global_calls.back().enabled);
        EXPECT_TRUE(state->m_trim_policy.enabled);
        EXPECT_TRUE(state->m_trim_dataplane_armed);
        EXPECT_TRUE(state->m_trim_reconcile_pending);
        sai_attribute_t attr = {};
        attr.id = SAI_SWITCH_ATTR_PACKET_TRIM_SIZE;
        ASSERT_EQ(SAI_STATUS_SUCCESS, state->get(SAI_OBJECT_TYPE_SWITCH, switch_id, 1, &attr));
        EXPECT_EQ(256u, attr.value.u32);
    }
    EXPECT_EQ(SAI_STATUS_SUCCESS, setSize(0));
    EXPECT_FALSE(state->m_trim_reconcile_pending);
}

TEST_F(SwitchVppTrimTest, pendingDisableCleansEvenWhenNotPreviouslyArmed)
{
    port({trimQueue(1)});
    global_results = {-99, -99};
    ASSERT_EQ(SAI_STATUS_FAILURE, setSize(256));
    ASSERT_TRUE(state->m_trim_reconcile_pending);
    ASSERT_FALSE(state->m_trim_dataplane_armed);
    ASSERT_TRUE(queue_calls.empty());

    ASSERT_EQ(SAI_STATUS_SUCCESS, setSize(0));
    EXPECT_FALSE(state->m_trim_reconcile_pending);
    EXPECT_FALSE(state->m_trim_dataplane_armed);
    ASSERT_EQ(8u, queue_calls.size());
    for (const auto &call : queue_calls)
    {
        EXPECT_FALSE(call.eligible);
    }
}

TEST_F(SwitchVppTrimTest, pendingDisableStillFailsForUnresolvedPort)
{
    sai_object_id_t queue_map;
    pendingUnsupportedQosMap(queue_map);
    ASSERT_TRUE(state->m_trim_reconcile_pending);
    auto unresolved = object(SAI_OBJECT_TYPE_PORT);
    sai_attribute_t attr = {};
    attr.id = SAI_PORT_ATTR_TYPE;
    attr.value.s32 = SAI_PORT_TYPE_LOGICAL;
    put(SAI_OBJECT_TYPE_PORT, unresolved, attr);

    EXPECT_EQ(SAI_STATUS_FAILURE, setSize(0));
    EXPECT_TRUE(state->m_trim_reconcile_pending);
    EXPECT_TRUE(state->m_trim_dataplane_armed);
    EXPECT_TRUE(state->m_trim_policy.enabled);
    attr.id = SAI_SWITCH_ATTR_PACKET_TRIM_SIZE;
    ASSERT_EQ(SAI_STATUS_SUCCESS, state->get(SAI_OBJECT_TYPE_SWITCH, switch_id, 1, &attr));
    EXPECT_EQ(256u, attr.value.u32);
}

TEST_F(SwitchVppTrimTest, queueFailureStopsBeforeRemainingQueuesAndClassifier)
{
    port({trimQueue(1), trimQueue(2)});
    port({trimQueue(1)}, "pg1", "Ethernet4");
    ASSERT_EQ(SAI_STATUS_SUCCESS, setSize(256));
    queue_calls.clear();
    map_calls.clear();
    trim_calls.clear();
    queue_results = {-99, -99};

    EXPECT_EQ(SAI_STATUS_FAILURE, setSwitchByte(SAI_SWITCH_ATTR_PACKET_TRIM_DSCP_VALUE, 48));
    EXPECT_EQ((std::vector<std::string>{"global", "queue", "global", "queue"}), trim_calls);
    EXPECT_EQ(2u, queue_calls.size());
    EXPECT_TRUE(map_calls.empty());
    EXPECT_TRUE(state->m_trim_reconcile_pending);
    EXPECT_EQ(0u, state->m_trim_policy.dscp_value);

    global_results = {-99};
    EXPECT_EQ(SAI_STATUS_FAILURE, setSwitchByte(SAI_SWITCH_ATTR_PACKET_TRIM_DSCP_VALUE, 49));
    EXPECT_EQ(2u, queue_calls.size());
    EXPECT_EQ(0u, state->m_trim_policy.dscp_value);
    EXPECT_TRUE(state->m_trim_reconcile_pending);

    queue_calls.clear();
    EXPECT_EQ(SAI_STATUS_SUCCESS, setSwitchByte(SAI_SWITCH_ATTR_PACKET_TRIM_DSCP_VALUE, 49));
    EXPECT_EQ(6u, queue_calls.size());
    EXPECT_EQ(2u, map_calls.size());
    EXPECT_FALSE(state->m_trim_reconcile_pending);
    EXPECT_EQ(49u, state->m_trim_policy.dscp_value);
}

TEST_F(SwitchVppTrimTest, queueFailureRestoresCommittedBinding)
{
    auto queue = trimQueue(1);
    auto attr = oidAttr(SAI_QUEUE_ATTR_BUFFER_PROFILE_ID, SAI_NULL_OBJECT_ID);
    ASSERT_EQ(SAI_STATUS_SUCCESS, state->get(SAI_OBJECT_TYPE_QUEUE, queue, 1, &attr));
    auto profile = attr.value.oid;
    port({queue, trimQueue(2)});
    ASSERT_EQ(SAI_STATUS_SUCCESS, setSize(256));
    queue_calls.clear();
    map_calls.clear();
    queue_results = {-99};

    EXPECT_EQ(SAI_STATUS_FAILURE, setOid(
            SAI_OBJECT_TYPE_QUEUE, queue, SAI_QUEUE_ATTR_BUFFER_PROFILE_ID, SAI_NULL_OBJECT_ID));
    ASSERT_EQ(3u, queue_calls.size());
    EXPECT_FALSE(queue_calls[0].eligible);
    EXPECT_TRUE(queue_calls[1].eligible);
    EXPECT_TRUE(queue_calls[2].eligible);
    EXPECT_EQ(1u, map_calls.size());
    EXPECT_FALSE(state->m_trim_reconcile_pending);
    ASSERT_EQ(SAI_STATUS_SUCCESS, state->get(SAI_OBJECT_TYPE_QUEUE, queue, 1, &attr));
    EXPECT_EQ(profile, attr.value.oid);
    EXPECT_TRUE(queue_state.at({"pg0", 1}).eligible);
    EXPECT_TRUE(queue_state.at({"pg0", 2}).eligible);
}

TEST_F(SwitchVppTrimTest, queueListFailureRequiresReconciliationBeforeCommittingBinding)
{
    auto queue = trimQueue(1);
    auto attr = oidAttr(SAI_QUEUE_ATTR_BUFFER_PROFILE_ID, SAI_NULL_OBJECT_ID);
    ASSERT_EQ(SAI_STATUS_SUCCESS, state->get(SAI_OBJECT_TYPE_QUEUE, queue, 1, &attr));
    auto profile = attr.value.oid;
    auto port_id = port({queue});
    ASSERT_EQ(SAI_STATUS_SUCCESS, setSize(256));
    queue_calls.clear();
    map_calls.clear();

    std::vector<sai_object_id_t> oversized(MAX_OBJLIST_LEN + 1, queue);
    attr.id = SAI_PORT_ATTR_QOS_QUEUE_LIST;
    attr.value.objlist.count = static_cast<uint32_t>(oversized.size());
    attr.value.objlist.list = oversized.data();
    put(SAI_OBJECT_TYPE_PORT, port_id, attr);

    EXPECT_EQ(SAI_STATUS_BUFFER_OVERFLOW, setOid(
            SAI_OBJECT_TYPE_QUEUE, queue, SAI_QUEUE_ATTR_BUFFER_PROFILE_ID, SAI_NULL_OBJECT_ID));
    EXPECT_TRUE(queue_calls.empty());
    EXPECT_TRUE(map_calls.empty());
    EXPECT_TRUE(state->m_trim_reconcile_pending);
    attr.id = SAI_QUEUE_ATTR_BUFFER_PROFILE_ID;
    ASSERT_EQ(SAI_STATUS_SUCCESS, state->get(SAI_OBJECT_TYPE_QUEUE, queue, 1, &attr));
    EXPECT_EQ(profile, attr.value.oid);
    EXPECT_TRUE(queue_state.at({"pg0", 1}).eligible);

    EXPECT_EQ(SAI_STATUS_BUFFER_OVERFLOW, setOid(
            SAI_OBJECT_TYPE_QUEUE, queue, SAI_QUEUE_ATTR_BUFFER_PROFILE_ID, SAI_NULL_OBJECT_ID));
    EXPECT_TRUE(queue_calls.empty());
    EXPECT_TRUE(map_calls.empty());

    attr.id = SAI_PORT_ATTR_QOS_QUEUE_LIST;
    attr.value.objlist = {1, &queue};
    put(SAI_OBJECT_TYPE_PORT, port_id, attr);
    ASSERT_EQ(SAI_STATUS_SUCCESS, setOid(
            SAI_OBJECT_TYPE_QUEUE, queue, SAI_QUEUE_ATTR_BUFFER_PROFILE_ID, SAI_NULL_OBJECT_ID));
    EXPECT_FALSE(state->m_trim_reconcile_pending);
    ASSERT_EQ(2u, queue_calls.size());
    EXPECT_TRUE(queue_calls.front().eligible);
    EXPECT_FALSE(queue_calls.back().eligible);
    EXPECT_FALSE(queue_state.at({"pg0", 1}).eligible);
}

TEST_F(SwitchVppTrimTest, missingPortQueueListIsDeferredUntilReplayCompletes)
{
    auto queue = trimQueue(1);
    auto port_id = port({queue});
    state->m_objectHash[SAI_OBJECT_TYPE_PORT][sai_serialize_object_id(port_id)]
        .erase("SAI_PORT_ATTR_QOS_QUEUE_LIST");

    ASSERT_EQ(SAI_STATUS_SUCCESS, setSize(256));
    EXPECT_TRUE(queue_calls.empty());
    EXPECT_FALSE(state->m_trim_reconcile_pending);

    sai_attribute_t attr = {};
    attr.id = SAI_PORT_ATTR_QOS_QUEUE_LIST;
    attr.value.objlist = {1, &queue};
    put(SAI_OBJECT_TYPE_PORT, port_id, attr);
    ASSERT_EQ(SAI_STATUS_SUCCESS, setSwitchByte(SAI_SWITCH_ATTR_PACKET_TRIM_DSCP_VALUE, 48));
    ASSERT_EQ(1u, queue_calls.size());
    EXPECT_TRUE(queue_calls.back().eligible);
    EXPECT_FALSE(state->m_trim_reconcile_pending);
}

TEST_F(SwitchVppTrimTest, bestEffortCreateRecordsFailedRefreshForReconciliation)
{
    port({trimQueue(1), trimQueue(2)});
    ASSERT_EQ(SAI_STATUS_SUCCESS, setSize(256));
    queue_calls.clear();
    map_calls.clear();
    queue_results = {-99};
    auto profile = manager->allocateNewObjectId(SAI_OBJECT_TYPE_BUFFER_PROFILE, switch_id);
    sai_attribute_t attr = {};
    attr.id = SAI_BUFFER_PROFILE_ATTR_PACKET_ADMISSION_FAIL_ACTION;
    attr.value.s32 = SAI_BUFFER_PROFILE_PACKET_ADMISSION_FAIL_ACTION_DROP_AND_TRIM;

    EXPECT_EQ(SAI_STATUS_SUCCESS, state->create_internal(SAI_OBJECT_TYPE_BUFFER_PROFILE,
            sai_serialize_object_id(profile), switch_id, 1, &attr));
    EXPECT_TRUE(state->m_trim_reconcile_pending);
    EXPECT_EQ(1u, queue_calls.size());
    EXPECT_TRUE(map_calls.empty());
    ASSERT_EQ(SAI_STATUS_SUCCESS, state->get(SAI_OBJECT_TYPE_BUFFER_PROFILE, profile, 1, &attr));
    EXPECT_EQ(SAI_BUFFER_PROFILE_PACKET_ADMISSION_FAIL_ACTION_DROP_AND_TRIM, attr.value.s32);
    EXPECT_EQ(SAI_STATUS_SUCCESS, setSwitchByte(SAI_SWITCH_ATTR_PACKET_TRIM_DSCP_VALUE, 48));
    EXPECT_FALSE(state->m_trim_reconcile_pending);
}

TEST_F(SwitchVppTrimTest, defaultOffDoesNotProgramQueues)
{
    auto queue = trimQueue(1);
    port({queue});
    ASSERT_EQ(SAI_STATUS_SUCCESS, state->create_internal(
            SAI_OBJECT_TYPE_SWITCH, sai_serialize_object_id(switch_id), switch_id, 0, nullptr));
    EXPECT_TRUE(queue_calls.empty());
    EXPECT_TRUE(map_calls.empty());
    EXPECT_EQ(SAI_STATUS_SUCCESS, setOid(
            SAI_OBJECT_TYPE_QUEUE, queue, SAI_QUEUE_ATTR_BUFFER_PROFILE_ID, SAI_NULL_OBJECT_ID));
    EXPECT_TRUE(queue_calls.empty());
    EXPECT_TRUE(map_calls.empty());
}

TEST_F(SwitchVppTrimTest, disableClearsAllSlotsWithoutQueueMetadataBeforeUnbinding)
{
    auto first = trimQueue(1);
    auto second = trimQueue(2);
    auto third = trimQueue(1);
    auto first_port = port({first, second});
    auto second_port = port({third}, "pg1", "Ethernet4");
    ASSERT_EQ(SAI_STATUS_SUCCESS, setSize(256));
    state->m_objectHash[SAI_OBJECT_TYPE_PORT][sai_serialize_object_id(first_port)]
        .erase("SAI_PORT_ATTR_QOS_QUEUE_LIST");
    state->m_objectHash[SAI_OBJECT_TYPE_PORT][sai_serialize_object_id(second_port)]
        .erase("SAI_PORT_ATTR_QOS_QUEUE_LIST");
    queue_calls.clear();
    map_calls.clear();

    ASSERT_EQ(SAI_STATUS_SUCCESS, setSize(0));
    ASSERT_EQ(16u, queue_calls.size());
    EXPECT_EQ(16u, queue_state.size());
    for (const auto &entry : queue_state)
    {
        EXPECT_FALSE(entry.second.eligible);
        EXPECT_EQ(UINT64_MAX, entry.second.rate);
        EXPECT_EQ(UINT64_MAX, entry.second.capacity);
    }
    EXPECT_TRUE(map_calls.empty());
    EXPECT_EQ(SAI_STATUS_SUCCESS, setOid(
            SAI_OBJECT_TYPE_QUEUE, first, SAI_QUEUE_ATTR_BUFFER_PROFILE_ID, SAI_NULL_OBJECT_ID));
    EXPECT_EQ(16u, queue_calls.size());

    sai_attribute_t attr = {};
    attr.id = SAI_PORT_ATTR_QOS_QUEUE_LIST;
    sai_object_id_t queues[] = {first, second};
    attr.value.objlist = {2, queues};
    put(SAI_OBJECT_TYPE_PORT, first_port, attr);
    attr.value.objlist = {1, &third};
    put(SAI_OBJECT_TYPE_PORT, second_port, attr);
    ASSERT_EQ(SAI_STATUS_SUCCESS, setSize(256));
    EXPECT_FALSE(queue_state.at({"pg0", 1}).eligible);
    EXPECT_TRUE(queue_state.at({"pg0", 2}).eligible);
    EXPECT_TRUE(queue_state.at({"pg1", 1}).eligible);

    ASSERT_EQ(SAI_STATUS_SUCCESS, setOid(
            SAI_OBJECT_TYPE_QUEUE, second, SAI_QUEUE_ATTR_BUFFER_PROFILE_ID, SAI_NULL_OBJECT_ID));
    ASSERT_EQ(SAI_STATUS_SUCCESS, setOid(
            SAI_OBJECT_TYPE_QUEUE, third, SAI_QUEUE_ATTR_BUFFER_PROFILE_ID, SAI_NULL_OBJECT_ID));
    for (const auto &entry : queue_state)
    {
        EXPECT_FALSE(entry.second.eligible);
    }
    queue_calls.clear();
    map_calls.clear();
    EXPECT_EQ(SAI_STATUS_SUCCESS, setSize(0));
    EXPECT_EQ(16u, queue_calls.size());
    EXPECT_TRUE(map_calls.empty());
}

TEST_F(SwitchVppTrimTest, failedDisableCleanupRestoresEnabledBindings)
{
    port({trimQueue(1), trimQueue(2)});
    ASSERT_EQ(SAI_STATUS_SUCCESS, setSize(256));
    queue_calls.clear();
    map_calls.clear();
    queue_results = {-99};

    EXPECT_EQ(SAI_STATUS_FAILURE, setSize(0));
    ASSERT_EQ(3u, queue_calls.size());
    EXPECT_FALSE(queue_calls.front().eligible);
    EXPECT_TRUE(queue_state.at({"pg0", 1}).eligible);
    EXPECT_TRUE(queue_state.at({"pg0", 2}).eligible);
    EXPECT_TRUE(state->m_trim_policy.enabled);
    EXPECT_EQ(256u, state->m_trim_policy.trim_size);
    EXPECT_TRUE(global_calls.back().enabled);
    EXPECT_FALSE(state->m_trim_reconcile_pending);

    queue_calls.clear();
    map_calls.clear();
    ASSERT_EQ(SAI_STATUS_SUCCESS, setSize(0));
    EXPECT_EQ(8u, queue_calls.size());
    EXPECT_TRUE(map_calls.empty());
    EXPECT_EQ(SAI_STATUS_SUCCESS, setSize(0));
    EXPECT_EQ(8u, queue_calls.size());
}

TEST_F(SwitchVppTrimTest, failedEnableCleansPartialEligibility)
{
    port({trimQueue(1), trimQueue(2)});
    queue_results = {0, -99};

    EXPECT_EQ(SAI_STATUS_FAILURE, setSize(256));
    EXPECT_FALSE(state->m_trim_policy.enabled);
    EXPECT_FALSE(state->m_trim_reconcile_pending);
    ASSERT_EQ(10u, queue_calls.size());
    EXPECT_TRUE(queue_calls[0].eligible);
    EXPECT_TRUE(queue_calls[1].eligible);
    for (const auto &entry : queue_state)
    {
        EXPECT_FALSE(entry.second.eligible);
    }
    EXPECT_TRUE(map_calls.empty());
    EXPECT_EQ(SAI_STATUS_SUCCESS, setSize(256));
    EXPECT_TRUE(queue_state.at({"pg0", 1}).eligible);
    EXPECT_TRUE(queue_state.at({"pg0", 2}).eligible);
}

TEST_F(SwitchVppTrimTest, failedEnableCleanupIsRetriedBeforeLaterSet)
{
    port({trimQueue(1), trimQueue(2)});
    queue_results = {0, -99, -99};

    EXPECT_EQ(SAI_STATUS_FAILURE, setSize(256));
    EXPECT_FALSE(state->m_trim_policy.enabled);
    EXPECT_TRUE(state->m_trim_reconcile_pending);
    EXPECT_EQ(3u, queue_calls.size());

    queue_calls.clear();
    EXPECT_EQ(SAI_STATUS_SUCCESS, setSwitchByte(SAI_SWITCH_ATTR_PACKET_TRIM_DSCP_VALUE, 48));
    EXPECT_EQ(8u, queue_calls.size());
    EXPECT_FALSE(state->m_trim_reconcile_pending);
    EXPECT_FALSE(state->m_trim_policy.enabled);
    EXPECT_EQ(48u, state->m_trim_policy.dscp_value);
    EXPECT_TRUE(map_calls.empty());
    for (const auto &entry : queue_state)
    {
        EXPECT_FALSE(entry.second.eligible);
    }
}

TEST_F(SwitchVppTrimTest, statsCapabilityOmitsUnsourcedTrimCountersWithFilteredSizing)
{
    const std::map<sai_object_type_t, std::set<sai_stat_id_t>> unsupported = {
        {SAI_OBJECT_TYPE_SWITCH, {
            SAI_SWITCH_STAT_DROPPED_TRIM_PACKETS, SAI_SWITCH_STAT_TX_TRIM_PACKETS}},
        {SAI_OBJECT_TYPE_PORT, {
            SAI_PORT_STAT_TRIM_PACKETS, SAI_PORT_STAT_DROPPED_TRIM_PACKETS,
            SAI_PORT_STAT_TX_TRIM_PACKETS}},
        {SAI_OBJECT_TYPE_QUEUE, {
            SAI_QUEUE_STAT_TRIM_PACKETS, SAI_QUEUE_STAT_DROPPED_TRIM_PACKETS,
            SAI_QUEUE_STAT_TX_TRIM_PACKETS}}
    };
    for (const auto &object : unsupported)
    {
        SCOPED_TRACE(object.first);
        sai_stat_capability_list_t base = {};
        ASSERT_EQ(SAI_STATUS_BUFFER_OVERFLOW, state->SwitchStateBase::queryStatsCapability(
                switch_id, object.first, &base));
        std::vector<sai_stat_capability_t> entries(base.count);
        base.list = entries.data();
        ASSERT_EQ(SAI_STATUS_SUCCESS, state->SwitchStateBase::queryStatsCapability(
                switch_id, object.first, &base));
        std::vector<sai_stat_capability_t> expected;
        for (const auto &entry : entries)
        {
            if (object.second.count(entry.stat_enum) == 0)
            {
                expected.push_back(entry);
            }
        }
        ASSERT_EQ(object.second.size(), entries.size() - expected.size());
        auto count = static_cast<uint32_t>(expected.size());
        sai_stat_capability_t sentinel = {};
        sentinel.stat_enum = -1;
        for (uint32_t capacity : {0u, count - 1, count, count + 1})
        {
            SCOPED_TRACE(capacity);
            std::vector<sai_stat_capability_t> output(entries.size() + 1, sentinel);
            sai_stat_capability_list_t result = {capacity, capacity ? output.data() : nullptr};
            EXPECT_EQ(capacity < count ? SAI_STATUS_BUFFER_OVERFLOW : SAI_STATUS_SUCCESS,
                    state->queryStatsCapability(switch_id, object.first, &result));
            EXPECT_EQ(count, result.count);
            if (capacity < count)
            {
                for (const auto &entry : output)
                {
                    EXPECT_EQ(sentinel.stat_enum, entry.stat_enum);
                }
                continue;
            }
            for (size_t i = 0; i < expected.size(); ++i)
            {
                EXPECT_EQ(expected[i].stat_enum, output[i].stat_enum);
                EXPECT_EQ(expected[i].stat_modes, output[i].stat_modes);
            }
            EXPECT_EQ(sentinel.stat_enum, output[expected.size()].stat_enum);
        }
    }
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

TEST(SwitchVpp, fdbResyncRetriesFailedL2fibDump)
{
    vppMock().reset();

    auto sc = std::make_shared<SwitchConfig>(0, "");

    sc->m_saiSwitchType = SAI_SWITCH_TYPE_NPU;
    sc->m_switchType = SAI_VS_SWITCH_TYPE_VPP;
    sc->m_bootType = SAI_VS_BOOT_TYPE_COLD;
    sc->m_useTapDevice = false;
    sc->m_laneMap = LaneMap::getDefaultLaneMap();
    sc->m_eventQueue = std::make_shared<EventQueue>(std::make_shared<Signal>());

    auto scc = std::make_shared<SwitchConfigContainer>();
    scc->insert(sc);

    auto ridmgr = std::make_shared<RealObjectIdManager>(0, scc);
    auto swid = ridmgr->allocateNewSwitchObjectId("");

    SwitchVpp sw(swid, ridmgr, sc);

    // FDB notifications go to the switch's FDB event callback
    sai_attribute_t attr;
    attr.id = SAI_SWITCH_ATTR_FDB_EVENT_NOTIFY;
    attr.value.ptr = reinterpret_cast<void *>(0x1);

    ASSERT_EQ(SAI_STATUS_SUCCESS, sw.create_internal(SAI_OBJECT_TYPE_SWITCH,
                sai_serialize_object_id(swid), swid, 1, &attr));

    // A MAC learned from VPP, whose DELETE VPP sent while the event socket was down
    SwitchVpp::VppFdbKey key = {{0x02, 0x11, 0x22, 0x33, 0x44, 0x55}, 1000};

    FdbInfo fi;
    fi.m_fdbEntry.switch_id = swid;
    fi.m_fdbEntry.bv_id = ridmgr->allocateNewObjectId(SAI_OBJECT_TYPE_VLAN, swid);
    memcpy(fi.m_fdbEntry.mac_address, key.mac, sizeof(sai_mac_t));
    fi.setVlanId(static_cast<sai_vlan_id_t>(key.bd_id));
    fi.setPortId(ridmgr->allocateNewObjectId(SAI_OBJECT_TYPE_PORT, swid));
    fi.setBridgePortId(ridmgr->allocateNewObjectId(SAI_OBJECT_TYPE_BRIDGE_PORT, swid));

    sai_attribute_t attrs[2];
    attrs[0].id = SAI_FDB_ENTRY_ATTR_TYPE;
    attrs[0].value.s32 = SAI_FDB_ENTRY_TYPE_DYNAMIC;
    attrs[1].id = SAI_FDB_ENTRY_ATTR_BRIDGE_PORT_ID;
    attrs[1].value.oid = fi.getBridgePortId();

    auto fdbId = sai_serialize_fdb_entry(fi.getFdbEntry());
    auto &fdbs = sw.m_objectHash.at(SAI_OBJECT_TYPE_FDB_ENTRY);

    ASSERT_EQ(SAI_STATUS_SUCCESS, sw.create_internal(SAI_OBJECT_TYPE_FDB_ENTRY, fdbId, swid, 2, attrs));
    sw.m_fdb_info_set.insert(fi);
    sw.m_vpp_fdb_entries[key] = 5;

    // The event socket reconnected
    sw.m_mac_event_queue.push({{0}, ~0u, VPP_MAC_ACTION_RESYNC});

    // The L2FIB dump fails: nothing changes and the resync stays pending
    vppMock().l2fibDumpResult = -1;

    sw.processFdbEntriesForAging();

    EXPECT_EQ(1u, vppMock().l2fibDumpCalls);
    EXPECT_EQ(1u, sw.m_vpp_fdb_entries.count(key));
    EXPECT_EQ(1u, sw.m_fdb_info_set.size());
    EXPECT_EQ(1u, fdbs.count(fdbId));
    EXPECT_EQ(0u, sc->m_eventQueue->size());
    ASSERT_EQ(1u, sw.m_mac_event_queue.size());
    EXPECT_EQ(static_cast<uint8_t>(VPP_MAC_ACTION_RESYNC), sw.m_mac_event_queue.front().action);

    // The next aging pass dumps the L2FIB, which no longer has the MAC: it is aged out
    vppMock().l2fibDumpResult = 0;

    sw.processFdbEntriesForAging();

    EXPECT_EQ(2u, vppMock().l2fibDumpCalls);
    EXPECT_TRUE(sw.m_mac_event_queue.empty());
    EXPECT_TRUE(sw.m_vpp_fdb_entries.empty());
    EXPECT_TRUE(sw.m_fdb_info_set.empty());
    EXPECT_TRUE(fdbs.empty());

    ASSERT_EQ(1u, sc->m_eventQueue->size());

    auto event = sc->m_eventQueue->dequeue();

    ASSERT_EQ(EVENT_TYPE_NOTIFICATION, event->getType());

    auto ntf = std::dynamic_pointer_cast<EventPayloadNotification>(event->getPayload())->getNotification();

    EXPECT_EQ(SAI_SWITCH_NOTIFICATION_TYPE_FDB_EVENT, ntf->getNotificationType());
    EXPECT_NE(std::string::npos, ntf->getSerializedNotification().find("SAI_FDB_EVENT_AGED"));
    EXPECT_NE(std::string::npos, ntf->getSerializedNotification().find("02:11:22:33:44:55"));
}

TEST(SwitchVpp, fdbResyncSkipsL2fibRowsFromAnEarlierBd)
{
    vppMock().reset();

    auto sc = std::make_shared<SwitchConfig>(0, "");

    sc->m_saiSwitchType = SAI_SWITCH_TYPE_NPU;
    sc->m_switchType = SAI_VS_SWITCH_TYPE_VPP;
    sc->m_bootType = SAI_VS_BOOT_TYPE_COLD;
    sc->m_useTapDevice = false;
    sc->m_laneMap = LaneMap::getDefaultLaneMap();
    sc->m_eventQueue = std::make_shared<EventQueue>(std::make_shared<Signal>());

    auto scc = std::make_shared<SwitchConfigContainer>();
    scc->insert(sc);

    auto ridmgr = std::make_shared<RealObjectIdManager>(0, scc);
    auto swid = ridmgr->allocateNewSwitchObjectId("");

    SwitchVpp sw(swid, ridmgr, sc);

    sai_attribute_t attr;
    attr.id = SAI_SWITCH_ATTR_FDB_EVENT_NOTIFY;
    attr.value.ptr = reinterpret_cast<void *>(0x1);

    ASSERT_EQ(SAI_STATUS_SUCCESS, sw.create_internal(SAI_OBJECT_TYPE_SWITCH,
                sai_serialize_object_id(swid), swid, 1, &attr));

    // A port that has just moved from VLAN 100 to VLAN 200
    auto port = ridmgr->allocateNewObjectId(SAI_OBJECT_TYPE_PORT, swid);

    ASSERT_NE(nullptr, sw.m_ifaceRegistry.addPhysicalPort("bobm1", "Ethernet4", port));
    ASSERT_TRUE(sw.m_ifaceRegistry.bindSwIfIndex("bobm1", 5));
    ASSERT_TRUE(sw.m_ifaceRegistry.setBdId("bobm1", 200));

    // A MAC learned on it in VLAN 200, whose DELETE VPP sent while the event socket was down
    SwitchVpp::VppFdbKey key = {{0x02, 0x11, 0x22, 0x33, 0x44, 0x55}, 200};

    FdbInfo fi;
    fi.m_fdbEntry.switch_id = swid;
    fi.m_fdbEntry.bv_id = ridmgr->allocateNewObjectId(SAI_OBJECT_TYPE_VLAN, swid);
    memcpy(fi.m_fdbEntry.mac_address, key.mac, sizeof(sai_mac_t));
    fi.setVlanId(static_cast<sai_vlan_id_t>(key.bd_id));
    fi.setPortId(port);
    fi.setBridgePortId(ridmgr->allocateNewObjectId(SAI_OBJECT_TYPE_BRIDGE_PORT, swid));

    sai_attribute_t attrs[2];
    attrs[0].id = SAI_FDB_ENTRY_ATTR_TYPE;
    attrs[0].value.s32 = SAI_FDB_ENTRY_TYPE_DYNAMIC;
    attrs[1].id = SAI_FDB_ENTRY_ATTR_BRIDGE_PORT_ID;
    attrs[1].value.oid = fi.getBridgePortId();

    auto fdbId = sai_serialize_fdb_entry(fi.getFdbEntry());
    auto &fdbs = sw.m_objectHash.at(SAI_OBJECT_TYPE_FDB_ENTRY);

    ASSERT_EQ(SAI_STATUS_SUCCESS, sw.create_internal(SAI_OBJECT_TYPE_FDB_ENTRY, fdbId, swid, 2, attrs));
    sw.m_fdb_info_set.insert(fi);
    sw.m_vpp_fdb_entries[key] = 5;

    // VPP's only row for the MAC is from VLAN 100: flushed when the port left
    // that VLAN, and not yet deleted by VPP's aging scan
    vpp_l2fib_entry_t row = {};
    memcpy(row.mac, key.mac, sizeof(row.mac));
    row.bd_id = 100;
    row.sw_if_index = 5;

    vppMock().l2fib = {row};

    // The event socket reconnected: that row does not keep the MAC in VLAN 200
    sw.m_mac_event_queue.push({{0}, ~0u, VPP_MAC_ACTION_RESYNC});
    sw.processFdbEntriesForAging();

    EXPECT_EQ(1u, vppMock().l2fibDumpCalls);
    EXPECT_TRUE(sw.m_mac_event_queue.empty());
    EXPECT_TRUE(sw.m_vpp_fdb_entries.empty());
    EXPECT_TRUE(sw.m_fdb_info_set.empty());
    EXPECT_TRUE(fdbs.empty());

    ASSERT_EQ(1u, sc->m_eventQueue->size());

    auto event = sc->m_eventQueue->dequeue();

    ASSERT_EQ(EVENT_TYPE_NOTIFICATION, event->getType());

    auto ntf = std::dynamic_pointer_cast<EventPayloadNotification>(event->getPayload())->getNotification();

    EXPECT_EQ(SAI_SWITCH_NOTIFICATION_TYPE_FDB_EVENT, ntf->getNotificationType());
    EXPECT_NE(std::string::npos, ntf->getSerializedNotification().find("SAI_FDB_EVENT_AGED"));
    EXPECT_NE(std::string::npos, ntf->getSerializedNotification().find("02:11:22:33:44:55"));
}

TEST(SwitchVpp, bfdStateEventAppliedOnEventQueueThread)
{
    auto sc = std::make_shared<SwitchConfig>(0, "");

    sc->m_saiSwitchType = SAI_SWITCH_TYPE_NPU;
    sc->m_switchType = SAI_VS_SWITCH_TYPE_VPP;
    sc->m_bootType = SAI_VS_BOOT_TYPE_COLD;
    sc->m_useTapDevice = false;
    sc->m_laneMap = LaneMap::getDefaultLaneMap();
    sc->m_eventQueue = std::make_shared<EventQueue>(std::make_shared<Signal>());

    auto scc = std::make_shared<SwitchConfigContainer>();
    scc->insert(sc);

    auto ridmgr = std::make_shared<RealObjectIdManager>(0, scc);
    auto swid = ridmgr->allocateNewSwitchObjectId("");

    SwitchVpp sw(swid, ridmgr, sc);

    // BFD notifications go to the switch's BFD session state change callback
    sai_attribute_t attr;
    attr.id = SAI_SWITCH_ATTR_BFD_SESSION_STATE_CHANGE_NOTIFY;
    attr.value.ptr = reinterpret_cast<void *>(0x1);

    ASSERT_EQ(SAI_STATUS_SUCCESS, sw.create_internal(SAI_OBJECT_TYPE_SWITCH,
                sai_serialize_object_id(swid), swid, 1, &attr));

    // A BFD session that is DOWN
    auto bfdOid = ridmgr->allocateNewObjectId(SAI_OBJECT_TYPE_BFD_SESSION, swid);

    attr.id = SAI_BFD_SESSION_ATTR_STATE;
    attr.value.s32 = SAI_BFD_SESSION_STATE_DOWN;

    ASSERT_EQ(SAI_STATUS_SUCCESS, sw.create_internal(SAI_OBJECT_TYPE_BFD_SESSION,
                sai_serialize_object_id(bfdOid), swid, 1, &attr));

    auto bfdState = [&sw, bfdOid]()
    {
        sai_attribute_t stateAttr;
        stateAttr.id = SAI_BFD_SESSION_ATTR_STATE;

        EXPECT_EQ(SAI_STATUS_SUCCESS, sw.get(SAI_OBJECT_TYPE_BFD_SESSION, bfdOid, 1, &stateAttr));

        return static_cast<sai_bfd_session_state_t>(stateAttr.value.s32);
    };

    // VPP reports the session UP
    auto *ev = static_cast<vpp_event_info_t *>(calloc(1, sizeof(vpp_event_info_t)));

    ASSERT_NE(nullptr, ev);

    auto &notif = ev->data.bfd_notif;

    ev->type = VPP_BFD_STATE_CHANGE;
    notif.state = VPP_API_BFD_STATE_UP;
    notif.local_addr.sa_family = AF_INET;
    notif.peer_addr.sa_family = AF_INET;

    ASSERT_EQ(1, inet_pton(AF_INET, "10.0.0.1", &notif.local_addr.addr.ip4.sin_addr));
    ASSERT_EQ(1, inet_pton(AF_INET, "10.0.0.2", &notif.peer_addr.addr.ip4.sin_addr));

    SwitchVpp::vpp_bfd_info_t key;
    memset(&key, 0, sizeof(key));
    vpp_ip_addr_t_to_sai_ip_address_t(notif.local_addr, key.local_addr);
    vpp_ip_addr_t_to_sai_ip_address_t(notif.peer_addr, key.peer_addr);
    sw.m_bfd_info_map[key] = bfdOid;

    // The VPP events thread takes the event (and frees it)
    deliverVppEvents({ev});

    ASSERT_TRUE(waitForVppEventsTaken());

    // It touches neither Meta nor the switch state, which need m_apimutex: it
    // only posts the event, for the vslib event queue thread to apply
    EXPECT_EQ(SAI_BFD_SESSION_STATE_DOWN, bfdState());

    ASSERT_EQ(1u, sc->m_eventQueue->size());

    auto event = sc->m_eventQueue->dequeue();

    ASSERT_EQ(EVENT_TYPE_PLATFORM_EVENT, event->getType());

    auto payload = std::dynamic_pointer_cast<EventPayloadVppEvent>(event->getPayload());

    ASSERT_NE(nullptr, payload);
    EXPECT_EQ(swid, payload->getSwitchId());
    EXPECT_EQ(VPP_BFD_STATE_CHANGE, payload->getEvent().type);
    EXPECT_EQ(nullptr, payload->getEvent().next);

    // The event queue thread applies it (with m_apimutex held)
    sw.syncOnPlatformEvent(payload);

    EXPECT_EQ(SAI_BFD_SESSION_STATE_UP, bfdState());

    ASSERT_EQ(1u, sc->m_eventQueue->size());

    event = sc->m_eventQueue->dequeue();

    ASSERT_EQ(EVENT_TYPE_NOTIFICATION, event->getType());

    auto ntf = std::dynamic_pointer_cast<EventPayloadNotification>(event->getPayload())->getNotification();

    EXPECT_EQ(SAI_SWITCH_NOTIFICATION_TYPE_BFD_SESSION_STATE_CHANGE, ntf->getNotificationType());
    EXPECT_NE(std::string::npos, ntf->getSerializedNotification().find("SAI_BFD_SESSION_STATE_UP"));
}

TEST(SwitchVpp, syncOnPlatformEventDropsUnusableEvents)
{
    auto sc = std::make_shared<SwitchConfig>(0, "");

    sc->m_saiSwitchType = SAI_SWITCH_TYPE_NPU;
    sc->m_switchType = SAI_VS_SWITCH_TYPE_VPP;
    sc->m_bootType = SAI_VS_BOOT_TYPE_COLD;
    sc->m_useTapDevice = false;
    sc->m_laneMap = LaneMap::getDefaultLaneMap();
    sc->m_eventQueue = std::make_shared<EventQueue>(std::make_shared<Signal>());

    auto scc = std::make_shared<SwitchConfigContainer>();
    scc->insert(sc);

    auto ridmgr = std::make_shared<RealObjectIdManager>(0, scc);
    auto swid = ridmgr->allocateNewSwitchObjectId("");

    SwitchVpp sw(swid, ridmgr, sc);

    sai_attribute_t attr;
    attr.id = SAI_SWITCH_ATTR_PORT_STATE_CHANGE_NOTIFY;
    attr.value.ptr = reinterpret_cast<void *>(0x1);

    ASSERT_EQ(SAI_STATUS_SUCCESS, sw.create_internal(SAI_OBJECT_TYPE_SWITCH,
                sai_serialize_object_id(swid), swid, 1, &attr));

    // A platform event that is not a VPP event
    sw.syncOnPlatformEvent(std::make_shared<EventPayloadPlatformEvent>(swid));

    // A link event for an interface that is not a port
    vpp_event_info_t ev;
    memset(&ev, 0, sizeof(ev));

    ev.type = VPP_INTF_LINK_STATUS;
    strncpy(ev.data.intf_status.hwif_name, "bvi1000", sizeof(ev.data.intf_status.hwif_name) - 1);
    ev.data.intf_status.link_up = true;

    sw.syncOnPlatformEvent(std::make_shared<EventPayloadVppEvent>(swid, ev));

    EXPECT_EQ(0u, sc->m_eventQueue->size());
}

/*
 * Link events VPP sent while the event socket was down are lost, so a reconnect
 * queues a link state resync behind the events read before the failure. A stale
 * DOWN read before the failure must not win over that resync, which reads the
 * link back UP. And the resync compares with what SONiC was last told, not with
 * what an earlier resync read.
 */
TEST(SwitchVpp, linkResyncFollowsQueuedLinkEvents)
{
    vppMock().reset();

    auto sc = std::make_shared<SwitchConfig>(0, "");

    sc->m_saiSwitchType = SAI_SWITCH_TYPE_NPU;
    sc->m_switchType = SAI_VS_SWITCH_TYPE_VPP;
    sc->m_bootType = SAI_VS_BOOT_TYPE_COLD;
    sc->m_useTapDevice = false;
    sc->m_laneMap = LaneMap::getDefaultLaneMap();
    sc->m_eventQueue = std::make_shared<EventQueue>(std::make_shared<Signal>());

    auto scc = std::make_shared<SwitchConfigContainer>();
    scc->insert(sc);

    auto ridmgr = std::make_shared<RealObjectIdManager>(0, scc);
    auto swid = ridmgr->allocateNewSwitchObjectId("");

    SwitchVpp sw(swid, ridmgr, sc);

    sai_attribute_t attr;
    attr.id = SAI_SWITCH_ATTR_PORT_STATE_CHANGE_NOTIFY;
    attr.value.ptr = reinterpret_cast<void *>(0x1);

    ASSERT_EQ(SAI_STATUS_SUCCESS, sw.create_internal(SAI_OBJECT_TYPE_SWITCH,
                sai_serialize_object_id(swid), swid, 1, &attr));

    // Port Ethernet4 is UP, with its host interface and its VPP interface
    auto port = ridmgr->allocateNewObjectId(SAI_OBJECT_TYPE_PORT, swid);

    attr.id = SAI_PORT_ATTR_OPER_STATUS;
    attr.value.s32 = SAI_PORT_OPER_STATUS_UP;

    ASSERT_EQ(SAI_STATUS_SUCCESS, sw.create_internal(SAI_OBJECT_TYPE_PORT,
                sai_serialize_object_id(port), swid, 1, &attr));

    ASSERT_NE(nullptr, sw.m_ifaceRegistry.addPhysicalPort("bobm1", "Ethernet4", port));

    // what sonic_vpp_ifmap.ini maps the host interface to
    sw.m_ifaceRegistry.m_osIfToHwIf["Ethernet4"] = "bobm1";
    sw.m_ifaceRegistry.m_ifMapLoaded = true;

    sw.m_hostif_info_map["Ethernet4"] = std::make_shared<HostInterfaceInfo>(0, -1, -1, "Ethernet4", port, sc->m_eventQueue);

    vppMock().linkUp["bobm1"] = true;

    auto operStatus = [&sw, port]()
    {
        sai_attribute_t statusAttr;
        statusAttr.id = SAI_PORT_ATTR_OPER_STATUS;

        EXPECT_EQ(SAI_STATUS_SUCCESS, sw.get(SAI_OBJECT_TYPE_PORT, port, 1, &statusAttr));

        return static_cast<sai_port_oper_status_t>(statusAttr.value.s32);
    };

    auto vppEvent = [](vpp_event_type_e type)
    {
        auto *ev = static_cast<vpp_event_info_t *>(calloc(1, sizeof(vpp_event_info_t)));

        ev->type = type;

        return ev;
    };

    auto *linkDown = vppEvent(VPP_INTF_LINK_STATUS);

    snprintf(linkDown->data.intf_status.hwif_name, sizeof(linkDown->data.intf_status.hwif_name), "%s", "bobm1");
    linkDown->data.intf_status.link_up = false;

    // An earlier reconnect's resync finds nothing to fix. Then the event thread
    // reads a link DOWN, the event socket fails, the link comes back UP while it
    // is down, and the reconnect queues a resync behind the DOWN.
    deliverVppEvents({vppEvent(VPP_INTF_LINK_RESYNC), linkDown, vppEvent(VPP_INTF_LINK_RESYNC)});

    ASSERT_TRUE(waitForVppEventsTaken());

    // The event thread posts them in that order...
    std::vector<std::shared_ptr<EventPayloadVppEvent>> posted;

    while (auto event = sc->m_eventQueue->dequeue())
    {
        ASSERT_EQ(EVENT_TYPE_PLATFORM_EVENT, event->getType());

        posted.push_back(std::dynamic_pointer_cast<EventPayloadVppEvent>(event->getPayload()));
    }

    ASSERT_EQ(3u, posted.size());
    EXPECT_EQ(VPP_INTF_LINK_RESYNC, posted[0]->getEvent().type);
    EXPECT_EQ(VPP_INTF_LINK_STATUS, posted[1]->getEvent().type);
    EXPECT_EQ(VPP_INTF_LINK_RESYNC, posted[2]->getEvent().type);

    // ...and the event queue thread applies them in that order
    std::vector<std::string> notified;

    auto apply = [&sw, &sc, &notified](std::shared_ptr<EventPayloadVppEvent> payload)
    {
        sw.syncOnPlatformEvent(payload);

        while (auto event = sc->m_eventQueue->dequeue())
        {
            EXPECT_EQ(EVENT_TYPE_NOTIFICATION, event->getType());

            auto ntf = std::dynamic_pointer_cast<EventPayloadNotification>(event->getPayload())->getNotification();

            EXPECT_EQ(SAI_SWITCH_NOTIFICATION_TYPE_PORT_STATE_CHANGE, ntf->getNotificationType());

            notified.push_back(ntf->getSerializedNotification());
        }
    };

    apply(posted[0]);

    EXPECT_TRUE(notified.empty());
    EXPECT_EQ(SAI_PORT_OPER_STATUS_UP, operStatus());

    apply(posted[1]);

    EXPECT_EQ(SAI_PORT_OPER_STATUS_DOWN, operStatus());

    apply(posted[2]);

    EXPECT_EQ(SAI_PORT_OPER_STATUS_UP, operStatus());

    ASSERT_EQ(2u, notified.size());
    EXPECT_NE(std::string::npos, notified[0].find("SAI_PORT_OPER_STATUS_DOWN"));
    EXPECT_NE(std::string::npos, notified[1].find("SAI_PORT_OPER_STATUS_UP"));
}

namespace
{
    std::shared_ptr<SwitchConfig> vppSwitchConfig()
    {
        SWSS_LOG_ENTER();

        auto sc = std::make_shared<SwitchConfig>(0, "");

        sc->m_saiSwitchType = SAI_SWITCH_TYPE_NPU;
        sc->m_switchType = SAI_VS_SWITCH_TYPE_VPP;
        sc->m_bootType = SAI_VS_BOOT_TYPE_COLD;
        sc->m_useTapDevice = false;
        sc->m_laneMap = LaneMap::getDefaultLaneMap();
        sc->m_eventQueue = std::make_shared<EventQueue>(std::make_shared<Signal>());

        return sc;
    }
}

/*
 * The event thread reads what VPP has sent as soon as it wakes up for it, and
 * pings VPP, to check the event socket, only after an interval with nothing to
 * read.
 */
TEST(SwitchVpp, eventThreadPingsOnlyAfterAQuietInterval)
{
    resetVppEventCalls();

    // An event is waiting when the event thread starts
    auto *ev = static_cast<vpp_event_info_t *>(calloc(1, sizeof(vpp_event_info_t)));

    ASSERT_NE(nullptr, ev);

    ev->type = VPP_INTF_LINK_RESYNC;

    deliverVppEvents({ev});

    {
        auto sc = vppSwitchConfig();
        auto scc = std::make_shared<SwitchConfigContainer>();
        scc->insert(sc);

        auto ridmgr = std::make_shared<RealObjectIdManager>(0, scc);

        SwitchVpp sw(ridmgr->allocateNewSwitchObjectId(""), ridmgr, sc);

        ASSERT_TRUE(waitForVppEventsTaken());
    }

    // The switch is gone, and its event thread with it
    auto calls = vppEventCalls();

    unsigned int drains = 0;
    unsigned int pings = 0;

    for (size_t i = 0; i < calls.size(); i++)
    {
        if (calls[i].api != "vpp_wait_for_events")
        {
            continue;
        }

        ASSERT_LT(i + 1, calls.size());

        if (calls[i].result > 0)
        {
            EXPECT_EQ("vpp_drain_events", calls[i + 1].api);
            drains++;
        }
        else
        {
            EXPECT_EQ("vpp_sync_for_events", calls[i + 1].api);
            pings++;
        }
    }

    EXPECT_EQ(1u, drains);
    EXPECT_LE(1u, pings);
}

/*
 * A failure of the event socket reconnects it. Were the next attempt to follow
 * at once, a socket that keeps failing would make the event thread spin: it
 * tries again only an interval later.
 */
TEST(SwitchVpp, eventThreadRetriesAFailedSocketAnIntervalLater)
{
    // The first ping fails
    resetVppEventCalls(1);

    auto calls = vppEventCalls();

    auto failed = [&calls]()
    {
        return std::find_if(calls.begin(), calls.end(),
                [](const VppEventCall& call) { return call.result < 0; });
    };

    {
        auto sc = vppSwitchConfig();
        auto scc = std::make_shared<SwitchConfigContainer>();
        scc->insert(sc);

        auto ridmgr = std::make_shared<RealObjectIdManager>(0, scc);

        SwitchVpp sw(ridmgr->allocateNewSwitchObjectId(""), ridmgr, sc);

        // Until the event thread makes its next call after the failure
        for (int i = 0; i < 500; i++)
        {
            calls = vppEventCalls();

            auto it = failed();

            if (it != calls.end() && it + 1 != calls.end())
            {
                break;
            }

            usleep(10 * 1000);
        }
    }

    auto it = failed();

    ASSERT_NE(calls.end(), it);
    ASSERT_NE(calls.end(), it + 1);

    // It pings again, which reconnects the socket if it is still down, an
    // interval later and without waiting on the socket in between
    EXPECT_EQ("vpp_sync_for_events", (it + 1)->api);
    EXPECT_LE(std::chrono::milliseconds(1900), (it + 1)->time - it->time);
}

/*
 * BFD events lost with the event socket, sent while it was down or still
 * buffered on it when it failed, leave a session's state stale: the reconnect
 * queues a BFD state resync, which reports the sessions whose state VPP has
 * changed since. If VPP's sessions can't be dumped, the FDB aging pass retries.
 */
TEST(SwitchVpp, bfdResyncReportsStateChangesLostWithTheEventSocket)
{
    vppMock().reset();

    auto sc = vppSwitchConfig();
    auto scc = std::make_shared<SwitchConfigContainer>();
    scc->insert(sc);

    auto ridmgr = std::make_shared<RealObjectIdManager>(0, scc);
    auto swid = ridmgr->allocateNewSwitchObjectId("");

    SwitchVpp sw(swid, ridmgr, sc);

    sai_attribute_t attr;
    attr.id = SAI_SWITCH_ATTR_BFD_SESSION_STATE_CHANGE_NOTIFY;
    attr.value.ptr = reinterpret_cast<void *>(0x1);

    ASSERT_EQ(SAI_STATUS_SUCCESS, sw.create_internal(SAI_OBJECT_TYPE_SWITCH,
                sai_serialize_object_id(swid), swid, 1, &attr));

    // A BFD session, UP in SONiC or without a state change yet, and how VPP reports it
    auto addSession = [&](const char *peer, bool up, vpp_api_bfd_state_e vppState)
    {
        auto oid = ridmgr->allocateNewObjectId(SAI_OBJECT_TYPE_BFD_SESSION, swid);

        sai_attribute_t stateAttr;
        stateAttr.id = SAI_BFD_SESSION_ATTR_STATE;
        stateAttr.value.s32 = SAI_BFD_SESSION_STATE_UP;

        EXPECT_EQ(SAI_STATUS_SUCCESS, sw.create_internal(SAI_OBJECT_TYPE_BFD_SESSION,
                    sai_serialize_object_id(oid), swid, up ? 1 : 0, &stateAttr));

        vpp_bfd_state_notif_t session;
        memset(&session, 0, sizeof(session));

        session.multihop = true;
        session.sw_if_index = ~0u;
        session.state = vppState;
        session.local_addr.sa_family = AF_INET;
        session.peer_addr.sa_family = AF_INET;

        EXPECT_EQ(1, inet_pton(AF_INET, "10.0.0.1", &session.local_addr.addr.ip4.sin_addr));
        EXPECT_EQ(1, inet_pton(AF_INET, peer, &session.peer_addr.addr.ip4.sin_addr));

        SwitchVpp::vpp_bfd_info_t key;
        memset(&key, 0, sizeof(key));
        key.multihop = true;
        vpp_ip_addr_t_to_sai_ip_address_t(session.local_addr, key.local_addr);
        vpp_ip_addr_t_to_sai_ip_address_t(session.peer_addr, key.peer_addr);
        sw.m_bfd_info_map[key] = oid;

        vppMock().bfdSessions.push_back(session);

        return oid;
    };

    // While the event socket was down, VPP took a session DOWN and a new one
    // UP. It still has another session UP, and a new one DOWN.
    auto wentDown = addSession("10.0.0.2", true, VPP_API_BFD_STATE_DOWN);
    auto stayedUp = addSession("10.0.0.3", true, VPP_API_BFD_STATE_UP);
    auto cameUp = addSession("10.0.0.4", false, VPP_API_BFD_STATE_UP);
    auto stayedDown = addSession("10.0.0.5", false, VPP_API_BFD_STATE_DOWN);

    auto bfdState = [&sw](sai_object_id_t oid)
    {
        sai_attribute_t stateAttr;
        stateAttr.id = SAI_BFD_SESSION_ATTR_STATE;

        EXPECT_EQ(SAI_STATUS_SUCCESS, sw.get(SAI_OBJECT_TYPE_BFD_SESSION, oid, 1, &stateAttr));

        return static_cast<sai_bfd_session_state_t>(stateAttr.value.s32);
    };

    vpp_event_info_t resync;
    memset(&resync, 0, sizeof(resync));
    resync.type = VPP_BFD_RESYNC;

    // If the session dump fails, nothing changes, and the next FDB aging pass
    // tries again
    vppMock().bfdDumpResult = -1;

    sw.syncOnPlatformEvent(std::make_shared<EventPayloadVppEvent>(swid, resync));

    EXPECT_EQ(1u, vppMock().bfdDumpCalls);
    EXPECT_EQ(SAI_BFD_SESSION_STATE_UP, bfdState(wentDown));
    EXPECT_EQ(0u, sc->m_eventQueue->size());

    sw.processFdbEntriesForAging();

    EXPECT_EQ(2u, vppMock().bfdDumpCalls);
    EXPECT_EQ(SAI_BFD_SESSION_STATE_UP, bfdState(wentDown));
    EXPECT_EQ(0u, sc->m_eventQueue->size());

    // Once the dump succeeds, the resync reports the sessions whose state
    // changed, and only those
    vppMock().bfdDumpResult = 0;

    sw.processFdbEntriesForAging();

    EXPECT_EQ(3u, vppMock().bfdDumpCalls);
    EXPECT_EQ(SAI_BFD_SESSION_STATE_DOWN, bfdState(wentDown));
    EXPECT_EQ(SAI_BFD_SESSION_STATE_UP, bfdState(stayedUp));
    EXPECT_EQ(SAI_BFD_SESSION_STATE_UP, bfdState(cameUp));

    std::vector<std::string> notified;

    while (auto event = sc->m_eventQueue->dequeue())
    {
        ASSERT_EQ(EVENT_TYPE_NOTIFICATION, event->getType());

        auto ntf = std::dynamic_pointer_cast<EventPayloadNotification>(event->getPayload())->getNotification();

        EXPECT_EQ(SAI_SWITCH_NOTIFICATION_TYPE_BFD_SESSION_STATE_CHANGE, ntf->getNotificationType());

        notified.push_back(ntf->getSerializedNotification());
    }

    ASSERT_EQ(2u, notified.size());
    EXPECT_NE(std::string::npos, notified[0].find(sai_serialize_object_id(wentDown)));
    EXPECT_NE(std::string::npos, notified[0].find("SAI_BFD_SESSION_STATE_DOWN"));
    EXPECT_NE(std::string::npos, notified[1].find(sai_serialize_object_id(cameUp)));
    EXPECT_NE(std::string::npos, notified[1].find("SAI_BFD_SESSION_STATE_UP"));

    for (const auto& ntf: notified)
    {
        EXPECT_EQ(std::string::npos, ntf.find(sai_serialize_object_id(stayedDown)));
    }

    // Done: the aging pass doesn't resync again
    sw.processFdbEntriesForAging();

    EXPECT_EQ(3u, vppMock().bfdDumpCalls);
    EXPECT_EQ(0u, sc->m_eventQueue->size());
}
