#include "MockVppXlate.h"
#include "vpp/SwitchVpp.h"
#include "ContextConfigContainer.h"
#include "SwitchConfigContainer.h"
#include "meta/sai_serialize.h"
#include "saivs.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <deque>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

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

TEST(SwitchVpp, getTrimEnumValuesCapability)
{
    sai_int32_t value = -1;

    // The DSCP resolution mode advertises only DSCP_VALUE (symmetric trim); the
    // asymmetric FROM_TC mode is intentionally not offered on VPP.
    ASSERT_TRUE(SwitchVppTrim::getTrimEnumValuesCapability(
        SAI_OBJECT_TYPE_SWITCH, SAI_SWITCH_ATTR_PACKET_TRIM_DSCP_RESOLUTION_MODE, &value));
    EXPECT_EQ((sai_int32_t)SAI_PACKET_TRIM_DSCP_RESOLUTION_MODE_DSCP_VALUE, value);

    // The queue resolution mode advertises only STATIC.
    value = -1;
    ASSERT_TRUE(SwitchVppTrim::getTrimEnumValuesCapability(
        SAI_OBJECT_TYPE_SWITCH, SAI_SWITCH_ATTR_PACKET_TRIM_QUEUE_RESOLUTION_MODE, &value));
    EXPECT_EQ((sai_int32_t)SAI_PACKET_TRIM_QUEUE_RESOLUTION_MODE_STATIC, value);

    // Unrelated switch enum attributes fall through to the base metadata.
    EXPECT_FALSE(SwitchVppTrim::getTrimEnumValuesCapability(
        SAI_OBJECT_TYPE_SWITCH, SAI_SWITCH_ATTR_ECMP_HASH, &value));

    // A trim switch attr queried against the wrong object type must not match.
    EXPECT_FALSE(SwitchVppTrim::getTrimEnumValuesCapability(
        SAI_OBJECT_TYPE_BUFFER_PROFILE, SAI_SWITCH_ATTR_PACKET_TRIM_DSCP_RESOLUTION_MODE, &value));

    // A null out-pointer is tolerated (predicate still resolves).
    EXPECT_TRUE(SwitchVppTrim::getTrimEnumValuesCapability(
        SAI_OBJECT_TYPE_SWITCH, SAI_SWITCH_ATTR_PACKET_TRIM_QUEUE_RESOLUTION_MODE, nullptr));
}

TEST(SwitchVpp, isTrimDataplaneAttr)
{
    // Queue buffer-profile / scheduler bindings drive per-queue admission.
    EXPECT_TRUE(SwitchVppTrim::isTrimDataplaneAttr(
        SAI_OBJECT_TYPE_QUEUE, SAI_QUEUE_ATTR_BUFFER_PROFILE_ID));
    EXPECT_TRUE(SwitchVppTrim::isTrimDataplaneAttr(
        SAI_OBJECT_TYPE_QUEUE, SAI_QUEUE_ATTR_SCHEDULER_PROFILE_ID));
    EXPECT_TRUE(SwitchVppTrim::isTrimDataplaneAttr(
        SAI_OBJECT_TYPE_BUFFER_PROFILE, SAI_BUFFER_PROFILE_ATTR_PACKET_ADMISSION_FAIL_ACTION));
    EXPECT_TRUE(SwitchVppTrim::isTrimDataplaneAttr(
        SAI_OBJECT_TYPE_SCHEDULER_GROUP, SAI_SCHEDULER_GROUP_ATTR_SCHEDULER_PROFILE_ID));
    EXPECT_TRUE(SwitchVppTrim::isTrimDataplaneAttr(
        SAI_OBJECT_TYPE_SCHEDULER, SAI_SCHEDULER_ATTR_MAX_BANDWIDTH_RATE));

    // Port map (re)bindings and in-place map edits re-resolve the
    // switch-global DSCP->queue trim table.
    EXPECT_TRUE(SwitchVppTrim::isTrimDataplaneAttr(
        SAI_OBJECT_TYPE_PORT, SAI_PORT_ATTR_QOS_DSCP_TO_TC_MAP));
    EXPECT_TRUE(SwitchVppTrim::isTrimDataplaneAttr(
        SAI_OBJECT_TYPE_PORT, SAI_PORT_ATTR_QOS_TC_TO_QUEUE_MAP));
    EXPECT_TRUE(SwitchVppTrim::isTrimDataplaneAttr(
        SAI_OBJECT_TYPE_QOS_MAP, SAI_QOS_MAP_ATTR_MAP_TO_VALUE_LIST));

    // Unrelated attributes must not trigger a trim refresh.
    EXPECT_FALSE(SwitchVppTrim::isTrimDataplaneAttr(
        SAI_OBJECT_TYPE_QUEUE, SAI_QUEUE_ATTR_TYPE));
    EXPECT_FALSE(SwitchVppTrim::isTrimDataplaneAttr(
        SAI_OBJECT_TYPE_PORT, SAI_PORT_ATTR_MTU));
    EXPECT_TRUE(SwitchVppTrim::isTrimDataplaneAttr(
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
        ASSERT_TRUE(state->m_trim.m_trim_reconcile_pending);
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
    EXPECT_FALSE(state->m_trim.m_trim_policy.enabled);
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
    EXPECT_FALSE(state->m_trim.m_trim_policy.enabled);
    EXPECT_FALSE(state->m_trim.m_trim_reconcile_pending);
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
    ASSERT_TRUE(state->m_trim.m_trim_reconcile_pending);
    state.reset();
    state = std::make_shared<SwitchVpp>(switch_id, manager, config);
    stopEvents(state);
    ASSERT_FALSE(state->m_trim.m_trim_reconcile_pending);
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
    EXPECT_TRUE(state->m_trim.m_trim_reconcile_pending);
    EXPECT_EQ(SAI_STATUS_SUCCESS, state->create(SAI_OBJECT_TYPE_SWITCH,
            sai_serialize_object_id(switch_id), switch_id, 0, nullptr));
    EXPECT_FALSE(global_calls.back().enabled);
    EXPECT_FALSE(state->m_trim.m_trim_reconcile_pending);
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
    EXPECT_FALSE(state->m_trim.m_trim_policy.enabled);
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
        EXPECT_EQ(65535u, state->m_trim.m_trim_policy.trim_size);
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
        EXPECT_FALSE(state->m_trim.m_trim_policy.enabled);
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
            EXPECT_EQ(63u, state->m_trim.m_trim_policy.dscp_value);
            EXPECT_EQ(size != 0, state->m_trim.m_trim_policy.enabled);
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
    EXPECT_TRUE(state->m_trim.m_trim_policy.enabled);
    EXPECT_EQ(256u, state->m_trim.m_trim_policy.trim_size);
    EXPECT_TRUE(global_calls.back().enabled);
    map_results = {-1, 0};
    EXPECT_EQ(SAI_STATUS_FAILURE, setSwitchByte(SAI_SWITCH_ATTR_PACKET_TRIM_DSCP_VALUE, 48));
    EXPECT_EQ(0u, state->m_trim.m_trim_policy.dscp_value);
    EXPECT_EQ(0u, global_calls.back().dscp);
    EXPECT_FALSE(state->m_trim.m_trim_reconcile_pending);
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
    EXPECT_TRUE(state->m_trim.m_trim_reconcile_pending);
    global_results = {-99};
    EXPECT_EQ(SAI_STATUS_FAILURE, setSwitchByte(SAI_SWITCH_ATTR_PACKET_TRIM_DSCP_VALUE, 48));
    EXPECT_EQ(0u, state->m_trim.m_trim_policy.dscp_value);
    EXPECT_TRUE(state->m_trim.m_trim_reconcile_pending);
    EXPECT_EQ(SAI_STATUS_SUCCESS, setSwitchByte(SAI_SWITCH_ATTR_PACKET_TRIM_DSCP_VALUE, 48));
    EXPECT_FALSE(state->m_trim.m_trim_reconcile_pending);
    EXPECT_EQ(48u, state->m_trim.m_trim_policy.dscp_value);
}

TEST_F(SwitchVppTrimTest, pendingInvalidQosCanBeDisabledRepairedAndEnabled)
{
    sai_object_id_t queue_map;
    pendingUnsupportedQosMap(queue_map);
    ASSERT_TRUE(state->m_trim.m_trim_reconcile_pending);
    sai_qos_map_t entry = {};
    entry.key.tc = 2;
    entry.value.queue_index = 5;
    sai_attribute_t attr = {};
    attr.id = SAI_QOS_MAP_ATTR_MAP_TO_VALUE_LIST;
    attr.value.qosmap = {1, &entry};
    EXPECT_EQ(SAI_STATUS_NOT_SUPPORTED, state->set_internal(
            SAI_OBJECT_TYPE_QOS_MAP, sai_serialize_object_id(queue_map), &attr));
    EXPECT_TRUE(state->m_trim.m_trim_reconcile_pending);
    global_calls.clear();
    queue_calls.clear();
    map_calls.clear();

    ASSERT_EQ(SAI_STATUS_SUCCESS, setSize(0));
    ASSERT_EQ(1u, global_calls.size());
    EXPECT_FALSE(global_calls.back().enabled);
    EXPECT_FALSE(state->m_trim.m_trim_policy.enabled);
    EXPECT_FALSE(state->m_trim.m_trim_reconcile_pending);
    EXPECT_FALSE(state->m_trim.m_trim_dataplane_armed);
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
    EXPECT_FALSE(state->m_trim.m_trim_reconcile_pending);
    EXPECT_TRUE(queue_state.at({"pg0", 1}).eligible);
    ASSERT_FALSE(map_calls.empty());
    EXPECT_EQ(5u, map_calls.back()[10]);
}

TEST_F(SwitchVppTrimTest, pendingDisableFailuresRetainCommittedPolicy)
{
    sai_object_id_t queue_map;
    pendingUnsupportedQosMap(queue_map);
    ASSERT_TRUE(state->m_trim.m_trim_reconcile_pending);

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
        EXPECT_TRUE(state->m_trim.m_trim_policy.enabled);
        EXPECT_TRUE(state->m_trim.m_trim_dataplane_armed);
        EXPECT_TRUE(state->m_trim.m_trim_reconcile_pending);
        sai_attribute_t attr = {};
        attr.id = SAI_SWITCH_ATTR_PACKET_TRIM_SIZE;
        ASSERT_EQ(SAI_STATUS_SUCCESS, state->get(SAI_OBJECT_TYPE_SWITCH, switch_id, 1, &attr));
        EXPECT_EQ(256u, attr.value.u32);
    }
    EXPECT_EQ(SAI_STATUS_SUCCESS, setSize(0));
    EXPECT_FALSE(state->m_trim.m_trim_reconcile_pending);
}

TEST_F(SwitchVppTrimTest, pendingDisableCleansEvenWhenNotPreviouslyArmed)
{
    port({trimQueue(1)});
    global_results = {-99, -99};
    ASSERT_EQ(SAI_STATUS_FAILURE, setSize(256));
    ASSERT_TRUE(state->m_trim.m_trim_reconcile_pending);
    ASSERT_FALSE(state->m_trim.m_trim_dataplane_armed);
    ASSERT_TRUE(queue_calls.empty());

    ASSERT_EQ(SAI_STATUS_SUCCESS, setSize(0));
    EXPECT_FALSE(state->m_trim.m_trim_reconcile_pending);
    EXPECT_FALSE(state->m_trim.m_trim_dataplane_armed);
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
    ASSERT_TRUE(state->m_trim.m_trim_reconcile_pending);
    auto unresolved = object(SAI_OBJECT_TYPE_PORT);
    sai_attribute_t attr = {};
    attr.id = SAI_PORT_ATTR_TYPE;
    attr.value.s32 = SAI_PORT_TYPE_LOGICAL;
    put(SAI_OBJECT_TYPE_PORT, unresolved, attr);

    EXPECT_EQ(SAI_STATUS_FAILURE, setSize(0));
    EXPECT_TRUE(state->m_trim.m_trim_reconcile_pending);
    EXPECT_TRUE(state->m_trim.m_trim_dataplane_armed);
    EXPECT_TRUE(state->m_trim.m_trim_policy.enabled);
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
    EXPECT_TRUE(state->m_trim.m_trim_reconcile_pending);
    EXPECT_EQ(0u, state->m_trim.m_trim_policy.dscp_value);

    global_results = {-99};
    EXPECT_EQ(SAI_STATUS_FAILURE, setSwitchByte(SAI_SWITCH_ATTR_PACKET_TRIM_DSCP_VALUE, 49));
    EXPECT_EQ(2u, queue_calls.size());
    EXPECT_EQ(0u, state->m_trim.m_trim_policy.dscp_value);
    EXPECT_TRUE(state->m_trim.m_trim_reconcile_pending);

    queue_calls.clear();
    EXPECT_EQ(SAI_STATUS_SUCCESS, setSwitchByte(SAI_SWITCH_ATTR_PACKET_TRIM_DSCP_VALUE, 49));
    EXPECT_EQ(6u, queue_calls.size());
    EXPECT_EQ(2u, map_calls.size());
    EXPECT_FALSE(state->m_trim.m_trim_reconcile_pending);
    EXPECT_EQ(49u, state->m_trim.m_trim_policy.dscp_value);
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
    EXPECT_FALSE(state->m_trim.m_trim_reconcile_pending);
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
    EXPECT_TRUE(state->m_trim.m_trim_reconcile_pending);
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
    EXPECT_FALSE(state->m_trim.m_trim_reconcile_pending);
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
    EXPECT_FALSE(state->m_trim.m_trim_reconcile_pending);

    sai_attribute_t attr = {};
    attr.id = SAI_PORT_ATTR_QOS_QUEUE_LIST;
    attr.value.objlist = {1, &queue};
    put(SAI_OBJECT_TYPE_PORT, port_id, attr);
    ASSERT_EQ(SAI_STATUS_SUCCESS, setSwitchByte(SAI_SWITCH_ATTR_PACKET_TRIM_DSCP_VALUE, 48));
    ASSERT_EQ(1u, queue_calls.size());
    EXPECT_TRUE(queue_calls.back().eligible);
    EXPECT_FALSE(state->m_trim.m_trim_reconcile_pending);
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
    EXPECT_TRUE(state->m_trim.m_trim_reconcile_pending);
    EXPECT_EQ(1u, queue_calls.size());
    EXPECT_TRUE(map_calls.empty());
    ASSERT_EQ(SAI_STATUS_SUCCESS, state->get(SAI_OBJECT_TYPE_BUFFER_PROFILE, profile, 1, &attr));
    EXPECT_EQ(SAI_BUFFER_PROFILE_PACKET_ADMISSION_FAIL_ACTION_DROP_AND_TRIM, attr.value.s32);
    EXPECT_EQ(SAI_STATUS_SUCCESS, setSwitchByte(SAI_SWITCH_ATTR_PACKET_TRIM_DSCP_VALUE, 48));
    EXPECT_FALSE(state->m_trim.m_trim_reconcile_pending);
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
    EXPECT_TRUE(state->m_trim.m_trim_policy.enabled);
    EXPECT_EQ(256u, state->m_trim.m_trim_policy.trim_size);
    EXPECT_TRUE(global_calls.back().enabled);
    EXPECT_FALSE(state->m_trim.m_trim_reconcile_pending);

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
    EXPECT_FALSE(state->m_trim.m_trim_policy.enabled);
    EXPECT_FALSE(state->m_trim.m_trim_reconcile_pending);
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
    EXPECT_FALSE(state->m_trim.m_trim_policy.enabled);
    EXPECT_TRUE(state->m_trim.m_trim_reconcile_pending);
    EXPECT_EQ(3u, queue_calls.size());

    queue_calls.clear();
    EXPECT_EQ(SAI_STATUS_SUCCESS, setSwitchByte(SAI_SWITCH_ATTR_PACKET_TRIM_DSCP_VALUE, 48));
    EXPECT_EQ(8u, queue_calls.size());
    EXPECT_FALSE(state->m_trim.m_trim_reconcile_pending);
    EXPECT_FALSE(state->m_trim.m_trim_policy.enabled);
    EXPECT_EQ(48u, state->m_trim.m_trim_policy.dscp_value);
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
