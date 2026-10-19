#include "FlexCounter.h"
#include "VendorSaiOptions.h"
#include "sai_serialize.h"
#include "MockableSaiInterface.h"
#include "MockHelper.h"
#include "VirtualObjectIdManager.h"
#include "VidManager.h"
#include "NumberOidIndexGenerator.h"
#include <atomic>
#include <string>
#include <chrono>
#include <condition_variable>
#include <fstream>
#include <future>
#include <map>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <gtest/gtest.h>
#include "swss/dbconnector.h"
#include "swss/redisapi.h"

using namespace saimeta;
using namespace sairedis;
using namespace syncd;
using namespace std;

std::string join(const std::vector<std::string>& input)
{
    SWSS_LOG_ENTER();

    if (input.empty())
    {
        return "";
    }
    std::ostringstream ostream;
    auto iter = input.begin();
    ostream << *iter;
    while (++iter != input.end())
    {
        ostream << "," << *iter;
    }
    return ostream.str();
}

template <typename T>
std::string toOid(T value)
{
    SWSS_LOG_ENTER();

    std::ostringstream ostream;
    ostream << "oid:0x" << std::hex << value;
    return ostream.str();
}

std::shared_ptr<MockableSaiInterface> sai(new MockableSaiInterface());
typedef std::function<void(swss::Table &countersTable, const std::string& key, const std::vector<std::string>& counterIdNames, const std::vector<std::string>& expectedValues)> VerifyStatsFunc;

class ScopedPerPortCounterDiscovery
{
    public:

        explicit ScopedPerPortCounterDiscovery(
                _In_ bool enabled)
        {
            SWSS_LOG_ENTER();
            m_previous = sai->getOptions(VendorSaiOptions::OPTIONS_KEY);

            auto options = std::make_shared<VendorSaiOptions>();

            if (auto previousVendorOptions = std::dynamic_pointer_cast<VendorSaiOptions>(m_previous))
            {
                *options = *previousVendorOptions;
            }

            options->m_enablePerPortCounterDiscovery = enabled;

            sai->setOptions(VendorSaiOptions::OPTIONS_KEY, options);
        }

        ~ScopedPerPortCounterDiscovery()
        {
            SWSS_LOG_ENTER();
            sai->setOptions(VendorSaiOptions::OPTIONS_KEY, m_previous);
        }

        std::shared_ptr<sairedis::SaiOptions> m_previous;
};

std::vector<sai_object_id_t> generateOids(
        unsigned int numOid,
        sai_object_type_t object_type)
{
    SWSS_LOG_ENTER();

    std::vector<sai_object_id_t> object_ids;
    if (!numOid)
        return object_ids;

    auto scc = std::make_shared<SwitchConfigContainer>();
    for (unsigned int i = 0; i < numOid; i++){
        auto hw_info = "asic" + std::to_string(i);
        scc->insert(std::make_shared<SwitchConfig>(i, hw_info));
    }

    auto vidManager = VirtualObjectIdManager(0, scc, std::make_shared<NumberOidIndexGenerator>());
    if (object_type == SAI_OBJECT_TYPE_SWITCH)
    {
        for (unsigned int i = 0; i < numOid; i++){
            auto hw_info = "asic" + std::to_string(i);
            object_ids.push_back(vidManager.allocateNewSwitchObjectId(hw_info));
        }
    }
    else
    {
        auto sid = vidManager.allocateNewSwitchObjectId("asic0");
        for (unsigned int i = 0; i < numOid; i++){
            object_ids.push_back(vidManager.allocateNewObjectId(object_type, sid));
        }
    }
    return object_ids;
}

void removeTimeStamp(std::vector<std::string>& keys, swss::Table& countersTable)
{
    SWSS_LOG_ENTER();

    auto it = std::find(keys.begin(), keys.end(), "TIME_STAMP");
    if (it != keys.end())
    {
        countersTable.del("TIME_STAMP");
        keys.erase(it);
    }
}

/*
 * Count keys in the table, excluding the TIME_STAMP entry without deleting it.
 */
size_t countNonTimestampKeys(swss::Table& countersTable)
{
    SWSS_LOG_ENTER();

    std::vector<std::string> keys;
    countersTable.getKeys(keys);

    auto it = std::find(keys.begin(), keys.end(), "TIME_STAMP");

    return (it != keys.end()) ? keys.size() - 1 : keys.size();
}

/*
 * Poll-wait for at least the expected number of counter keys in COUNTERS_DB.
 * Replaces hardcoded usleep(1000*1050) which is flaky under CI load.
 * Polls every 100ms, asserts on timeout after 5 seconds.
 */
void waitForCounterKeys(
        swss::Table& countersTable,
        size_t expectedKeys,
        int timeoutMs = 5000)
{
    SWSS_LOG_ENTER();

    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    size_t actualKeys = 0;

    while (std::chrono::steady_clock::now() < deadline)
    {
        actualKeys = countNonTimestampKeys(countersTable);

        if (actualKeys >= expectedKeys)
        {
            return;
        }
        usleep(100 * 1000);
    }

    ADD_FAILURE() << "waitForCounterKeys timed out after " << timeoutMs
                  << "ms: expected " << expectedKeys
                  << " keys, got " << actualKeys;
}

/*
 * Poll-wait for a counter field to have a value other than "0" (or empty).
 * Used after counter keys appear to wait for the first real poll cycle.
 * Polls every 100ms, asserts on timeout after 5 seconds.
 */
void waitForNonZeroCounterValue(
        swss::Table& countersTable,
        const std::string& key,
        const std::string& field,
        int timeoutMs = 5000)
{
    SWSS_LOG_ENTER();

    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    while (std::chrono::steady_clock::now() < deadline)
    {
        std::string value;
        if (countersTable.hget(key, field, value) && !value.empty() && value != "0")
        {
            return;
        }
        usleep(100 * 1000);
    }

    std::string value;
    countersTable.hget(key, field, value);
    ADD_FAILURE() << "waitForNonZeroCounterValue timed out after " << timeoutMs
                  << "ms: key='" << key << "' field='" << field
                  << "' actual='" << value << "'";
}

/*
 * Poll-wait for ALL counter fields to reach their expected values in the DB.
 * Combines waiting and verification into a single function to avoid races
 * between separate wait and verify steps. Polls every 100ms, asserts on
 * timeout after 5 seconds.
 */
void waitForCounterValues(
        swss::Table& countersTable,
        const std::string& key,
        const std::vector<std::string>& fields,
        const std::vector<std::string>& expectedValues,
        int timeoutMs = 5000)
{
    SWSS_LOG_ENTER();

    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    while (std::chrono::steady_clock::now() < deadline)
    {
        bool allMatch = true;
        for (size_t i = 0; i < fields.size(); i++)
        {
            std::string value;
            if (!countersTable.hget(key, fields[i], value) || value != expectedValues[i])
            {
                allMatch = false;
                break;
            }
        }
        if (allMatch)
        {
            return;
        }
        usleep(100 * 1000);
    }

    // Timeout — report which fields didn't match
    for (size_t i = 0; i < fields.size(); i++)
    {
        std::string value;
        countersTable.hget(key, fields[i], value);
        if (value != expectedValues[i])
        {
            ADD_FAILURE() << "waitForCounterValues timed out after " << timeoutMs
                          << "ms: key='" << key << "' field='" << fields[i]
                          << "' expected='" << expectedValues[i]
                          << "' actual='" << value << "'";
        }
    }
}

void testAddRemoveCounter(
        unsigned int numOid,
        sai_object_type_t object_type,
        const std::string& counterIdFieldName,
        const std::vector<std::string>& counterIdNames,
        const std::vector<std::string>& expectedValues,
        VerifyStatsFunc verifyFunc,
        bool autoRemoveDbEntry,
        const std::string statsMode = STATS_MODE_READ,
        bool bulkAdd = false,
        const std::string bulkChunkSize = "",
        const std::string bulkChunkSizePerCounter = "",
        bool bulkChunkSizeAfterPort = true,
        const std::string pluginName = "",
        bool immediatelyRemoveBulkChunkSizePerCounter = false,
        bool forceSingleCreate = false,
        const std::string secondaryPollFactor = "")
{
    SWSS_LOG_ENTER();

    FlexCounter fc("test", sai, "COUNTERS_DB");

    test_syncd::mockVidManagerObjectTypeQuery(object_type);

    std::vector<sai_object_id_t> object_ids = generateOids(numOid, object_type);
    EXPECT_EQ(object_ids.size(), numOid);

    std::vector<swss::FieldValueTuple> values;
    values.emplace_back(POLL_INTERVAL_FIELD, "1000");
    if (!secondaryPollFactor.empty())
    {
        values.emplace_back(SECONDARY_POLL_FACTOR_FIELD, secondaryPollFactor);
    }
    // Chunk-size assertions describe the complete configured population, not
    // intermediate enrollment snapshots. Enable polling only after setup.
    values.emplace_back(FLEX_COUNTER_STATUS_FIELD, "disable");
    values.emplace_back(STATS_MODE_FIELD, statsMode);
    std::vector<swss::FieldValueTuple> fcValues = values;
    auto &bulkChunkSizeValues = bulkChunkSizeAfterPort ? fcValues : values;
    if (!bulkChunkSize.empty())
    {
        bulkChunkSizeValues.emplace_back(BULK_CHUNK_SIZE_FIELD, bulkChunkSize);
    }
    if (!bulkChunkSizePerCounter.empty())
    {
        bulkChunkSizeValues.emplace_back(BULK_CHUNK_SIZE_PER_PREFIX_FIELD, bulkChunkSizePerCounter);
    }
    if (!pluginName.empty())
    {
        values.emplace_back(pluginName, "");
    }
    fc.addCounterPlugin(values);

    values.clear();
    values.emplace_back(counterIdFieldName, join(counterIdNames));

    if (bulkAdd)
    {
        fc.bulkAddCounter(object_type, object_ids, object_ids, values);
    }
    else if (forceSingleCreate)
    {
        for (auto object_id : object_ids)
        {
            fc.addCounter(object_id, object_id, values);
        }
    }
    else
    {
        for (auto object_id : object_ids)
        {
            std::vector<sai_object_id_t> tmp_object_ids = {object_id};
            fc.bulkAddCounter(object_type, tmp_object_ids, tmp_object_ids, values);
        }
    }

    if (bulkChunkSizeAfterPort)
    {
        fc.addCounterPlugin(bulkChunkSizeValues);
        if (immediatelyRemoveBulkChunkSizePerCounter)
        {
            bulkChunkSizeValues.clear();
            bulkChunkSizeValues.emplace_back(BULK_CHUNK_SIZE_PER_PREFIX_FIELD, "");
            fc.addCounterPlugin(bulkChunkSizeValues);
        }
    }

    fc.addCounterPlugin({{FLEX_COUNTER_STATUS_FIELD, "enable"}});

    EXPECT_EQ(fc.isEmpty(), false);

    swss::DBConnector db("COUNTERS_DB", 0);
    swss::RedisPipeline pipeline(&db);
    swss::Table countersTable(&pipeline, COUNTERS_TABLE, false);

    waitForCounterKeys(countersTable, object_ids.size());

    // Wait for all fields before joining: stopping at the first published row
    // can cancel the remaining polling plan. Joining also protects mock-owned
    // maps during verification and excludes partial teardown populations from
    // assertions about the steady-state chunk size.
    for (const auto object_id : object_ids)
    {
        const auto key = toOid(object_id);
        if (!expectedValues.empty())
        {
            waitForCounterValues(countersTable, key, counterIdNames, expectedValues);
        }
        else
        {
            for (const auto& counterId : counterIdNames)
            {
                waitForNonZeroCounterValue(countersTable, key, counterId);
            }
        }
    }
    fc.endFlexCounterThread();

    std::vector<std::string> keys;
    countersTable.getKeys(keys);
    // We have a dedicated item for all timestamps for counters using bulk counter polling
    removeTimeStamp(keys, countersTable);
    EXPECT_EQ(keys.size(), object_ids.size());

    for (size_t i = 0; i < object_ids.size(); i++)
    {
        std::string expectedKey = toOid(object_ids[i]);
        verifyFunc(countersTable, expectedKey, counterIdNames, expectedValues);
    }

    for (auto object_id : object_ids)
    {
        fc.removeCounter(object_id);
        if (!autoRemoveDbEntry)
        {
            countersTable.del(toOid(object_id));
        }
    }
    EXPECT_EQ(fc.isEmpty(), true);

    countersTable.getKeys(keys);
    removeTimeStamp(keys, countersTable);
    ASSERT_TRUE(keys.empty());
}

TEST(FlexCounter, addRemoveCounter)
{
    sai->mock_getStatsExt = [](sai_object_type_t, sai_object_id_t, uint32_t number_of_counters, const sai_stat_id_t *, sai_stats_mode_t, uint64_t *counters) {
        for (uint32_t i = 0; i < number_of_counters; i++)
        {
            counters[i] = (i + 1) * 100;
        }
        return SAI_STATUS_SUCCESS;
    };
    sai->mock_getStats = [](sai_object_type_t, sai_object_id_t, uint32_t number_of_counters, const sai_stat_id_t *, uint64_t *counters) {
        for (uint32_t i = 0; i < number_of_counters; i++)
        {
            counters[i] = (i + 1) * 100;
        }
        return SAI_STATUS_SUCCESS;
    };
    sai->mock_queryStatsCapability = [](sai_object_id_t switch_id, sai_object_type_t object_type, sai_stat_capability_list_t *stats_capability) {
        // For now, just return failure to make test simple, will write a singe test to cover querySupportedCounters
        return SAI_STATUS_FAILURE;
    };

    sai->mock_bulkGetStats = [](sai_object_id_t, sai_object_type_t, uint32_t, const sai_object_key_t *, uint32_t, const sai_stat_id_t *, sai_stats_mode_t, sai_status_t *, uint64_t *)
    {
        return SAI_STATUS_FAILURE;
    };

    auto counterVerifyFunc = [] (swss::Table &countersTable, const std::string& key, const std::vector<std::string>& counterIdNames, const std::vector<std::string>& expectedValues)
    {
        std::string value;
        for (size_t i = 0; i < counterIdNames.size(); i++)
        {
            countersTable.hget(key, counterIdNames[i], value);
            EXPECT_EQ(value, expectedValues[i]);
        }
    };

    testAddRemoveCounter(
        1,
        SAI_OBJECT_TYPE_COUNTER,
        FLOW_COUNTER_ID_LIST,
        {"SAI_COUNTER_STAT_PACKETS", "SAI_COUNTER_STAT_BYTES"},
        {"100", "200"},
        counterVerifyFunc,
        true);

    // Bulk create mode to satisfy the coverage requirement
    testAddRemoveCounter(
        1,
        SAI_OBJECT_TYPE_COUNTER,
        FLOW_COUNTER_ID_LIST,
        {"SAI_COUNTER_STAT_PACKETS", "SAI_COUNTER_STAT_BYTES"},
        {"100", "200"},
        counterVerifyFunc,
        true,
        STATS_MODE_READ,
        true);

    testAddRemoveCounter(
        1,
        SAI_OBJECT_TYPE_MACSEC_FLOW,
        MACSEC_FLOW_COUNTER_ID_LIST,
        {"SAI_MACSEC_FLOW_STAT_CONTROL_PKTS", "SAI_MACSEC_FLOW_STAT_PKTS_UNTAGGED"},
        {"100", "200"},
        counterVerifyFunc,
        false);

    // Bulk create mode to satisfy the coverage requirement
    testAddRemoveCounter(
        1,
        SAI_OBJECT_TYPE_MACSEC_FLOW,
        MACSEC_FLOW_COUNTER_ID_LIST,
        {"SAI_MACSEC_FLOW_STAT_CONTROL_PKTS", "SAI_MACSEC_FLOW_STAT_PKTS_UNTAGGED"},
        {"100", "200"},
        counterVerifyFunc,
        false,
        STATS_MODE_READ,
        true);

    testAddRemoveCounter(
        1,
        SAI_OBJECT_TYPE_MACSEC_SA,
        MACSEC_SA_COUNTER_ID_LIST,
        {"SAI_MACSEC_SA_STAT_OCTETS_ENCRYPTED", "SAI_MACSEC_SA_STAT_OCTETS_PROTECTED"},
        {"100", "200"},
        counterVerifyFunc,
        false);

    // Bulk create mode to satisfy the coverage requirement
    testAddRemoveCounter(
        1,
        SAI_OBJECT_TYPE_MACSEC_SA,
        MACSEC_SA_COUNTER_ID_LIST,
        {"SAI_MACSEC_SA_STAT_OCTETS_ENCRYPTED", "SAI_MACSEC_SA_STAT_OCTETS_PROTECTED"},
        {"100", "200"},
        counterVerifyFunc,
        false,
        STATS_MODE_READ,
        true);

    testAddRemoveCounter(
        1,
        SAI_OBJECT_TYPE_PORT,
        PORT_COUNTER_ID_LIST,
        {"SAI_PORT_STAT_IF_IN_OCTETS", "SAI_PORT_STAT_IF_IN_UCAST_PKTS"},
        {"100", "200"},
        counterVerifyFunc,
        false);

    testAddRemoveCounter(
        1,
        SAI_OBJECT_TYPE_PORT,
        PORT_DEBUG_COUNTER_ID_LIST,
        {"SAI_PORT_STAT_IN_CONFIGURED_DROP_REASONS_0_DROPPED_PKTS", "SAI_PORT_STAT_IN_CONFIGURED_DROP_REASONS_1_DROPPED_PKTS"},
        {"100", "200"},
        counterVerifyFunc,
        false);

    // Bulk create mode to satisfy the coverage requirement
    testAddRemoveCounter(
        1,
        SAI_OBJECT_TYPE_PORT,
        PORT_DEBUG_COUNTER_ID_LIST,
        {"SAI_PORT_STAT_IN_CONFIGURED_DROP_REASONS_0_DROPPED_PKTS", "SAI_PORT_STAT_IN_CONFIGURED_DROP_REASONS_1_DROPPED_PKTS"},
        {"100", "200"},
        counterVerifyFunc,
        false,
        STATS_MODE_READ,
        true);

    bool clearCalled = false;
    sai->mock_clearStats = [&] (sai_object_type_t object_type, sai_object_id_t object_id, uint32_t number_of_counters, const sai_stat_id_t *counter_ids) {
        clearCalled = true;
        return SAI_STATUS_SUCCESS;
    };

    testAddRemoveCounter(
        1,
        SAI_OBJECT_TYPE_QUEUE,
        QUEUE_COUNTER_ID_LIST,
        {"SAI_QUEUE_STAT_PACKETS", "SAI_QUEUE_STAT_BYTES"},
        {"100", "200"},
        counterVerifyFunc,
        false,
        STATS_MODE_READ_AND_CLEAR);
    EXPECT_EQ(true, clearCalled);

    // Bulk create mode to satisfy the coverage requirement
    testAddRemoveCounter(
        1,
        SAI_OBJECT_TYPE_QUEUE,
        QUEUE_COUNTER_ID_LIST,
        {"SAI_QUEUE_STAT_PACKETS", "SAI_QUEUE_STAT_BYTES"},
        {"100", "200"},
        counterVerifyFunc,
        false,
        STATS_MODE_READ_AND_CLEAR,
        true);
    EXPECT_EQ(true, clearCalled);

    testAddRemoveCounter(
        1,
        SAI_OBJECT_TYPE_INGRESS_PRIORITY_GROUP,
        PG_COUNTER_ID_LIST,
        {"SAI_INGRESS_PRIORITY_GROUP_STAT_PACKETS", "SAI_INGRESS_PRIORITY_GROUP_STAT_BYTES"},
        {"100", "200"},
        counterVerifyFunc,
        false);

    // Bulk create mode to satisfy the coverage requirement
    testAddRemoveCounter(
        1,
        SAI_OBJECT_TYPE_INGRESS_PRIORITY_GROUP,
        PG_COUNTER_ID_LIST,
        {"SAI_INGRESS_PRIORITY_GROUP_STAT_PACKETS", "SAI_INGRESS_PRIORITY_GROUP_STAT_BYTES"},
        {"100", "200"},
        counterVerifyFunc,
        false,
        STATS_MODE_READ,
        true);

    testAddRemoveCounter(
        1,
        SAI_OBJECT_TYPE_ROUTER_INTERFACE,
        RIF_COUNTER_ID_LIST,
        {"SAI_ROUTER_INTERFACE_STAT_IN_OCTETS", "SAI_ROUTER_INTERFACE_STAT_IN_PACKETS"},
        {"100", "200"},
        counterVerifyFunc,
        false);

    // Bulk create mode to satisfy the coverage requirement
    testAddRemoveCounter(
        1,
        SAI_OBJECT_TYPE_ROUTER_INTERFACE,
        RIF_COUNTER_ID_LIST,
        {"SAI_ROUTER_INTERFACE_STAT_IN_OCTETS", "SAI_ROUTER_INTERFACE_STAT_IN_PACKETS"},
        {"100", "200"},
        counterVerifyFunc,
        false,
        STATS_MODE_READ,
        true);

    testAddRemoveCounter(
        1,
        SAI_OBJECT_TYPE_SWITCH,
        SWITCH_DEBUG_COUNTER_ID_LIST,
        {"SAI_SWITCH_STAT_IN_CONFIGURED_DROP_REASONS_0_DROPPED_PKTS", "SAI_SWITCH_STAT_IN_CONFIGURED_DROP_REASONS_1_DROPPED_PKTS"},
        {"100", "200"},
        counterVerifyFunc,
        false);

    // Bulk create mode to satisfy the coverage requirement
    testAddRemoveCounter(
        1,
        SAI_OBJECT_TYPE_SWITCH,
        SWITCH_DEBUG_COUNTER_ID_LIST,
        {"SAI_SWITCH_STAT_IN_CONFIGURED_DROP_REASONS_0_DROPPED_PKTS", "SAI_SWITCH_STAT_IN_CONFIGURED_DROP_REASONS_1_DROPPED_PKTS"},
        {"100", "200"},
        counterVerifyFunc,
        false,
        STATS_MODE_READ,
        true);

    testAddRemoveCounter(
        1,
        SAI_OBJECT_TYPE_TUNNEL,
        TUNNEL_COUNTER_ID_LIST,
        {"SAI_TUNNEL_STAT_IN_OCTETS", "SAI_TUNNEL_STAT_IN_PACKETS"},
        {"100", "200"},
        counterVerifyFunc,
        false);

    // Bulk create mode to satisfy the coverage requirement
    testAddRemoveCounter(
        1,
        SAI_OBJECT_TYPE_TUNNEL,
        TUNNEL_COUNTER_ID_LIST,
        {"SAI_TUNNEL_STAT_IN_OCTETS", "SAI_TUNNEL_STAT_IN_PACKETS"},
        {"100", "200"},
        counterVerifyFunc,
        false,
        STATS_MODE_READ,
        true);

    testAddRemoveCounter(
        1,
        (sai_object_type_t)SAI_OBJECT_TYPE_ENI,
        ENI_COUNTER_ID_LIST,
        {"SAI_ENI_STAT_FLOW_CREATED", "SAI_ENI_STAT_FLOW_CREATE_FAILED", "SAI_ENI_STAT_FLOW_DELETED", "SAI_ENI_STAT_FLOW_DELETE_FAILED"},
        {"100", "200", "300", "400"},
        counterVerifyFunc,
        true);

    testAddRemoveCounter(
        1,
        (sai_object_type_t)SAI_OBJECT_TYPE_HA_SET,
        HA_SET_COUNTER_ID_LIST,
        {"SAI_HA_SET_STAT_DP_PROBE_REQ_RX_BYTES", "SAI_HA_SET_STAT_DP_PROBE_REQ_RX_PACKETS",
         "SAI_HA_SET_STAT_DP_PROBE_REQ_TX_BYTES", "SAI_HA_SET_STAT_DP_PROBE_REQ_TX_PACKETS",
         "SAI_HA_SET_STAT_DP_PROBE_ACK_RX_BYTES", "SAI_HA_SET_STAT_DP_PROBE_ACK_RX_PACKETS",
         "SAI_HA_SET_STAT_DP_PROBE_ACK_TX_BYTES", "SAI_HA_SET_STAT_DP_PROBE_ACK_TX_PACKETS",
         "SAI_HA_SET_STAT_DP_PROBE_FAILED",
         "SAI_HA_SET_STAT_CP_DATA_CHANNEL_CONNECT_ATTEMPTED", "SAI_HA_SET_STAT_CP_DATA_CHANNEL_CONNECT_RECEIVED",
         "SAI_HA_SET_STAT_CP_DATA_CHANNEL_CONNECT_SUCCEEDED", "SAI_HA_SET_STAT_CP_DATA_CHANNEL_CONNECT_FAILED",
         "SAI_HA_SET_STAT_CP_DATA_CHANNEL_CONNECT_REJECTED", "SAI_HA_SET_STAT_CP_DATA_CHANNEL_TIMEOUT_COUNT",
         "SAI_HA_SET_STAT_BULK_SYNC_MESSAGE_RECEIVED", "SAI_HA_SET_STAT_BULK_SYNC_MESSAGE_SENT",
         "SAI_HA_SET_STAT_BULK_SYNC_MESSAGE_SEND_FAILED",
         "SAI_HA_SET_STAT_BULK_SYNC_FLOW_RECEIVED", "SAI_HA_SET_STAT_BULK_SYNC_FLOW_SENT"},
        {"100", "200", "300", "400", "500", "600", "700", "800", "900", "1000",
         "1100", "1200", "1300", "1400", "1500", "1600", "1700", "1800", "1900", "2000"},
        counterVerifyFunc,
        false);

    clearCalled = false;
    testAddRemoveCounter(
        1,
        SAI_OBJECT_TYPE_BUFFER_POOL,
        BUFFER_POOL_COUNTER_ID_LIST,
        {"SAI_BUFFER_POOL_STAT_CURR_OCCUPANCY_BYTES", "SAI_BUFFER_POOL_STAT_WATERMARK_BYTES"},
        {"100", "200"},
        counterVerifyFunc,
        false);
    EXPECT_EQ(true, clearCalled);

    sai->mock_get = [] (sai_object_type_t objectType, sai_object_id_t objectId, uint32_t attr_count, sai_attribute_t *attr_list) {
        for (uint32_t i = 0; i < attr_count; i++)
        {
            if (attr_list[i].id == SAI_QUEUE_ATTR_PAUSE_STATUS)
            {
                attr_list[i].value.booldata = false;
            }
        }
        return SAI_STATUS_SUCCESS;
    };

    testAddRemoveCounter(
        1,
        SAI_OBJECT_TYPE_QUEUE,
        QUEUE_ATTR_ID_LIST,
        {"SAI_QUEUE_ATTR_PAUSE_STATUS"},
        {"false"},
        counterVerifyFunc,
        false);

    // Bulk create mode to satisfy the coverage requirement
    testAddRemoveCounter(
        1,
        SAI_OBJECT_TYPE_QUEUE,
        QUEUE_ATTR_ID_LIST,
        {"SAI_QUEUE_ATTR_PAUSE_STATUS"},
        {"false"},
        counterVerifyFunc,
        false,
        STATS_MODE_READ,
        true);

    sai->mock_get = [] (sai_object_type_t objectType, sai_object_id_t objectId, uint32_t attr_count, sai_attribute_t *attr_list) {
        for (uint32_t i = 0; i < attr_count; i++)
        {
            if (attr_list[i].id == SAI_INGRESS_PRIORITY_GROUP_ATTR_PORT)
            {
                attr_list[i].value.oid = 1;
            }
        }
        return SAI_STATUS_SUCCESS;
    };

    testAddRemoveCounter(
        1,
        SAI_OBJECT_TYPE_INGRESS_PRIORITY_GROUP,
        PG_ATTR_ID_LIST,
        {"SAI_INGRESS_PRIORITY_GROUP_ATTR_PORT"},
        {"oid:0x1"},
        counterVerifyFunc,
        false);

    // Bulk create mode to satisfy the coverage requirement
    testAddRemoveCounter(
        1,
        SAI_OBJECT_TYPE_INGRESS_PRIORITY_GROUP,
        PG_ATTR_ID_LIST,
        {"SAI_INGRESS_PRIORITY_GROUP_ATTR_PORT"},
        {"oid:0x1"},
        counterVerifyFunc,
        false,
        STATS_MODE_READ,
        true);

    sai->mock_get = [] (sai_object_type_t objectType, sai_object_id_t objectId, uint32_t attr_count, sai_attribute_t *attr_list) {
        for (uint32_t i = 0; i < attr_count; i++)
        {
            if (attr_list[i].id == SAI_MACSEC_SA_ATTR_CONFIGURED_EGRESS_XPN)
            {
                attr_list[i].value.u64 = 0;
            }
            else if (attr_list[i].id == SAI_MACSEC_SA_ATTR_AN)
            {
                attr_list[i].value.u8 = 1;
            }
        }
        return SAI_STATUS_SUCCESS;
    };

    testAddRemoveCounter(
        1,
        SAI_OBJECT_TYPE_MACSEC_SA,
        MACSEC_SA_ATTR_ID_LIST,
        {"SAI_MACSEC_SA_ATTR_CONFIGURED_EGRESS_XPN", "SAI_MACSEC_SA_ATTR_AN"},
        {"0", "1"},
        counterVerifyFunc,
        false);

    // Bulk create mode to satisfy the coverage requirement
    testAddRemoveCounter(
        1,
        SAI_OBJECT_TYPE_MACSEC_SA,
        MACSEC_SA_ATTR_ID_LIST,
        {"SAI_MACSEC_SA_ATTR_CONFIGURED_EGRESS_XPN", "SAI_MACSEC_SA_ATTR_AN"},
        {"0", "1"},
        counterVerifyFunc,
        false,
        STATS_MODE_READ,
        true);

    sai->mock_get = [] (sai_object_type_t objectType, sai_object_id_t objectId, uint32_t attr_count, sai_attribute_t *attr_list) {
        for (uint32_t i = 0; i < attr_count; i++)
        {
            if (attr_list[i].id == SAI_ACL_COUNTER_ATTR_PACKETS)
            {
                attr_list[i].value.u64 = 1000;
            }
        }
        return SAI_STATUS_SUCCESS;
    };

    testAddRemoveCounter(
        1,
        SAI_OBJECT_TYPE_ACL_COUNTER,
        ACL_COUNTER_ATTR_ID_LIST,
        {"SAI_ACL_COUNTER_ATTR_PACKETS"},
        {"1000"},
        counterVerifyFunc,
        true);

    // Bulk create mode to satisfy the coverage requirement
    testAddRemoveCounter(
        1,
        SAI_OBJECT_TYPE_ACL_COUNTER,
        ACL_COUNTER_ATTR_ID_LIST,
        {"SAI_ACL_COUNTER_ATTR_PACKETS"},
        {"1000"},
        counterVerifyFunc,
        true,
        STATS_MODE_READ,
        true);

    sai->mock_get = [] (sai_object_type_t objectType, sai_object_id_t objectId, uint32_t attr_count, sai_attribute_t *attr_list) {
        for (uint32_t i = 0; i < attr_count; i++)
        {
            if (attr_list[i].id == SAI_OTN_ATTENUATOR_ATTR_ATTENUATION)
            {
                attr_list[i].value.u32 = 500;
            }
            else if (attr_list[i].id == SAI_OTN_ATTENUATOR_ATTR_ENABLED)
            {
                attr_list[i].value.booldata = true;
            }
        }
        return SAI_STATUS_SUCCESS;
    };

    testAddRemoveCounter(
        1,
        (sai_object_type_t)SAI_OBJECT_TYPE_OTN_ATTENUATOR,
        OTN_ATTENUATOR_ATTR_ID_LIST,
        {"SAI_OTN_ATTENUATOR_ATTR_ATTENUATION", "SAI_OTN_ATTENUATOR_ATTR_ENABLED"},
        {"500", "true"},
        counterVerifyFunc,
        false);

    // Bulk create mode to satisfy the coverage requirement
    testAddRemoveCounter(
        1,
        (sai_object_type_t)SAI_OBJECT_TYPE_OTN_ATTENUATOR,
        OTN_ATTENUATOR_ATTR_ID_LIST,
        {"SAI_OTN_ATTENUATOR_ATTR_ATTENUATION", "SAI_OTN_ATTENUATOR_ATTR_ENABLED"},
        {"500", "true"},
        counterVerifyFunc,
        false,
        STATS_MODE_READ,
        true);

    sai->mock_get = [] (sai_object_type_t objectType, sai_object_id_t objectId, uint32_t attr_count, sai_attribute_t *attr_list) {
        for (uint32_t i = 0; i < attr_count; i++)
        {
            if (attr_list[i].id == SAI_OTN_OA_ATTR_TARGET_GAIN)
            {
                attr_list[i].value.u32 = 2000;
            }
            else if (attr_list[i].id == SAI_OTN_OA_ATTR_ENABLED)
            {
                attr_list[i].value.booldata = true;
            }
        }
        return SAI_STATUS_SUCCESS;
    };

    testAddRemoveCounter(
        1,
        (sai_object_type_t)SAI_OBJECT_TYPE_OTN_OA,
        OTN_OA_ATTR_ID_LIST,
        {"SAI_OTN_OA_ATTR_TARGET_GAIN", "SAI_OTN_OA_ATTR_ENABLED"},
        {"2000", "true"},
        counterVerifyFunc,
        false);

    // Bulk create mode to satisfy the coverage requirement
    testAddRemoveCounter(
        1,
        (sai_object_type_t)SAI_OBJECT_TYPE_OTN_OA,
        OTN_OA_ATTR_ID_LIST,
        {"SAI_OTN_OA_ATTR_TARGET_GAIN", "SAI_OTN_OA_ATTR_ENABLED"},
        {"2000", "true"},
        counterVerifyFunc,
        false,
        STATS_MODE_READ,
        true);

    testAddRemoveCounter(
        1,
        SAI_OBJECT_TYPE_COUNTER,
        SRV6_COUNTER_ID_LIST,
        {"SAI_COUNTER_STAT_PACKETS", "SAI_COUNTER_STAT_BYTES"},
        {"100", "200"},
        counterVerifyFunc,
        false);

    testAddRemoveCounter(
        1,
        SAI_OBJECT_TYPE_COUNTER,
        SRV6_COUNTER_ID_LIST,
        {"SAI_COUNTER_STAT_PACKETS", "SAI_COUNTER_STAT_BYTES"},
        {"100", "200"},
        counterVerifyFunc,
        false,
        STATS_MODE_READ,
        true);

    // Packet Trimming

    testAddRemoveCounter(
        1,
        SAI_OBJECT_TYPE_PORT,
        PORT_COUNTER_ID_LIST,
        {"SAI_PORT_STAT_TRIM_PACKETS", "SAI_PORT_STAT_DROPPED_TRIM_PACKETS", "SAI_PORT_STAT_TX_TRIM_PACKETS"},
        {"100", "200", "300"},
        counterVerifyFunc,
        false);

    testAddRemoveCounter(
        1,
        SAI_OBJECT_TYPE_PORT,
        PORT_COUNTER_ID_LIST,
        {"SAI_PORT_STAT_TRIM_PACKETS", "SAI_PORT_STAT_DROPPED_TRIM_PACKETS", "SAI_PORT_STAT_TX_TRIM_PACKETS"},
        {"100", "200", "300"},
        counterVerifyFunc,
        false,
        STATS_MODE_READ,
        true);

    testAddRemoveCounter(
        1,
        SAI_OBJECT_TYPE_QUEUE,
        QUEUE_COUNTER_ID_LIST,
        {"SAI_QUEUE_STAT_TRIM_PACKETS", "SAI_QUEUE_STAT_DROPPED_TRIM_PACKETS", "SAI_QUEUE_STAT_TX_TRIM_PACKETS"},
        {"100", "200", "300"},
        counterVerifyFunc,
        false);

    testAddRemoveCounter(
        1,
        SAI_OBJECT_TYPE_QUEUE,
        QUEUE_COUNTER_ID_LIST,
        {"SAI_QUEUE_STAT_TRIM_PACKETS", "SAI_QUEUE_STAT_DROPPED_TRIM_PACKETS", "SAI_QUEUE_STAT_TX_TRIM_PACKETS"},
        {"100", "200", "300"},
        counterVerifyFunc,
        false,
        STATS_MODE_READ,
        true);

    testAddRemoveCounter(
        1,
        SAI_OBJECT_TYPE_SWITCH,
        SWITCH_COUNTER_ID_LIST,
        {"SAI_SWITCH_STAT_DROPPED_TRIM_PACKETS", "SAI_SWITCH_STAT_TX_TRIM_PACKETS"},
        {"100", "200"},
        counterVerifyFunc,
        false);

    testAddRemoveCounter(
        1,
        SAI_OBJECT_TYPE_SWITCH,
        SWITCH_COUNTER_ID_LIST,
        {"SAI_SWITCH_STAT_DROPPED_TRIM_PACKETS", "SAI_SWITCH_STAT_TX_TRIM_PACKETS"},
        {"100", "200"},
        counterVerifyFunc,
        false,
        STATS_MODE_READ,
        true);
}

TEST(FlexCounter, UpdateExistingCounterAddBulk)
{
    sai->mock_bulkGetStats = [](sai_object_id_t, sai_object_type_t, uint32_t, const sai_object_key_t *, uint32_t, const sai_stat_id_t *, sai_stats_mode_t, sai_status_t *, uint64_t *)
    {
        return SAI_STATUS_NOT_SUPPORTED;
    };

    std::string value;
    // Create test data for multiple objects
    std::vector<sai_object_id_t> vids = generateOids(1, SAI_OBJECT_TYPE_SWITCH);  // Virtual IDs
    std::vector<sai_object_id_t> rids = generateOids(1, SAI_OBJECT_TYPE_SWITCH);  // Real IDs
    std::vector<swss::FieldValueTuple> values;

    auto fc = std::make_shared<FlexCounter>("TEST_FLEX_COUNTER", sai, "COUNTERS_DB", false);
    swss::DBConnector db("COUNTERS_DB", 0);
    swss::Table countersTable(&db, COUNTERS_TABLE);
    std::vector<std::string> keys;

    values.clear();
    values.emplace_back(POLL_INTERVAL_FIELD, "50");
    values.emplace_back(FLEX_COUNTER_STATUS_FIELD, "enable");
    fc->addCounterPlugin(values);

    // Add some counter IDs
    std::vector<std::string> counterIds = {"SAI_SWITCH_STAT_IN_CONFIGURED_DROP_REASONS_2_DROPPED_PKTS"};
    values.clear();
    values.emplace_back("SWITCH_DEBUG_COUNTER_ID_LIST", join(counterIds));
    fc->bulkAddCounter(SAI_OBJECT_TYPE_SWITCH, vids, rids, values);

    usleep(60*1000);
    countersTable.getKeys(keys);
    EXPECT_EQ(keys.size(), size_t(1));
    ASSERT_TRUE(countersTable.hget(keys[0], "SAI_SWITCH_STAT_IN_CONFIGURED_DROP_REASONS_2_DROPPED_PKTS", value));

    // Update the counter IDs
    values.clear();
    counterIds.push_back("SAI_SWITCH_STAT_IN_CONFIGURED_DROP_REASONS_3_DROPPED_PKTS");
    values.emplace_back("SWITCH_DEBUG_COUNTER_ID_LIST", join(counterIds));
    fc->bulkAddCounter(SAI_OBJECT_TYPE_SWITCH, vids, rids, values);

    usleep(60*1000);
    keys.clear();
    countersTable.getKeys(keys);
    EXPECT_EQ(keys.size(), size_t(1));
    ASSERT_TRUE(countersTable.hget(keys[0], "SAI_SWITCH_STAT_IN_CONFIGURED_DROP_REASONS_2_DROPPED_PKTS", value));
    ASSERT_TRUE(countersTable.hget(keys[0], "SAI_SWITCH_STAT_IN_CONFIGURED_DROP_REASONS_3_DROPPED_PKTS", value));

    /* Cleanup */
    fc->removeCounterPlugins();
    countersTable.del(toOid(vids[0]));
    fc->removeCounter(vids[0]);
}

TEST(FlexCounter, queryCounterCapability)
{
    sai->mock_queryStatsCapability = [](sai_object_id_t switch_id, sai_object_type_t object_type, sai_stat_capability_list_t *stats_capability) {
        if (stats_capability->count == 0)
        {
            stats_capability->count = 1;
            return SAI_STATUS_BUFFER_OVERFLOW;
        }
        else
        {
            stats_capability->list[0].stat_enum = SAI_PORT_STAT_IF_IN_OCTETS;
            stats_capability->list[0].stat_modes = SAI_STATS_MODE_READ | SAI_STATS_MODE_READ_AND_CLEAR;
            return SAI_STATUS_SUCCESS;
        }
    };

    sai->mock_getStats = [](sai_object_type_t, sai_object_id_t, uint32_t number_of_counters, const sai_stat_id_t *counter_ids, uint64_t *counters) {
        for (uint32_t i = 0; i < number_of_counters; i++)
        {
            counters[i] = 1000;
        }
        return SAI_STATUS_SUCCESS;
    };

    sai->mock_clearStats = [&] (sai_object_type_t object_type, sai_object_id_t object_id, uint32_t number_of_counters, const sai_stat_id_t *counter_ids) {
        return SAI_STATUS_SUCCESS;
    };

    sai->mock_bulkGetStats = [](sai_object_id_t, sai_object_type_t, uint32_t, const sai_object_key_t *, uint32_t, const sai_stat_id_t *, sai_stats_mode_t, sai_status_t *, uint64_t *)
    {
        return SAI_STATUS_FAILURE;
    };

    test_syncd::mockVidManagerObjectTypeQuery(SAI_OBJECT_TYPE_PORT);
    const auto oid = generateOids(1, SAI_OBJECT_TYPE_PORT).front();
    FlexCounter fc("test", sai, "COUNTERS_DB");
    fc.addCounterPlugin({{POLL_INTERVAL_FIELD, "1000"}, {STATS_MODE_FIELD, STATS_MODE_READ}});
    fc.bulkAddCounter(SAI_OBJECT_TYPE_PORT, {oid}, {oid}, {{PORT_COUNTER_ID_LIST,
            "SAI_PORT_STAT_IF_IN_OCTETS,SAI_PORT_STAT_IF_IN_UCAST_PKTS"}});
    fc.addCounterPlugin({{FLEX_COUNTER_STATUS_FIELD, "enable"}});
    EXPECT_FALSE(fc.isEmpty());

    swss::DBConnector db("COUNTERS_DB", 0);
    swss::Table countersTable(&db, COUNTERS_TABLE);
    const auto key = toOid(oid);
    // Only the supported field may appear; waiting for every requested field
    // would incorrectly require the deliberately unsupported counter.
    waitForCounterValues(countersTable, key, {"SAI_PORT_STAT_IF_IN_OCTETS"}, {"1000"});
    fc.endFlexCounterThread();
    EXPECT_EQ(countNonTimestampKeys(countersTable), 1u);
    std::string value;
    EXPECT_TRUE(countersTable.hget(key, "SAI_PORT_STAT_IF_IN_OCTETS", value));
    EXPECT_EQ(value, "1000");
    EXPECT_FALSE(countersTable.hget(key, "SAI_PORT_STAT_IF_IN_UCAST_PKTS", value));

    fc.removeCounter(oid);
    countersTable.del(key);
    countersTable.del("TIME_STAMP");
    EXPECT_TRUE(fc.isEmpty());
    EXPECT_EQ(countNonTimestampKeys(countersTable), 0u);
}

TEST(FlexCounter, noSupportedCounters)
{
    sai->mock_queryStatsCapability = [](sai_object_id_t switch_id, sai_object_type_t object_type, sai_stat_capability_list_t *stats_capability) {
        return SAI_STATUS_FAILURE;
    };

    sai->mock_getStats = [](sai_object_type_t, sai_object_id_t, uint32_t number_of_counters, const sai_stat_id_t *, uint64_t *counters) {
        return SAI_STATUS_FAILURE;
    };

    FlexCounter fc("test", sai, "COUNTERS_DB");
    std::vector<swss::FieldValueTuple> values;
    values.emplace_back(PORT_COUNTER_ID_LIST, "SAI_PORT_STAT_IF_IN_OCTETS,SAI_PORT_STAT_IF_IN_UCAST_PKTS");

    test_syncd::mockVidManagerObjectTypeQuery(SAI_OBJECT_TYPE_PORT);

    fc.addCounter(sai_object_id_t(0x1000000000000), sai_object_id_t(0x1000000000000), values);
    // No supported counter, this object shall not be queried
    EXPECT_EQ(fc.isEmpty(), true);

    std::vector<sai_object_id_t> oids = {0x1000000000000};
    fc.bulkAddCounter(SAI_OBJECT_TYPE_PORT, oids, oids, values);
    EXPECT_EQ(fc.isEmpty(), true);
}

void testAddRemovePlugin(const std::string& pluginFieldName)
{
    SWSS_LOG_ENTER();

    FlexCounter fc("test", sai, "COUNTERS_DB", true);

    std::vector<swss::FieldValueTuple> values;
    values.emplace_back(pluginFieldName, "dummy_sha_strings");
    fc.addCounterPlugin(values);
    EXPECT_EQ(fc.isEmpty(), false);

    fc.removeCounterPlugins();
    EXPECT_EQ(fc.isEmpty(), true);
}

TEST(FlexCounter, addRemoveCounterPlugin)
{
    std::string fields[] = {QUEUE_PLUGIN_FIELD,
                            PG_PLUGIN_FIELD,
                            PORT_PLUGIN_FIELD,
                            RIF_PLUGIN_FIELD,
                            BUFFER_POOL_PLUGIN_FIELD,
                            TUNNEL_PLUGIN_FIELD,
                            FLOW_COUNTER_PLUGIN_FIELD,
                            WRED_QUEUE_PLUGIN_FIELD,
                            WRED_PORT_PLUGIN_FIELD,
                            OTN_ATTENUATOR_PLUGIN_FIELD,
                            OTN_OA_PLUGIN_FIELD};
    for (auto &field : fields)
    {
        testAddRemovePlugin(field);
    }
}

TEST(FlexCounter, addDuplicateCounterPlugin)
{
    SWSS_LOG_ENTER();

    FlexCounter fc("test", sai, "COUNTERS_DB", true);

    std::vector<swss::FieldValueTuple> values;
    values.emplace_back(PORT_PLUGIN_FIELD, "dummy_sha_string");
    fc.addCounterPlugin(values);
    EXPECT_EQ(fc.isEmpty(), false);

    // Adding the same plugin again should be a no-op (duplicate ignored)
    fc.addCounterPlugin(values);
    EXPECT_EQ(fc.isEmpty(), false);

    fc.removeCounterPlugins();
    EXPECT_EQ(fc.isEmpty(), true);
}

TEST(FlexCounter, addRemoveCounterForPort)
{
    FlexCounter fc("test", sai, "COUNTERS_DB");

    sai_object_id_t counterVid{0x1000000000000};
    sai_object_id_t counterRid{0x1000000000000};
    std::vector<swss::FieldValueTuple> values;
    values.emplace_back(PORT_COUNTER_ID_LIST, "SAI_PORT_STAT_IF_IN_OCTETS,SAI_PORT_STAT_IF_IN_ERRORS");

    test_syncd::mockVidManagerObjectTypeQuery(SAI_OBJECT_TYPE_PORT);
    sai->mock_getStats = [](sai_object_type_t, sai_object_id_t, uint32_t number_of_counters, const sai_stat_id_t *ids, uint64_t *counters) {
        for (uint32_t i = 0; i < number_of_counters; i++)
        {
            if (ids[i] == SAI_PORT_STAT_IF_IN_OCTETS)
            {
                counters[i] = 100;
            }
            else if (ids[i] == SAI_PORT_STAT_IF_IN_ERRORS)
            {
                counters[i] = 200;
            }
            else
            {
                return SAI_STATUS_FAILURE;
            }
        }
        return SAI_STATUS_SUCCESS;
    };

    fc.addCounter(counterVid, counterRid, values);
    EXPECT_EQ(fc.isEmpty(), false);

    values.clear();
    values.emplace_back(POLL_INTERVAL_FIELD, "1000");
    values.emplace_back(FLEX_COUNTER_STATUS_FIELD, "enable");
    values.emplace_back(STATS_MODE_FIELD, STATS_MODE_READ);
    fc.addCounterPlugin(values);

    swss::DBConnector db("COUNTERS_DB", 0);
    swss::RedisPipeline pipeline(&db);
    swss::Table countersTable(&pipeline, COUNTERS_TABLE, false);

    waitForCounterKeys(countersTable, 1);

    std::vector<std::string> keys;
    countersTable.getKeys(keys);
    EXPECT_EQ(keys.size(), size_t(1));
    std::string expectedKey = toOid(counterVid);
    EXPECT_EQ(keys[0], expectedKey);

    std::string value;
    countersTable.hget(expectedKey, "SAI_PORT_STAT_IF_IN_OCTETS", value);
    EXPECT_EQ(value, "100");
    countersTable.hget(expectedKey, "SAI_PORT_STAT_IF_IN_ERRORS", value);
    EXPECT_EQ(value, "200");

    fc.removeCounter(counterVid);
    EXPECT_EQ(fc.isEmpty(), true);
    countersTable.del(expectedKey);
    countersTable.getKeys(keys);
    ASSERT_TRUE(keys.empty());

    // Test again with queryStatsCapability support
    sai->mock_queryStatsCapability = [](sai_object_id_t, sai_object_type_t, sai_stat_capability_list_t *capability) {
        if (capability->count < 2)
        {
            capability->count = 2;
            return SAI_STATUS_BUFFER_OVERFLOW;
        }

        capability->list[0].stat_enum = SAI_PORT_STAT_IF_IN_OCTETS;
        capability->list[0].stat_modes = SAI_STATS_MODE_READ | SAI_STATS_MODE_READ_AND_CLEAR;
        capability->list[1].stat_enum = SAI_PORT_STAT_IF_IN_ERRORS;
        capability->list[1].stat_modes = SAI_STATS_MODE_READ | SAI_STATS_MODE_READ_AND_CLEAR;
        return SAI_STATUS_SUCCESS;
    };

    values.clear();
    values.emplace_back(PORT_COUNTER_ID_LIST, "SAI_PORT_STAT_IF_IN_OCTETS,SAI_PORT_STAT_IF_IN_ERRORS");
    fc.addCounter(counterVid, counterRid, values);
    EXPECT_EQ(fc.isEmpty(), false);

    waitForCounterValues(countersTable, expectedKey,
                      {"SAI_PORT_STAT_IF_IN_OCTETS", "SAI_PORT_STAT_IF_IN_ERRORS"},
                      {"100", "200"});

    fc.removeCounter(counterVid);
    EXPECT_EQ(fc.isEmpty(), true);
    countersTable.del(expectedKey);
    countersTable.getKeys(keys);
    ASSERT_TRUE(keys.empty());
}

TEST(FlexCounter, bulkCounter)
{
    sai->mock_getStatsExt = [&](sai_object_type_t, sai_object_id_t, uint32_t number_of_counters, const sai_stat_id_t *, sai_stats_mode_t, uint64_t *counters) {
        for (uint32_t i = 0; i < number_of_counters; i++)
        {
            counters[i] = (i + 1) * 10;
        }
        return SAI_STATUS_SUCCESS;
    };
    sai->mock_getStats = [&](sai_object_type_t, sai_object_id_t, uint32_t number_of_counters, const sai_stat_id_t *, uint64_t *counters) {
        for (uint32_t i = 0; i < number_of_counters; i++)
        {
            counters[i] = (i + 1) * 10;
        }
        return SAI_STATUS_SUCCESS;
    };
    int counterOffset = 0;
    int capabilities = 0;
    sai->mock_queryStatsCapability = [&](sai_object_id_t switch_id, sai_object_type_t object_type, sai_stat_capability_list_t *stats_capability) {
        // Assume all counters are in range [currentOffset, currentOffset + 250]
        if (stats_capability->count == 0)
        {
            stats_capability->count = 250;
            return SAI_STATUS_BUFFER_OVERFLOW;
        }
        else
        {
            for (int i = 0; i < 250; i++)
	    {
                stats_capability->list[i].stat_enum = i + counterOffset;
                if (capabilities == 0)
                {
                    stats_capability->list[i].stat_modes = (SAI_STATS_MODE_READ|SAI_STATS_MODE_BULK_READ|SAI_STATS_MODE_READ_AND_CLEAR|SAI_STATS_MODE_BULK_READ_AND_CLEAR|SAI_STATS_MODE_BULK_CLEAR);
                }
                else
                {
                    stats_capability->list[i].stat_modes = capabilities;
                }
            }
            return SAI_STATUS_SUCCESS;
        }
    };

    bool clearCalled = false;
    sai->mock_bulkGetStats = [&](sai_object_id_t,
                                sai_object_type_t,
                                uint32_t object_count,
                                const sai_object_key_t *object_keys,
                                uint32_t number_of_counters,
                                const sai_stat_id_t *counter_ids,
                                sai_stats_mode_t mode,
                                sai_status_t *object_status,
                                uint64_t *counters)
    {
        EXPECT_TRUE(mode == SAI_STATS_MODE_BULK_READ_AND_CLEAR || mode == SAI_STATS_MODE_BULK_READ);
        if (mode == SAI_STATS_MODE_BULK_READ_AND_CLEAR)
        {
            clearCalled = true;
        }
        for (uint32_t i = 0; i < object_count; i++)
        {
            object_status[i] = SAI_STATUS_SUCCESS;
            for (uint32_t j = 0; j < number_of_counters; j++)
            {
                counters[i * number_of_counters + j] = (j + 1) * 100;
            }
        }
        return SAI_STATUS_SUCCESS;
    };

    auto counterVerifyFunc = [] (swss::Table &countersTable, const std::string& key, const std::vector<std::string>& counterIdNames, const std::vector<std::string>& expectedValues)
    {
        std::string value;
        for (size_t i = 0; i < counterIdNames.size(); i++)
        {
            countersTable.hget(key, counterIdNames[i], value);
            ASSERT_EQ(value, expectedValues[i]);
        }
    };

    testAddRemoveCounter(
        2,
        SAI_OBJECT_TYPE_COUNTER,
        FLOW_COUNTER_ID_LIST,
        {"SAI_COUNTER_STAT_PACKETS", "SAI_COUNTER_STAT_BYTES"},
        {"100", "200"},
        counterVerifyFunc,
        true);

    testAddRemoveCounter(
        2,
        SAI_OBJECT_TYPE_MACSEC_FLOW,
        MACSEC_FLOW_COUNTER_ID_LIST,
        {"SAI_MACSEC_FLOW_STAT_CONTROL_PKTS", "SAI_MACSEC_FLOW_STAT_PKTS_UNTAGGED"},
        {"100", "200"},
        counterVerifyFunc,
        false);

    testAddRemoveCounter(
        2,
        SAI_OBJECT_TYPE_MACSEC_SA,
        MACSEC_SA_COUNTER_ID_LIST,
        {"SAI_MACSEC_SA_STAT_OCTETS_ENCRYPTED", "SAI_MACSEC_SA_STAT_OCTETS_PROTECTED"},
        {"100", "200"},
        counterVerifyFunc,
        false);

    testAddRemoveCounter(
        2,
        SAI_OBJECT_TYPE_PORT,
        PORT_COUNTER_ID_LIST,
        {"SAI_PORT_STAT_IF_IN_OCTETS", "SAI_PORT_STAT_IF_IN_UCAST_PKTS"},
        {"100", "200"},
        counterVerifyFunc,
        false);

    counterOffset = SAI_PORT_STAT_IN_DROP_REASON_RANGE_BASE;
    testAddRemoveCounter(
        2,
        SAI_OBJECT_TYPE_PORT,
        PORT_DEBUG_COUNTER_ID_LIST,
        {"SAI_PORT_STAT_IN_CONFIGURED_DROP_REASONS_0_DROPPED_PKTS", "SAI_PORT_STAT_IN_CONFIGURED_DROP_REASONS_1_DROPPED_PKTS"},
        {"100", "200"},
        counterVerifyFunc,
        false);
    counterOffset = 0;

    testAddRemoveCounter(
        2,
        SAI_OBJECT_TYPE_QUEUE,
        QUEUE_COUNTER_ID_LIST,
        {"SAI_QUEUE_STAT_PACKETS", "SAI_QUEUE_STAT_BYTES"},
        {"100", "200"},
        counterVerifyFunc,
        false,
        STATS_MODE_READ_AND_CLEAR);
    EXPECT_EQ(true, clearCalled);

    testAddRemoveCounter(
        2,
        SAI_OBJECT_TYPE_INGRESS_PRIORITY_GROUP,
        PG_COUNTER_ID_LIST,
        {"SAI_INGRESS_PRIORITY_GROUP_STAT_PACKETS", "SAI_INGRESS_PRIORITY_GROUP_STAT_BYTES"},
        {"100", "200"},
        counterVerifyFunc,
        false);

    testAddRemoveCounter(
        2,
        SAI_OBJECT_TYPE_ROUTER_INTERFACE,
        RIF_COUNTER_ID_LIST,
        {"SAI_ROUTER_INTERFACE_STAT_IN_OCTETS", "SAI_ROUTER_INTERFACE_STAT_IN_PACKETS"},
        {"100", "200"},
        counterVerifyFunc,
        false);

    counterOffset = SAI_SWITCH_STAT_IN_DROP_REASON_RANGE_BASE;
    testAddRemoveCounter(
        2,
        SAI_OBJECT_TYPE_SWITCH,
        SWITCH_DEBUG_COUNTER_ID_LIST,
        {"SAI_SWITCH_STAT_IN_CONFIGURED_DROP_REASONS_0_DROPPED_PKTS", "SAI_SWITCH_STAT_IN_CONFIGURED_DROP_REASONS_1_DROPPED_PKTS"},
        {"100", "200"},
        counterVerifyFunc,
        false);
    counterOffset = 0;

    testAddRemoveCounter(
        2,
        SAI_OBJECT_TYPE_TUNNEL,
        TUNNEL_COUNTER_ID_LIST,
        {"SAI_TUNNEL_STAT_IN_OCTETS", "SAI_TUNNEL_STAT_IN_PACKETS"},
        {"100", "200"},
        counterVerifyFunc,
        false);

    testAddRemoveCounter(
        2,
        (sai_object_type_t)SAI_OBJECT_TYPE_ENI,
        ENI_COUNTER_ID_LIST,
        {"SAI_ENI_STAT_FLOW_CREATED", "SAI_ENI_STAT_FLOW_CREATE_FAILED", "SAI_ENI_STAT_FLOW_DELETED", "SAI_ENI_STAT_FLOW_DELETE_FAILED"},
        {"100", "200", "300", "400"},
        counterVerifyFunc,
        true);

    clearCalled = false;
    capabilities = (SAI_STATS_MODE_READ|SAI_STATS_MODE_READ_AND_CLEAR);
    testAddRemoveCounter(
        2,
        SAI_OBJECT_TYPE_BUFFER_POOL,
        BUFFER_POOL_COUNTER_ID_LIST,
        {"SAI_BUFFER_POOL_STAT_CURR_OCCUPANCY_BYTES", "SAI_BUFFER_POOL_STAT_WATERMARK_BYTES"},
        {"10", "20"},
        counterVerifyFunc,
        false);
    // buffer pool stats does not support bulk
    EXPECT_EQ(false, clearCalled);

    capabilities = (SAI_STATS_MODE_READ|SAI_STATS_MODE_BULK_READ);
    testAddRemoveCounter(
        2,
        SAI_OBJECT_TYPE_POLICER,
        POLICER_COUNTER_ID_LIST,
        {"SAI_POLICER_STAT_PACKETS", "SAI_POLICER_STAT_ATTR_BYTES",
        "SAI_POLICER_STAT_GREEN_PACKETS", "SAI_POLICER_STAT_GREEN_BYTES",
        "SAI_POLICER_STAT_YELLOW_PACKETS", "SAI_POLICER_STAT_YELLOW_BYTES",
        "SAI_POLICER_STAT_RED_PACKETS", "SAI_POLICER_STAT_RED_BYTES"},
        {"100", "200", "300", "400", "500", "600", "700", "800"},
        counterVerifyFunc,
        false);

    testAddRemoveCounter(
        2,
        SAI_OBJECT_TYPE_COUNTER,
        SRV6_COUNTER_ID_LIST,
        {"SAI_COUNTER_STAT_PACKETS", "SAI_COUNTER_STAT_BYTES"},
        {"100", "200"},
        counterVerifyFunc,
        false);

    // Packet Trimming

    testAddRemoveCounter(
        2,
        SAI_OBJECT_TYPE_PORT,
        PORT_COUNTER_ID_LIST,
        {"SAI_PORT_STAT_TRIM_PACKETS", "SAI_PORT_STAT_DROPPED_TRIM_PACKETS", "SAI_PORT_STAT_TX_TRIM_PACKETS"},
        {"100", "200", "300"},
        counterVerifyFunc,
        false);

    testAddRemoveCounter(
        2,
        SAI_OBJECT_TYPE_QUEUE,
        QUEUE_COUNTER_ID_LIST,
        {"SAI_QUEUE_STAT_TRIM_PACKETS", "SAI_QUEUE_STAT_DROPPED_TRIM_PACKETS", "SAI_QUEUE_STAT_TX_TRIM_PACKETS"},
        {"100", "200", "300"},
        counterVerifyFunc,
        false);

    testAddRemoveCounter(
        2,
        SAI_OBJECT_TYPE_SWITCH,
        SWITCH_COUNTER_ID_LIST,
        {"SAI_SWITCH_STAT_DROPPED_TRIM_PACKETS", "SAI_SWITCH_STAT_TX_TRIM_PACKETS"},
        {"100", "200"},
        counterVerifyFunc,
        false);
}

TEST(FlexCounter, bulkChunksize)
{
    /*
     * Test logic
     * 1. Generate counter values and store them whenever the bulk get stat is called after initialization
     * 2. Convert stored counter values to string when the verify function is called
     *    and verify whether the database content aligns with the stored values
     * 3. Verify whether values of all counter IDs of all objects have been generated
     * 4. Verify whether the bulk chunk size is correct
     * 5. Simulate bulk-unsupported counter IDs which should be fetched by single call
     *    with both per counter bulk size supported or not
     *
     * A counter can be polled in initialization phase and runtime
     * For each test, it is expected to call bulk for "initialCheckCount" times during initialization.
     * In this stage, the mock function just return succeed for failure to indicate whether bulk poll is supported
     * but it does not provide a counter value for further check
     *
     * The calls to bulk starting from "initialCheckCount+1" are treated as runtime calls.
     * The counter values objects are generated as following:
     *   - a counterSeed maintains the current counter value to return
     *   - If the counterValuesMap[object_id][counter_id] exists, returns it as the counter's value
     *   - Otherwise, it's the first time the (object, counter ID) is polled
     *     - return the current value of counterSeed as the counter's value
     *     - store the counter's value into counterValuesMap
     *     - increase the counterSeed
     * When the test finishes, the counterSeed should equal (number_of_objects * number_of_bulk_supported_counters)
     * And all integer < counterSeed should be returned to one and only one (object, counter ID) tuple,
     * which can be verified by the verify function.
     *
     * For the bulk-unsupported counters, getStatExt will be called to poll counters, and return counter_id * OID as the counter's value
     */
    bool noBulkCapabilityOnly = false;
    std::set<sai_stat_id_t> bulkUnsupportedCounters;
    std::set<sai_stat_id_t> allCounters = {
        SAI_PORT_STAT_IF_IN_OCTETS,
        SAI_PORT_STAT_IF_IN_UCAST_PKTS,
        SAI_PORT_STAT_IF_OUT_QLEN,
        SAI_PORT_STAT_IF_IN_FEC_CORRECTABLE_FRAMES,
        SAI_PORT_STAT_IF_IN_FEC_NOT_CORRECTABLE_FRAMES
	};
    sai->mock_queryStatsCapability = [&](sai_object_id_t switch_id, sai_object_type_t object_type, sai_stat_capability_list_t *stats_capability) {
        std::set<sai_stat_id_t> &counterCapabilities = noBulkCapabilityOnly ? bulkUnsupportedCounters : allCounters;
        if (stats_capability->count == 0)
        {
            stats_capability->count = static_cast<uint32_t>(counterCapabilities.size());
            return SAI_STATUS_BUFFER_OVERFLOW;
        }
        else
        {
            int i = 0;
            for (auto stat: counterCapabilities)
            {
                stats_capability->list[i].stat_enum = stat;
                stats_capability->list[i].stat_modes = noBulkCapabilityOnly ? SAI_STATS_MODE_READ : (SAI_STATS_MODE_READ|SAI_STATS_MODE_BULK_READ);
                i++;
            }
            return SAI_STATUS_SUCCESS;
        }
    };

    // Map of number from {oid: {counter_id: counter value}}
    std::map<sai_object_id_t, std::map<sai_stat_id_t, sai_uint64_t>> counterValuesMap;
    // Map of string from {oid: {counter_id: counter value}}
    std::map<std::string, std::map<std::string, std::string>> expectedValuesMap;

    std::set<std::string> allCounterIds = {
        "SAI_PORT_STAT_IF_IN_OCTETS",
        "SAI_PORT_STAT_IF_IN_UCAST_PKTS",
        "SAI_PORT_STAT_IF_OUT_QLEN",
        "SAI_PORT_STAT_IF_IN_FEC_CORRECTABLE_FRAMES",
        "SAI_PORT_STAT_IF_IN_FEC_NOT_CORRECTABLE_FRAMES"
    };
    std::set<std::string> allObjectIds;
    auto generateExpectedValues = [&]()
    {
        std::set<sai_uint64_t> allCounterValueSet;
        for (const auto &oidRef : counterValuesMap)
        {
            auto &expected = expectedValuesMap[toOid(oidRef.first)];
            std::set<std::string> localAllCounterIds = allCounterIds;
            for (const auto &counters : oidRef.second)
            {
                // No duplicate counter value
                EXPECT_EQ(allCounterValueSet.find(counters.second), allCounterValueSet.end());
                allCounterValueSet.insert(counters.second);

                // For each object, no unexpected counter ID
                const auto &counterId = sai_serialize_port_stat((sai_port_stat_t)counters.first);
                EXPECT_TRUE(localAllCounterIds.find(counterId) != localAllCounterIds.end());
                localAllCounterIds.erase(counterId);

                expected[counterId] = to_string(counters.second);
            }

            // For each object, all expected counters are generated
            EXPECT_TRUE(localAllCounterIds.empty());
        }
    };

    // Bulk-unsupported counter IDs should be polled using single call (getStatExt)
    std::set<sai_object_id_t> bulkUnsupportedObjectIds;
    bool forceSingleCall = false;
    // <oid, <counter id, counter value>>
    auto _getStatsExt = [&](sai_object_type_t, sai_object_id_t oid, uint32_t number_of_counters, const sai_stat_id_t *counter_ids, sai_stats_mode_t, uint64_t *counters) {
        for (auto i = 0u; i < number_of_counters; i++)
        {
            if (forceSingleCall
                || (bulkUnsupportedCounters.find(counter_ids[i]) != bulkUnsupportedCounters.end()
                    && (bulkUnsupportedObjectIds.empty() || bulkUnsupportedObjectIds.find(oid) != bulkUnsupportedObjectIds.end())))
            {
                // avoid counter_id == 0 which causes the same counter value (0) for all objects
                counters[i] = (1 + counter_ids[i]) * (uint64_t)oid;
                if (counterValuesMap.find(oid) == counterValuesMap.end())
                {
                    counterValuesMap[oid] = {};
                }
                counterValuesMap[oid][counter_ids[i]] = counters[i];
            }
        }
        return SAI_STATUS_SUCCESS;
    };

    sai->mock_getStats = [&](sai_object_type_t type, sai_object_id_t oid, uint32_t number_of_counters, const sai_stat_id_t *counter_ids, uint64_t *counters) {
        return _getStatsExt(type, oid, number_of_counters, counter_ids, SAI_STATS_MODE_READ, counters);
    };

    sai->mock_getStatsExt = [&](sai_object_type_t type, sai_object_id_t oid, uint32_t number_of_counters, const sai_stat_id_t *counter_ids, sai_stats_mode_t mode, uint64_t *counters) {
        return _getStatsExt(type, oid, number_of_counters, counter_ids, mode, counters);
    };

    std::vector<std::vector<sai_stat_id_t>> counterRecord;
    std::vector<std::vector<uint64_t>> valueRecord;
    sai_uint64_t counterSeed = 0;
    // non zero unifiedBulkChunkSize indicates all counter IDs share the same bulk chunk size
    uint32_t unifiedBulkChunkSize = 0;
    int32_t initialCheckCount;
    int32_t partialSupportingBulkObjectFactor = 0;
    sai->mock_bulkGetStats = [&](sai_object_id_t,
                                sai_object_type_t,
                                uint32_t object_count,
                                const sai_object_key_t *object_keys,
                                uint32_t number_of_counters,
                                const sai_stat_id_t *counter_ids,
                                sai_stats_mode_t mode,
                                sai_status_t *object_status,
                                uint64_t *counters)
    {
        EXPECT_TRUE(mode == SAI_STATS_MODE_BULK_READ);
        std::vector<sai_stat_id_t> record;
        std::vector<uint64_t> value;
        if (initialCheckCount-- > 0)
        {
            allObjectIds.insert(toOid(object_keys[0].key.object_id));
            // This call is to check whether bulk counter polling is supported during initialization
            if (!bulkUnsupportedCounters.empty())
            {
                // Simulate counters that are not supported being polled in bulk mode
                for (auto i = 0u; i < number_of_counters; i++)
                {
                    if (bulkUnsupportedCounters.find(counter_ids[i]) != bulkUnsupportedCounters.end())
                    {
                        if (partialSupportingBulkObjectFactor != 0)
                        {
                            for(auto j = 0u; j < object_count; j++)
                            {
                                if (j % partialSupportingBulkObjectFactor == 0)
                                {
                                    bulkUnsupportedObjectIds.insert(object_keys[j].key.object_id);
                                }
                            }
                        }
                        return SAI_STATUS_FAILURE;
                    }
                }
            }
            return SAI_STATUS_SUCCESS;
        }

        EXPECT_TRUE(!forceSingleCall);

        for (uint32_t i = 0; i < object_count; i++)
        {
            object_status[i] = SAI_STATUS_SUCCESS;
            auto &counterMap = counterValuesMap[object_keys[i].key.object_id];
            for (uint32_t j = 0; j < number_of_counters; j++)
            {
                const auto &searchRef = counterMap.find(counter_ids[j]);
                if (searchRef == counterMap.end())
                {
                    counterMap[counter_ids[j]] = ++counterSeed;
                }
                counters[i * number_of_counters + j] = counterMap[counter_ids[j]];
                record.emplace_back(counter_ids[j]);
                value.emplace_back(counterSeed);
                // Only assert the unified chunk size when all counters are
                // polled together (merged state). Between the two
                // addCounterPlugin calls that set and then remove per-prefix
                // chunk sizes, the polling thread can poll with per-prefix
                // partitions that have fewer counters and different chunk
                // sizes. After the merge-back, FlexCounter also re-probes
                // bulk capability with single-object calls (object_count=1)
                // that have all counters. Skip the assertion for both
                // per-prefix polls and re-probe polls.
                if (unifiedBulkChunkSize > 0)
                {
                    if (object_count != unifiedBulkChunkSize
                        && number_of_counters == allCounters.size()
                        && object_count > 1)
                    {
                        EXPECT_EQ(object_count, unifiedBulkChunkSize);
                    }
                    continue;
                }
                // Skip re-probe polls (single-object capability probes
                // that happen after the merge-back, with object_count==1).
                // The per-counter assertions below check steady-state
                // per-prefix chunk sizes, which only apply when
                // object_count > 1. This matches the documented intent
                // in the comment above and the unified-path guard.
                if (object_count == 1)
                {
                    continue;
                }
                switch (counter_ids[j])
                {
                case SAI_PORT_STAT_IF_IN_OCTETS:
                case SAI_PORT_STAT_IF_IN_UCAST_PKTS:
                    // default chunk size 2, object number 6, object count 6 / 2 = 3
                    EXPECT_EQ(object_count, 3);
                    break;
                case SAI_PORT_STAT_IF_OUT_QLEN:
                    // queue length chunk size 0, object number 6, object count 6
                    EXPECT_EQ(object_count, 6);
                    break;
                case SAI_PORT_STAT_IF_IN_FEC_CORRECTABLE_FRAMES:
                case SAI_PORT_STAT_IF_IN_FEC_NOT_CORRECTABLE_FRAMES:
                    // FEC chunk size 2, object number 6, object count 6 / 3 = 2
                    EXPECT_EQ(object_count, 2);
                default:
                    break;
                }
            }
        }
        return SAI_STATUS_SUCCESS;
    };

    auto counterVerifyFunc = [&] (swss::Table &countersTable, const std::string& key, const std::vector<std::string>& counterIdNames, const std::vector<std::string>&)
    {
        std::string value;
        if (expectedValuesMap.empty())
        {
            generateExpectedValues();
        }
        auto const &searchRef = expectedValuesMap.find(key);
        ASSERT_TRUE(searchRef != expectedValuesMap.end());
        auto &oidCounters = searchRef->second;

        for (auto const &counter : counterIdNames)
        {
            countersTable.hget(key, counter, value);
            EXPECT_EQ(value, oidCounters[counter]);
            oidCounters.erase(counter);
        }

        EXPECT_TRUE(oidCounters.empty());
        expectedValuesMap.erase(searchRef);

        allObjectIds.erase(key);
    };

    // create ports first and then set bulk chunk size + per counter bulk chunk size
    initialCheckCount = 6;
    testAddRemoveCounter(
        6,
        SAI_OBJECT_TYPE_PORT,
        PORT_COUNTER_ID_LIST,
        {"SAI_PORT_STAT_IF_IN_OCTETS", "SAI_PORT_STAT_IF_IN_UCAST_PKTS", "SAI_PORT_STAT_IF_OUT_QLEN", "SAI_PORT_STAT_IF_IN_FEC_CORRECTABLE_FRAMES", "SAI_PORT_STAT_IF_IN_FEC_NOT_CORRECTABLE_FRAMES"},
        {},
        counterVerifyFunc,
        false,
        STATS_MODE_READ,
        false,
        "3",
        "SAI_PORT_STAT_IF_OUT_QLEN:0;SAI_PORT_STAT_IF_IN_FEC:2");
    EXPECT_TRUE(allObjectIds.empty());

    // Set the secondary poll factor explicitly. Other calls omit it to cover
    // the legacy/default behavior.
    initialCheckCount = 6;
    testAddRemoveCounter(
        6,
        SAI_OBJECT_TYPE_PORT,
        PORT_COUNTER_ID_LIST,
        {"SAI_PORT_STAT_IF_IN_OCTETS", "SAI_PORT_STAT_IF_IN_UCAST_PKTS", "SAI_PORT_STAT_IF_OUT_QLEN", "SAI_PORT_STAT_IF_IN_FEC_CORRECTABLE_FRAMES", "SAI_PORT_STAT_IF_IN_FEC_NOT_CORRECTABLE_FRAMES"},
        {},
        counterVerifyFunc,
        false,
        STATS_MODE_READ,
        false,
        "3",
        "SAI_PORT_STAT_IF_OUT_QLEN:0;SAI_PORT_STAT_IF_IN_FEC:2",
        false,
        PORT_PLUGIN_FIELD,
        false,
        true,
        "2");
    EXPECT_TRUE(allObjectIds.empty());

    // Remove per counter bulk chunk size after initializing it
    // This is to cover the scenario of removing per counter bulk chunk size filed
    // All counters share a unified bulk chunk size
    unifiedBulkChunkSize = 3;
    initialCheckCount = 6;
    testAddRemoveCounter(
        6,
        SAI_OBJECT_TYPE_PORT,
        PORT_COUNTER_ID_LIST,
        {"SAI_PORT_STAT_IF_IN_OCTETS", "SAI_PORT_STAT_IF_IN_UCAST_PKTS", "SAI_PORT_STAT_IF_OUT_QLEN", "SAI_PORT_STAT_IF_IN_FEC_CORRECTABLE_FRAMES", "SAI_PORT_STAT_IF_IN_FEC_NOT_CORRECTABLE_FRAMES"},
        {},
        counterVerifyFunc,
        false,
        STATS_MODE_READ,
        false,
        "3",
        "SAI_PORT_STAT_IF_OUT_QLEN:0;SAI_PORT_STAT_IF_IN_FEC:2",
        true,
        "",
        true);
    EXPECT_TRUE(allObjectIds.empty());
    unifiedBulkChunkSize = 0;

    // add ports counters in bulk mode first and then set bulk chunk size + per counter bulk chunk size
    initialCheckCount = 3;
    testAddRemoveCounter(
        6,
        SAI_OBJECT_TYPE_PORT,
        PORT_COUNTER_ID_LIST,
        {"SAI_PORT_STAT_IF_IN_OCTETS", "SAI_PORT_STAT_IF_IN_UCAST_PKTS", "SAI_PORT_STAT_IF_OUT_QLEN", "SAI_PORT_STAT_IF_IN_FEC_CORRECTABLE_FRAMES", "SAI_PORT_STAT_IF_IN_FEC_NOT_CORRECTABLE_FRAMES"},
        {},
        counterVerifyFunc,
        false,
        STATS_MODE_READ,
        true,
        "3",
        "SAI_PORT_STAT_IF_OUT_QLEN:0;SAI_PORT_STAT_IF_IN_FEC:2");
    EXPECT_TRUE(allObjectIds.empty());

    // set bulk chunk size + per counter bulk chunk size first and then add ports counters in bulk mode
    initialCheckCount = 3;
    testAddRemoveCounter(
        6,
        SAI_OBJECT_TYPE_PORT,
        PORT_COUNTER_ID_LIST,
        {"SAI_PORT_STAT_IF_IN_OCTETS", "SAI_PORT_STAT_IF_IN_UCAST_PKTS", "SAI_PORT_STAT_IF_OUT_QLEN", "SAI_PORT_STAT_IF_IN_FEC_CORRECTABLE_FRAMES", "SAI_PORT_STAT_IF_IN_FEC_NOT_CORRECTABLE_FRAMES"},
        {},
        counterVerifyFunc,
        false,
        STATS_MODE_READ,
        true,
        "3",
        "SAI_PORT_STAT_IF_OUT_QLEN:0;SAI_PORT_STAT_IF_IN_FEC:2",
        false,
        PORT_PLUGIN_FIELD);
    EXPECT_TRUE(allObjectIds.empty());

    // add ports counters in bulk mode with some bulk-unsupported counters first and then set bulk chunk size + per counter bulk chunk size
    // all counters will be polled using single call in runtime
    forceSingleCall = true;
    initialCheckCount = 1; // check bulk for all counter IDs altogether
    initialCheckCount += 6; // for bulk unsupported counter prefix, check bulk again for each objects
    bulkUnsupportedCounters = {
        SAI_PORT_STAT_IF_IN_FEC_CORRECTABLE_FRAMES,
        SAI_PORT_STAT_IF_IN_FEC_NOT_CORRECTABLE_FRAMES
    };
    testAddRemoveCounter(
        6,
        SAI_OBJECT_TYPE_PORT,
        PORT_COUNTER_ID_LIST,
        {"SAI_PORT_STAT_IF_IN_OCTETS", "SAI_PORT_STAT_IF_IN_UCAST_PKTS", "SAI_PORT_STAT_IF_OUT_QLEN", "SAI_PORT_STAT_IF_IN_FEC_CORRECTABLE_FRAMES", "SAI_PORT_STAT_IF_IN_FEC_NOT_CORRECTABLE_FRAMES"},
        {},
        counterVerifyFunc,
        false,
        STATS_MODE_READ,
        true,
        "3",
        "SAI_PORT_STAT_IF_OUT_QLEN:0;SAI_PORT_STAT_IF_IN_FEC:2");
    EXPECT_TRUE(allObjectIds.empty());
    forceSingleCall = false;

    forceSingleCall = true;
    noBulkCapabilityOnly = true;
    initialCheckCount = 0; // check bulk for all counter IDs altogether
    bulkUnsupportedCounters = {
	SAI_PORT_STAT_IF_IN_OCTETS,
	SAI_PORT_STAT_IF_IN_UCAST_PKTS,
	SAI_PORT_STAT_IF_OUT_QLEN,
        SAI_PORT_STAT_IF_IN_FEC_CORRECTABLE_FRAMES,
        SAI_PORT_STAT_IF_IN_FEC_NOT_CORRECTABLE_FRAMES
	};
    testAddRemoveCounter(
        6,
        SAI_OBJECT_TYPE_PORT,
        PORT_COUNTER_ID_LIST,
        {"SAI_PORT_STAT_IF_IN_OCTETS", "SAI_PORT_STAT_IF_IN_UCAST_PKTS", "SAI_PORT_STAT_IF_OUT_QLEN", "SAI_PORT_STAT_IF_IN_FEC_CORRECTABLE_FRAMES", "SAI_PORT_STAT_IF_IN_FEC_NOT_CORRECTABLE_FRAMES"},
        {},
        counterVerifyFunc,
        false,
        STATS_MODE_READ,
        true);
    EXPECT_TRUE(allObjectIds.empty());
    bulkUnsupportedCounters.clear();
    noBulkCapabilityOnly = false;
    forceSingleCall = false;

    // set bulk chunk size + per counter bulk chunk size first and then add ports counters in bulk mode with some bulk-unsupported counters
    // All bulk-unsupported counters are polled using single call and all the rest counters are polled using bulk call
    // For each OID, it will be in both m_bulkContexts and m_objectIdsMap
    initialCheckCount = 3; // check bulk for 3 prefixes
    initialCheckCount += 6; // for bulk unsupported counter prefix, check bulk again for each objects
    testAddRemoveCounter(
        6,
        SAI_OBJECT_TYPE_PORT,
        PORT_COUNTER_ID_LIST,
        {"SAI_PORT_STAT_IF_IN_OCTETS", "SAI_PORT_STAT_IF_IN_UCAST_PKTS", "SAI_PORT_STAT_IF_OUT_QLEN", "SAI_PORT_STAT_IF_IN_FEC_CORRECTABLE_FRAMES", "SAI_PORT_STAT_IF_IN_FEC_NOT_CORRECTABLE_FRAMES"},
        {},
        counterVerifyFunc,
        false,
        STATS_MODE_READ,
        true,
        "3",
        "SAI_PORT_STAT_IF_OUT_QLEN:0;SAI_PORT_STAT_IF_IN_FEC:2",
        false,
        PORT_PLUGIN_FIELD);
    EXPECT_TRUE(allObjectIds.empty());

    // set bulk chunk size + per counter bulk chunk size first and then add ports counters in bulk mode with some bulk-unsupported counters
    // All bulk-unsupported counters are polled using single call and all the rest counters are polled using bulk call
    // For each OID, it will be in both m_bulkContexts and m_objectIdsMap
    initialCheckCount = 3; // check bulk for 3 prefixes
    initialCheckCount += 6; // for bulk unsupported counter prefix, check bulk again for each objects
    partialSupportingBulkObjectFactor = 2;
    testAddRemoveCounter(
        6,
        SAI_OBJECT_TYPE_PORT,
        PORT_COUNTER_ID_LIST,
        {"SAI_PORT_STAT_IF_IN_OCTETS", "SAI_PORT_STAT_IF_IN_UCAST_PKTS", "SAI_PORT_STAT_IF_OUT_QLEN", "SAI_PORT_STAT_IF_IN_FEC_CORRECTABLE_FRAMES", "SAI_PORT_STAT_IF_IN_FEC_NOT_CORRECTABLE_FRAMES"},
        {},
        counterVerifyFunc,
        false,
        STATS_MODE_READ,
        true,
        "3",
        "SAI_PORT_STAT_IF_OUT_QLEN:0;SAI_PORT_STAT_IF_IN_FEC:2",
        false,
        PORT_PLUGIN_FIELD);
    EXPECT_TRUE(allObjectIds.empty());
}

TEST(FlexCounter, counterIdChange)
{
    sai->mock_queryStatsCapability = [](sai_object_id_t, sai_object_type_t, sai_stat_capability_list_t *capability) {
        return SAI_STATUS_FAILURE;
    };
    sai->mock_getStats = [&](sai_object_type_t, sai_object_id_t, uint32_t number_of_counters, const sai_stat_id_t *, uint64_t *counters) {
        for (uint32_t i = 0; i < number_of_counters; i++)
        {
            counters[i] = (i + 1) * 10;
        }
        return SAI_STATUS_SUCCESS;
    };
    sai->mock_bulkGetStats = [&](sai_object_id_t,
                                sai_object_type_t,
                                uint32_t object_count,
                                const sai_object_key_t *object_keys,
                                uint32_t number_of_counters,
                                const sai_stat_id_t *counter_ids,
                                sai_stats_mode_t mode,
                                sai_status_t *object_status,
                                uint64_t *counters)
    {
        for (uint32_t i = 0; i < number_of_counters; i++)
        {
            switch(counter_ids[i])
            {
                case SAI_PORT_STAT_IF_IN_OCTETS:
                case SAI_PORT_STAT_IF_IN_UCAST_PKTS:
                    break;
                default:
                    return SAI_STATUS_NOT_SUPPORTED;
            }
        }

        for (uint32_t i = 0; i < object_count; i++)
        {
            object_status[i] = SAI_STATUS_SUCCESS;
            for (uint32_t j = 0; j < number_of_counters; j++)
            {
                counters[i * number_of_counters + j] = (j + 1) * 100;
            }
        }
        return SAI_STATUS_SUCCESS;
    };

    FlexCounter fc("test", sai, "COUNTERS_DB");

    test_syncd::mockVidManagerObjectTypeQuery(SAI_OBJECT_TYPE_PORT);

    std::vector<swss::FieldValueTuple> values;
    values.emplace_back(POLL_INTERVAL_FIELD, "1000");
    values.emplace_back(FLEX_COUNTER_STATUS_FIELD, "enable");
    values.emplace_back(STATS_MODE_FIELD, STATS_MODE_READ);
    fc.addCounterPlugin(values);

    values.clear();
    values.emplace_back(PORT_COUNTER_ID_LIST, "SAI_PORT_STAT_IF_IN_NON_UCAST_PKTS,SAI_PORT_STAT_IF_IN_DISCARDS");
    sai_object_id_t oid{0x1000000000000};
    fc.addCounter(oid, oid, values);

    swss::DBConnector db("COUNTERS_DB", 0);
    swss::RedisPipeline pipeline(&db);
    swss::Table countersTable(&pipeline, COUNTERS_TABLE, false);

    waitForCounterKeys(countersTable, 1);

    std::vector<std::string> keys;
    countersTable.getKeys(keys);
    EXPECT_EQ(keys.size(),1);
    std::string expectedKey = toOid(oid);
    waitForCounterValues(countersTable,
                      expectedKey,
                      {"SAI_PORT_STAT_IF_IN_NON_UCAST_PKTS", "SAI_PORT_STAT_IF_IN_DISCARDS"},
                      {"10", "20"});

    // not support bulk to support bulk
    values.clear();
    values.emplace_back(PORT_COUNTER_ID_LIST, "SAI_PORT_STAT_IF_IN_OCTETS,SAI_PORT_STAT_IF_IN_UCAST_PKTS");
    fc.addCounter(oid, oid, values);

    waitForCounterValues(countersTable,
                      expectedKey,
                      {"SAI_PORT_STAT_IF_IN_OCTETS", "SAI_PORT_STAT_IF_IN_UCAST_PKTS"},
                      {"100", "200"});

    // support bulk but counter id changes
    values.clear();
    values.emplace_back(PORT_COUNTER_ID_LIST, "SAI_PORT_STAT_IF_IN_OCTETS");
    fc.addCounter(oid, oid, values);

    waitForCounterValues(countersTable,
                      expectedKey,
                      {"SAI_PORT_STAT_IF_IN_OCTETS"},
                      {"100"});

    // support bulk with different counter id
    sai_object_id_t oid1{0x1000000000001};
    values.clear();
    values.emplace_back(PORT_COUNTER_ID_LIST, "SAI_PORT_STAT_IF_IN_OCTETS,SAI_PORT_STAT_IF_IN_UCAST_PKTS");
    fc.addCounter(oid1, oid1, values);

    waitForCounterKeys(countersTable, 2);
    waitForCounterValues(countersTable,
                      toOid(oid1),
                      {"SAI_PORT_STAT_IF_IN_OCTETS", "SAI_PORT_STAT_IF_IN_UCAST_PKTS"},
                      {"100", "200"});

    // support bulk to not support bulk
    values.clear();
    values.emplace_back(PORT_COUNTER_ID_LIST, "SAI_PORT_STAT_IF_IN_UCAST_PKTS,SAI_PORT_STAT_IF_IN_NON_UCAST_PKTS");
    fc.addCounter(oid, oid, values);

    waitForCounterValues(countersTable,
                      expectedKey,
                      {"SAI_PORT_STAT_IF_IN_UCAST_PKTS","SAI_PORT_STAT_IF_IN_NON_UCAST_PKTS"},
                      {"10", "20"});

    // not support bulk but counter id changes
    values.clear();
    values.emplace_back(PORT_COUNTER_ID_LIST, "SAI_PORT_STAT_IF_IN_NON_UCAST_PKTS,SAI_PORT_STAT_IF_IN_DISCARDS");
    fc.addCounter(oid, oid, values);
    waitForCounterValues(countersTable,
                      expectedKey,
                      {"SAI_PORT_STAT_IF_IN_NON_UCAST_PKTS", "SAI_PORT_STAT_IF_IN_DISCARDS"},
                      {"10", "20"});

    // verify oid1 is still using bulk
    waitForCounterValues(countersTable,
                      toOid(oid1),
                      {"SAI_PORT_STAT_IF_IN_OCTETS", "SAI_PORT_STAT_IF_IN_UCAST_PKTS"},
                      {"100", "200"});

    fc.removeCounter(oid);
    countersTable.del(expectedKey);
    fc.removeCounter(oid1);
    countersTable.del(toOid(oid1));
}

using dash_meter_expected_val_t = std::vector<std::vector<std::string>>;
constexpr uint32_t DASH_NUM_METER_BUCKETS_PER_ENI = 4094;
using VerifyDashMeterStatsFunc = std::function<void(swss::Table &countersTable,
                                                    sai_object_id_t eni_id,
                                                    const std::vector<std::string>& counterIdNames,
                                                    const dash_meter_expected_val_t& expectedValues)>;

void testDashMeterAddRemoveCounter(
        const std::string& counterIdFieldName,
        const std::vector<std::string>& counterIdNames,
        const dash_meter_expected_val_t& expectedValues,
        VerifyDashMeterStatsFunc verifyFunc,
        bool supportedCounters,
        const std::string statsMode = STATS_MODE_READ)
{
    SWSS_LOG_ENTER();

    FlexCounter fc("test", sai, "COUNTERS_DB");

    sai_object_type_t object_type = (sai_object_type_t)SAI_OBJECT_TYPE_ENI;
    test_syncd::mockVidManagerObjectTypeQuery(object_type);

    const unsigned int numOid = 5;
    std::vector<sai_object_id_t> object_ids = generateOids(numOid, object_type);
    EXPECT_EQ(object_ids.size(), numOid);

    std::vector<swss::FieldValueTuple> values;
    values.emplace_back(POLL_INTERVAL_FIELD, "1000");
    values.emplace_back(FLEX_COUNTER_STATUS_FIELD, "enable");
    values.emplace_back(STATS_MODE_FIELD, statsMode);
    fc.addCounterPlugin(values);

    values.clear();
    values.emplace_back(counterIdFieldName, join(counterIdNames));
    for (auto object_id : object_ids)
    {
        fc.addCounter(object_id, object_id, values);
    }

    if (supportedCounters)
    {
        EXPECT_EQ(fc.isEmpty(), false);

        size_t expectedMeterKeys = 0;
        for (uint32_t i = 0; i < DASH_NUM_METER_BUCKETS_PER_ENI; ++i) {
            if (i % 100 == 0) expectedMeterKeys++;
        }
        expectedMeterKeys *= object_ids.size();

        swss::DBConnector pollDb("COUNTERS_DB", 0);
        swss::RedisPipeline pollPipeline(&pollDb);
        swss::Table pollTable(&pollPipeline, COUNTERS_TABLE, false);
        waitForCounterKeys(pollTable, expectedMeterKeys);
    }
    else
    {
        // No supported counter, this object shall not be queried
        EXPECT_EQ(fc.isEmpty(), true);
    }
    swss::DBConnector db("COUNTERS_DB", 0);
    swss::RedisPipeline pipeline(&db);
    swss::Table countersTable(&pipeline, COUNTERS_TABLE, false);

    if (supportedCounters)
    {
        for (size_t i = 0; i < object_ids.size(); i++)
        {
            verifyFunc(countersTable, object_ids[i], counterIdNames, expectedValues);
        }
    }

    for (auto object_id : object_ids)
    {
        fc.removeCounter(object_id);
    }
    EXPECT_EQ(fc.isEmpty(), true);

    std::vector<std::string> keys;
    countersTable.getKeys(keys);
    removeTimeStamp(keys, countersTable);
    ASSERT_TRUE(keys.empty());
}

void dash_meter_fill_values (uint32_t object_num, uint32_t num_counters, uint64_t* counters, dash_meter_expected_val_t* str_counters)
{
    SWSS_LOG_ENTER();

    if (object_num % 100 != 0) {
        if (counters != nullptr) {
            for (uint32_t i = 0; i < num_counters; i++)
            {
                counters[i] = 0;
            }
        }
        return;
    }
    if (counters == nullptr) {
        str_counters->emplace_back();
    }
    for (uint32_t i = 0; i < num_counters; i++)
    {
        auto value = (object_num * 1000) + (i + 1) * 100;
        if (counters != nullptr) {
            counters[i] = value;
        } else {
            str_counters->back().push_back(std::to_string(value));
        }
    }
};

TEST(FlexCounter, addRemoveDashMeterCounter)
{
    sai->mock_queryStatsCapability = [](sai_object_id_t switch_id, sai_object_type_t object_type, sai_stat_capability_list_t *stats_capability)
    {
        sai_stat_id_t meter_stats_cap[] = {
            SAI_METER_BUCKET_ENTRY_STAT_INBOUND_BYTES,
            SAI_METER_BUCKET_ENTRY_STAT_OUTBOUND_BYTES
        };
        EXPECT_TRUE(object_type == (sai_object_type_t)SAI_OBJECT_TYPE_METER_BUCKET_ENTRY);
        stats_capability->count = sizeof(meter_stats_cap) / sizeof(sai_stat_id_t);
        if (stats_capability->list == nullptr) {
            return SAI_STATUS_BUFFER_OVERFLOW;
        }
        for (uint32_t i = 0; i < stats_capability->count; ++i) {
            stats_capability->list[i].stat_enum = meter_stats_cap[i];
            stats_capability->list[i].stat_modes = SAI_STATS_MODE_READ;
        }
        return SAI_STATUS_SUCCESS;
    };

    sai->mock_get = [] (sai_object_type_t objectType, sai_object_id_t objectId, uint32_t attr_count, sai_attribute_t *attr_list)
    {
        for (uint32_t i = 0; i < attr_count; i++)
        {
            if (attr_list[i].id == SAI_SWITCH_ATTR_DASH_CAPS_MAX_METER_BUCKET_COUNT_PER_ENI)
            {
                attr_list[i].value.u32 = DASH_NUM_METER_BUCKETS_PER_ENI;
            }
        }
        return SAI_STATUS_SUCCESS;
    };

    sai->mock_bulkGetStats = [](sai_object_id_t,
                                sai_object_type_t object_type,
                                uint32_t object_count,
                                const sai_object_key_t *object_keys,
                                uint32_t number_of_counters,
                                const sai_stat_id_t *counter_ids,
                                sai_stats_mode_t mode,
                                sai_status_t *object_status,
                                uint64_t *counters)
    {
        EXPECT_TRUE(object_type == (sai_object_type_t)SAI_OBJECT_TYPE_METER_BUCKET_ENTRY);
        EXPECT_TRUE(object_count == DASH_NUM_METER_BUCKETS_PER_ENI);
        EXPECT_EQ(number_of_counters, 2);
        for (uint32_t i = 0; i < object_count; ++i)
        {
            EXPECT_EQ(object_keys[i].key.meter_bucket_entry.meter_class, i + 1);
            dash_meter_fill_values(i, number_of_counters, &(counters[i * number_of_counters]), nullptr);
            object_status[i] = SAI_STATUS_SUCCESS;
        }
        return SAI_STATUS_SUCCESS;
    };

    auto counterVerifyFunc = [] (swss::Table &countersTable, sai_object_id_t eni_id, const std::vector<std::string>& counterIdNames, const dash_meter_expected_val_t& expectedValues)
    {
        std::string value;
        auto switchVid = VidManager::switchIdQuery(eni_id);
        for (uint32_t i = 0; i < (expectedValues.size()/counterIdNames.size()); i++)
        {
            auto entry_key = sai_meter_bucket_entry_t {.switch_id = switchVid, .eni_id = eni_id,
                                                       .meter_class = (i*100) + 1};
            auto key = sai_serialize_meter_bucket_entry(entry_key);
            for (size_t j = 0; j < 2; ++j) {
                countersTable.hget(key, counterIdNames[j], value);
                EXPECT_EQ(value, expectedValues[i][j]);
            }
        }
    };
    dash_meter_expected_val_t expectedValues;

    for (uint32_t i = 0; i < DASH_NUM_METER_BUCKETS_PER_ENI; ++i) {
        dash_meter_fill_values(i, 2, nullptr, &expectedValues);
    }

    testDashMeterAddRemoveCounter(
        DASH_METER_COUNTER_ID_LIST,
        {"SAI_METER_BUCKET_ENTRY_STAT_OUTBOUND_BYTES", "SAI_METER_BUCKET_ENTRY_STAT_INBOUND_BYTES"},
        expectedValues,
        counterVerifyFunc,
        true);
}

TEST(FlexCounter, removeEniDeletesBothEniAndDashMeterCounters)
{
    sai->mock_getStatsExt = [](sai_object_type_t, sai_object_id_t, uint32_t number_of_counters, const sai_stat_id_t *, sai_stats_mode_t, uint64_t *counters) {
        for (uint32_t i = 0; i < number_of_counters; i++)
        {
            counters[i] = (i + 1) * 100;
        }
        return SAI_STATUS_SUCCESS;
    };

    sai->mock_queryStatsCapability = [](sai_object_id_t, sai_object_type_t object_type, sai_stat_capability_list_t *stats_capability)
    {
        if (object_type == (sai_object_type_t)SAI_OBJECT_TYPE_METER_BUCKET_ENTRY)
        {
            sai_stat_id_t meter_stats_cap[] = {
                SAI_METER_BUCKET_ENTRY_STAT_INBOUND_BYTES,
                SAI_METER_BUCKET_ENTRY_STAT_OUTBOUND_BYTES
            };
            stats_capability->count = sizeof(meter_stats_cap) / sizeof(sai_stat_id_t);
            if (stats_capability->list == nullptr) {
                return SAI_STATUS_BUFFER_OVERFLOW;
            }
            for (uint32_t i = 0; i < stats_capability->count; ++i) {
                stats_capability->list[i].stat_enum = meter_stats_cap[i];
                stats_capability->list[i].stat_modes = SAI_STATS_MODE_READ;
            }
            return SAI_STATUS_SUCCESS;
        }
        return SAI_STATUS_FAILURE;
    };

    sai->mock_get = [] (sai_object_type_t, sai_object_id_t, uint32_t attr_count, sai_attribute_t *attr_list)
    {
        for (uint32_t i = 0; i < attr_count; i++)
        {
            if (attr_list[i].id == SAI_SWITCH_ATTR_DASH_CAPS_MAX_METER_BUCKET_COUNT_PER_ENI)
            {
                attr_list[i].value.u32 = DASH_NUM_METER_BUCKETS_PER_ENI;
            }
        }
        return SAI_STATUS_SUCCESS;
    };

    sai->mock_bulkGetStats = [](sai_object_id_t,
                                sai_object_type_t,
                                uint32_t object_count,
                                const sai_object_key_t *,
                                uint32_t number_of_counters,
                                const sai_stat_id_t *,
                                sai_stats_mode_t,
                                sai_status_t *object_status,
                                uint64_t *counters)
    {
        for (uint32_t i = 0; i < object_count; ++i)
        {
            dash_meter_fill_values(i, number_of_counters, &(counters[i * number_of_counters]), nullptr);
            object_status[i] = SAI_STATUS_SUCCESS;
        }
        return SAI_STATUS_SUCCESS;
    };

    FlexCounter fc("test", sai, "COUNTERS_DB");

    sai_object_type_t object_type = (sai_object_type_t)SAI_OBJECT_TYPE_ENI;
    test_syncd::mockVidManagerObjectTypeQuery(object_type);

    std::vector<sai_object_id_t> object_ids = generateOids(2, object_type);

    // Enable flex counter polling
    std::vector<swss::FieldValueTuple> pluginValues;
    pluginValues.emplace_back(POLL_INTERVAL_FIELD, "1000");
    pluginValues.emplace_back(FLEX_COUNTER_STATUS_FIELD, "enable");
    pluginValues.emplace_back(STATS_MODE_FIELD, STATS_MODE_READ);
    fc.addCounterPlugin(pluginValues);

    // Add both ENI counter IDs and DASH meter counter IDs for each ENI object
    std::vector<swss::FieldValueTuple> counterValues;
    counterValues.emplace_back(ENI_COUNTER_ID_LIST,
        "SAI_ENI_STAT_FLOW_CREATED,SAI_ENI_STAT_FLOW_CREATE_FAILED");
    counterValues.emplace_back(DASH_METER_COUNTER_ID_LIST,
        "SAI_METER_BUCKET_ENTRY_STAT_OUTBOUND_BYTES,SAI_METER_BUCKET_ENTRY_STAT_INBOUND_BYTES");
    for (auto oid : object_ids)
    {
        fc.addCounter(oid, oid, counterValues);
    }

    EXPECT_FALSE(fc.isEmpty());

    // Wait for ENI counter keys to appear in COUNTERS_DB
    swss::DBConnector db("COUNTERS_DB", 0);
    swss::RedisPipeline pipeline(&db);
    swss::Table countersTable(&pipeline, COUNTERS_TABLE, false);

    // Wait for ENI counter entries to be populated
    waitForCounterKeys(countersTable, object_ids.size());

    // Wait for the first ENI counter value to be written by a real poll cycle
    std::string firstEniKey = toOid(object_ids[0]);
    waitForCounterValues(countersTable, firstEniKey,
        {"SAI_ENI_STAT_FLOW_CREATED"}, {"100"});

    // Verify meter bucket entries also exist
    auto switchVid = VidManager::switchIdQuery(object_ids[0]);
    auto meterEntryKey = sai_meter_bucket_entry_t {
        .switch_id = switchVid, .eni_id = object_ids[0], .meter_class = 1};
    auto meterKey = sai_serialize_meter_bucket_entry(meterEntryKey);
    waitForNonZeroCounterValue(countersTable, meterKey,
        "SAI_METER_BUCKET_ENTRY_STAT_OUTBOUND_BYTES");

    // Verify we have both ENI and meter bucket keys
    std::vector<std::string> keys;
    countersTable.getKeys(keys);
    removeTimeStamp(keys, countersTable);
    EXPECT_GT(keys.size(), object_ids.size());

    // Remove all ENI counters — should clean up both ENI and meter bucket entries
    for (auto oid : object_ids)
    {
        fc.removeCounter(oid);
    }
    EXPECT_TRUE(fc.isEmpty());

    // Verify all counter entries (both ENI and DASH meter) are deleted
    countersTable.getKeys(keys);
    removeTimeStamp(keys, countersTable);
    ASSERT_TRUE(keys.empty());
}

TEST(FlexCounter, noSupportedDashMeterCounter)
{
    sai->mock_queryStatsCapability = [](sai_object_id_t switch_id, sai_object_type_t object_type, sai_stat_capability_list_t *stats_capability) {
        EXPECT_TRUE(object_type == (sai_object_type_t)SAI_OBJECT_TYPE_METER_BUCKET_ENTRY);
        return SAI_STATUS_FAILURE;
    };
    auto counterVerifyFunc = [] (swss::Table &countersTable, sai_object_id_t eni_id, const std::vector<std::string>& counterIdNames, const dash_meter_expected_val_t& expectedValues)
    {
    };
    dash_meter_expected_val_t expectedValues;

    testDashMeterAddRemoveCounter(
        DASH_METER_COUNTER_ID_LIST,
        {"SAI_METER_BUCKET_ENTRY_STAT_OUTBOUND_BYTES", "SAI_METER_BUCKET_ENTRY_STAT_INBOUND_BYTES"},
        expectedValues,
        counterVerifyFunc,
        false);
}

TEST(FlexCounter, noEniDashMeterCounter)
{
    sai->mock_queryStatsCapability = [](sai_object_id_t switch_id, sai_object_type_t object_type, sai_stat_capability_list_t *stats_capability)
    {
        sai_stat_id_t meter_stats_cap[] = {
            SAI_METER_BUCKET_ENTRY_STAT_INBOUND_BYTES,
            SAI_METER_BUCKET_ENTRY_STAT_OUTBOUND_BYTES
        };
        EXPECT_TRUE(object_type == (sai_object_type_t)SAI_OBJECT_TYPE_METER_BUCKET_ENTRY);
        stats_capability->count = sizeof(meter_stats_cap) / sizeof(sai_stat_id_t);
        if (stats_capability->list == nullptr) {
            return SAI_STATUS_BUFFER_OVERFLOW;
        }
        for (uint32_t i = 0; i < stats_capability->count; ++i) {
            stats_capability->list[i].stat_enum = meter_stats_cap[i];
            stats_capability->list[i].stat_modes = SAI_STATS_MODE_READ;
        }
        return SAI_STATUS_SUCCESS;
    };

    sai->mock_get = [] (sai_object_type_t objectType, sai_object_id_t objectId, uint32_t attr_count, sai_attribute_t *attr_list)
    {
        for (uint32_t i = 0; i < attr_count; i++)
        {
            if (attr_list[i].id == SAI_SWITCH_ATTR_DASH_CAPS_MAX_METER_BUCKET_COUNT_PER_ENI)
            {
                attr_list[i].value.u32 = 0;
            }
        }
        return SAI_STATUS_SUCCESS;
    };

    auto counterVerifyFunc = [] (swss::Table &countersTable, sai_object_id_t eni_id, const std::vector<std::string>& counterIdNames, const dash_meter_expected_val_t& expectedValues)
    {
    };
    dash_meter_expected_val_t expectedValues;

    testDashMeterAddRemoveCounter(
        DASH_METER_COUNTER_ID_LIST,
        {"SAI_METER_BUCKET_ENTRY_STAT_OUTBOUND_BYTES", "SAI_METER_BUCKET_ENTRY_STAT_INBOUND_BYTES"},
        expectedValues,
        counterVerifyFunc,
        false);
}

class FlexCounterTcpFallback : public ::testing::Test
{
protected:
    static constexpr const char *configPath = "/tmp/test_tcp_fallback_db_config.json";

    void SetUp() override
    {
        const std::string configContent = R"({
            "INSTANCES": {
                "redis": {
                    "hostname": "127.0.0.1",
                    "port": 6379,
                    "unix_socket_path": ""
                }
            },
            "DATABASES": {
                "COUNTERS_DB": {
                    "id": 2,
                    "separator": ":",
                    "instance": "redis"
                }
            },
            "VERSION": "1.0"
        })";

        std::ofstream ofs(configPath);
        ofs << configContent;
        ofs.close();

        swss::SonicDBConfig::reset();
        swss::SonicDBConfig::initialize(configPath);
    }

    void TearDown() override
    {
        std::remove(configPath);
        swss::SonicDBConfig::reset();
        swss::SonicDBConfig::initialize();
    }
};

TEST_F(FlexCounterTcpFallback, tcpFallbackWhenNoUnixSocket)
{
    EXPECT_TRUE(swss::SonicDBConfig::getDbSock("COUNTERS_DB").empty());

    sai->mock_getStatsExt = [](sai_object_type_t, sai_object_id_t, uint32_t number_of_counters, const sai_stat_id_t *, sai_stats_mode_t, uint64_t *counters) {
        for (uint32_t i = 0; i < number_of_counters; i++)
        {
            counters[i] = (i + 1) * 100;
        }
        return SAI_STATUS_SUCCESS;
    };
    sai->mock_getStats = [](sai_object_type_t, sai_object_id_t, uint32_t number_of_counters, const sai_stat_id_t *, uint64_t *counters) {
        for (uint32_t i = 0; i < number_of_counters; i++)
        {
            counters[i] = (i + 1) * 100;
        }
        return SAI_STATUS_SUCCESS;
    };
    sai->mock_queryStatsCapability = [](sai_object_id_t, sai_object_type_t, sai_stat_capability_list_t *) {
        return SAI_STATUS_FAILURE;
    };
    sai->mock_bulkGetStats = [](sai_object_id_t, sai_object_type_t, uint32_t, const sai_object_key_t *, uint32_t, const sai_stat_id_t *, sai_stats_mode_t, sai_status_t *, uint64_t *) {
        return SAI_STATUS_FAILURE;
    };

    // FlexCounter should detect empty socket path and use TCP
    FlexCounter fc("test_tcp", sai, "COUNTERS_DB");

    test_syncd::mockVidManagerObjectTypeQuery(SAI_OBJECT_TYPE_PORT);

    std::vector<swss::FieldValueTuple> values;
    values.emplace_back(POLL_INTERVAL_FIELD, "1000");
    values.emplace_back(FLEX_COUNTER_STATUS_FIELD, "enable");
    values.emplace_back(STATS_MODE_FIELD, STATS_MODE_READ);
    fc.addCounterPlugin(values);

    values.clear();
    values.emplace_back(PORT_COUNTER_ID_LIST, "SAI_PORT_STAT_IF_IN_OCTETS,SAI_PORT_STAT_IF_IN_UCAST_PKTS");

    auto object_ids = generateOids(1, SAI_OBJECT_TYPE_PORT);
    fc.addCounter(object_ids[0], object_ids[0], values);
    EXPECT_FALSE(fc.isEmpty());

    // Use TCP to connect and verify counters were written
    swss::DBConnector db("COUNTERS_DB", 0, true);
    swss::RedisPipeline pipeline(&db);
    swss::Table countersTable(&pipeline, COUNTERS_TABLE, false);

    waitForCounterKeys(countersTable, 1);

    std::string key = toOid(object_ids[0]);
    waitForCounterValues(countersTable, key,
        {"SAI_PORT_STAT_IF_IN_OCTETS", "SAI_PORT_STAT_IF_IN_UCAST_PKTS"},
        {"100", "200"});

    fc.removeCounter(object_ids[0]);
    EXPECT_TRUE(fc.isEmpty());

    countersTable.del(key);
}

TEST(FlexCounter, dynamicCounterGroups)
{
    ScopedPerPortCounterDiscovery enablePerPortCounterDiscovery(true);

    // This test tests counter group functionality. It ensures each interface only polls the counters they support.

    // All 6 counters are requested for every port, but getStats fails for
    // unsupported ones, so each port's counter group only contains its subset.
    // Port 0: IN_OCTETS, OUT_OCTETS, IN_ERRORS                            (3 of 6)
    // Port 1: IN_UCAST_PKTS, OUT_UCAST_PKTS, OUT_ERRORS                   (3 of 6)
    // Port 2: IN_OCTETS, IN_UCAST_PKTS, IN_ERRORS, OUT_ERRORS             (4 of 6, overlaps both)
    // Port 3: IN_OCTETS, OUT_OCTETS, IN_UCAST_PKTS, IN_ERRORS, OUT_ERRORS (5 of 6, super-set of Port 2)
    //
    // Port 3 matches Port 2's existing group, but supports an extra counter
    // (OUT_OCTETS), so a new larger group must be created rather than reusing
    // Port 2's group.
    //
    // Unsupported counters must not appear in Redis for any port.

    std::vector<std::string> allCounterNames = {
        "SAI_PORT_STAT_IF_IN_OCTETS",
        "SAI_PORT_STAT_IF_OUT_OCTETS",
        "SAI_PORT_STAT_IF_IN_UCAST_PKTS",
        "SAI_PORT_STAT_IF_OUT_UCAST_PKTS",
        "SAI_PORT_STAT_IF_IN_ERRORS",
        "SAI_PORT_STAT_IF_OUT_ERRORS"
    };

    test_syncd::mockVidManagerObjectTypeQuery(SAI_OBJECT_TYPE_PORT);
    auto oids = generateOids(4, SAI_OBJECT_TYPE_PORT);
    ASSERT_EQ(oids.size(), 4u);

    // Per-RID supported counter sets (keyed by object_id since RID == VID in tests)
    std::map<sai_object_id_t, std::set<sai_port_stat_t>> supportedMap;
    supportedMap[oids[0]] = {SAI_PORT_STAT_IF_IN_OCTETS, SAI_PORT_STAT_IF_OUT_OCTETS, SAI_PORT_STAT_IF_IN_ERRORS};
    supportedMap[oids[1]] = {SAI_PORT_STAT_IF_IN_UCAST_PKTS, SAI_PORT_STAT_IF_OUT_UCAST_PKTS, SAI_PORT_STAT_IF_OUT_ERRORS};
    supportedMap[oids[2]] = {SAI_PORT_STAT_IF_IN_OCTETS, SAI_PORT_STAT_IF_IN_UCAST_PKTS, SAI_PORT_STAT_IF_IN_ERRORS, SAI_PORT_STAT_IF_OUT_ERRORS};
    supportedMap[oids[3]] = {SAI_PORT_STAT_IF_IN_OCTETS, SAI_PORT_STAT_IF_OUT_OCTETS, SAI_PORT_STAT_IF_IN_UCAST_PKTS, SAI_PORT_STAT_IF_IN_ERRORS, SAI_PORT_STAT_IF_OUT_ERRORS};

    // Deterministic counter values: value = (port_index + 1) * 1000 + stat_enum
    auto computeValue = [&](sai_object_id_t rid, sai_port_stat_t stat) -> uint64_t {
        int portIdx = 0;
        for (size_t i = 0; i < oids.size(); i++)
        {
            if (oids[i] == rid) { portIdx = static_cast<int>(i); break; }
        }
        return static_cast<uint64_t>((portIdx + 1) * 1000 + static_cast<int>(stat));
    };

    auto isAllSupported = [&](sai_object_id_t rid, uint32_t count, const sai_stat_id_t *ids) -> bool {
        auto it = supportedMap.find(rid);
        if (it == supportedMap.end()) return false;
        for (uint32_t i = 0; i < count; i++)
        {
            if (it->second.count(static_cast<sai_port_stat_t>(ids[i])) == 0)
                return false;
        }
        return true;
    };

    // Succeed only when all requested counters are in the RID's supported set.
    sai->mock_getStats = [&](sai_object_type_t, sai_object_id_t rid,
                             uint32_t count, const sai_stat_id_t *ids,
                             uint64_t *counters) -> sai_status_t
    {
        if (!isAllSupported(rid, count, ids))
            return SAI_STATUS_FAILURE;
        for (uint32_t i = 0; i < count; i++)
            counters[i] = computeValue(rid, static_cast<sai_port_stat_t>(ids[i]));
        return SAI_STATUS_SUCCESS;
    };

    // Same per-RID logic; PORT uses getStats, but mock this as a safety net.
    sai->mock_getStatsExt = [&](sai_object_type_t, sai_object_id_t rid,
                                uint32_t count, const sai_stat_id_t *ids,
                                sai_stats_mode_t, uint64_t *counters) -> sai_status_t
    {
        if (!isAllSupported(rid, count, ids))
            return SAI_STATUS_FAILURE;
        for (uint32_t i = 0; i < count; i++)
            counters[i] = computeValue(rid, static_cast<sai_port_stat_t>(ids[i]));
        return SAI_STATUS_SUCCESS;
    };

    // Skip HW capability query; counter support is determined by getStats probing.
    sai->mock_queryStatsCapability = [](sai_object_id_t, sai_object_type_t,
                                        sai_stat_capability_list_t *)
    {
        return SAI_STATUS_FAILURE;
    };

    // Force non-bulk path so per-port counter groups are polled individually.
    // (per-port counter group discovery cannot be validated by the bulk-path,
    // it has different counter discovery logic.)
    sai->mock_bulkGetStats = [](sai_object_id_t, sai_object_type_t, uint32_t,
                                const sai_object_key_t *, uint32_t,
                                const sai_stat_id_t *, sai_stats_mode_t,
                                sai_status_t *, uint64_t *)
    {
        return SAI_STATUS_FAILURE;
    };

    FlexCounter fc("test", sai, "COUNTERS_DB");

    std::vector<swss::FieldValueTuple> pluginValues;
    pluginValues.emplace_back(POLL_INTERVAL_FIELD, "1000");
    pluginValues.emplace_back(FLEX_COUNTER_STATUS_FIELD, "enable");
    pluginValues.emplace_back(STATS_MODE_FIELD, STATS_MODE_READ);
    fc.addCounterPlugin(pluginValues);

    std::vector<swss::FieldValueTuple> counterValues;
    counterValues.emplace_back(PORT_COUNTER_ID_LIST, join(allCounterNames));

    fc.bulkAddCounter(SAI_OBJECT_TYPE_PORT, oids, oids, counterValues);

    EXPECT_FALSE(fc.isEmpty());

    swss::DBConnector db("COUNTERS_DB", 0);
    swss::RedisPipeline pipeline(&db);
    swss::Table countersTable(&pipeline, COUNTERS_TABLE, false);

    waitForCounterKeys(countersTable, 4);

    // Verify each port has exactly its supported counters with correct values,
    // and unsupported counters are absent.
    std::set<sai_port_stat_t> allStats = {
        SAI_PORT_STAT_IF_IN_OCTETS,
        SAI_PORT_STAT_IF_OUT_OCTETS,
        SAI_PORT_STAT_IF_IN_UCAST_PKTS,
        SAI_PORT_STAT_IF_OUT_UCAST_PKTS,
        SAI_PORT_STAT_IF_IN_ERRORS,
        SAI_PORT_STAT_IF_OUT_ERRORS
    };

    for (size_t p = 0; p < oids.size(); p++)
    {
        std::string key = toOid(oids[p]);
        const auto &supported = supportedMap[oids[p]];

        // Wait for one of the supported counters to be populated
        sai_port_stat_t firstSupported = *supported.begin();
        std::string firstField = sai_serialize_port_stat(firstSupported);
        std::string expectedFirstVal = std::to_string(computeValue(oids[p], firstSupported));
        waitForCounterValues(countersTable, key, {firstField}, {expectedFirstVal});

        // Verify all supported counters have correct values
        for (auto stat : supported)
        {
            std::string field = sai_serialize_port_stat(stat);
            std::string value;
            ASSERT_TRUE(countersTable.hget(key, field, value))
                << "Port " << p << " missing supported counter " << field;
            std::string expected = std::to_string(computeValue(oids[p], stat));
            EXPECT_EQ(value, expected)
                << "Port " << p << " counter " << field << " value mismatch";
        }

        // Verify unsupported counters are absent
        for (auto stat : allStats)
        {
            if (supported.count(stat))
                continue;
            std::string field = sai_serialize_port_stat(stat);
            std::string value;
            EXPECT_FALSE(countersTable.hget(key, field, value))
                << "Port " << p << " should NOT have unsupported counter " << field
                << " but found value '" << value << "'";
        }
    }

    // Cleanup
    for (auto oid : oids)
    {
        fc.removeCounter(oid);
        countersTable.del(toOid(oid));
    }
    EXPECT_TRUE(fc.isEmpty());

    std::vector<std::string> keys;
    countersTable.getKeys(keys);
    removeTimeStamp(keys, countersTable);
    ASSERT_TRUE(keys.empty());
}

TEST(FlexCounter, dynamicCounterGroupsBulkPath)
{
    ScopedPerPortCounterDiscovery enablePerPortCounterDiscovery(true);

    // Bulk-path variant of dynamicCounterGroups. Uses
    // bulkAddObjectWithCounterGroups, which selects the largest counter group
    // for bulkGetStats and falls back to single-object polling for ports whose
    // supported set is smaller.

    // All 6 counters are requested for every port, but getStats fails for
    // unsupported ones, so each port's counter group only contains its subset.
    // Port 0: IN_OCTETS, OUT_OCTETS, IN_ERRORS                            (3 of 6)
    // Port 1: IN_UCAST_PKTS, OUT_UCAST_PKTS, OUT_ERRORS                   (3 of 6)
    // Port 2: IN_OCTETS, IN_UCAST_PKTS, IN_ERRORS, OUT_ERRORS             (4 of 6, overlaps both)
    // Port 3: IN_OCTETS, OUT_OCTETS, IN_UCAST_PKTS, IN_ERRORS, OUT_ERRORS (5 of 6, super-set of Port 2)
    //
    // Port 3 matches Port 2's existing group, but supports an extra counter
    // (OUT_OCTETS), so a new larger group must be created rather than reusing
    // Port 2's group.
    //
    // Unsupported counters must not appear in Redis for any port.

    std::vector<std::string> allCounterNames = {
        "SAI_PORT_STAT_IF_IN_OCTETS",
        "SAI_PORT_STAT_IF_OUT_OCTETS",
        "SAI_PORT_STAT_IF_IN_UCAST_PKTS",
        "SAI_PORT_STAT_IF_OUT_UCAST_PKTS",
        "SAI_PORT_STAT_IF_IN_ERRORS",
        "SAI_PORT_STAT_IF_OUT_ERRORS"
    };

    test_syncd::mockVidManagerObjectTypeQuery(SAI_OBJECT_TYPE_PORT);
    auto oids = generateOids(4, SAI_OBJECT_TYPE_PORT);
    ASSERT_EQ(oids.size(), 4u);

    // Per-RID supported counter sets (keyed by object_id since RID == VID in tests)
    std::map<sai_object_id_t, std::set<sai_port_stat_t>> supportedMap;
    supportedMap[oids[0]] = {SAI_PORT_STAT_IF_IN_OCTETS, SAI_PORT_STAT_IF_OUT_OCTETS, SAI_PORT_STAT_IF_IN_ERRORS};
    supportedMap[oids[1]] = {SAI_PORT_STAT_IF_IN_UCAST_PKTS, SAI_PORT_STAT_IF_OUT_UCAST_PKTS, SAI_PORT_STAT_IF_OUT_ERRORS};
    supportedMap[oids[2]] = {SAI_PORT_STAT_IF_IN_OCTETS, SAI_PORT_STAT_IF_IN_UCAST_PKTS, SAI_PORT_STAT_IF_IN_ERRORS, SAI_PORT_STAT_IF_OUT_ERRORS};
    supportedMap[oids[3]] = {SAI_PORT_STAT_IF_IN_OCTETS, SAI_PORT_STAT_IF_OUT_OCTETS, SAI_PORT_STAT_IF_IN_UCAST_PKTS, SAI_PORT_STAT_IF_IN_ERRORS, SAI_PORT_STAT_IF_OUT_ERRORS};

    // Deterministic counter values: value = (port_index + 1) * 1000 + stat_enum
    auto computeValue = [&](sai_object_id_t rid, sai_port_stat_t stat) -> uint64_t {
        int portIdx = 0;
        for (size_t i = 0; i < oids.size(); i++)
        {
            if (oids[i] == rid) { portIdx = static_cast<int>(i); break; }
        }
        return static_cast<uint64_t>((portIdx + 1) * 1000 + static_cast<int>(stat));
    };

    auto isAllSupported = [&](sai_object_id_t rid, uint32_t count, const sai_stat_id_t *ids) -> bool {
        auto it = supportedMap.find(rid);
        if (it == supportedMap.end()) return false;
        for (uint32_t i = 0; i < count; i++)
        {
            if (it->second.count(static_cast<sai_port_stat_t>(ids[i])) == 0)
                return false;
        }
        return true;
    };

    // Succeed only when all requested counters are in the RID's supported set.
    sai->mock_getStats = [&](sai_object_type_t, sai_object_id_t rid,
                             uint32_t count, const sai_stat_id_t *ids,
                             uint64_t *counters) -> sai_status_t
    {
        if (!isAllSupported(rid, count, ids))
            return SAI_STATUS_FAILURE;
        for (uint32_t i = 0; i < count; i++)
            counters[i] = computeValue(rid, static_cast<sai_port_stat_t>(ids[i]));
        return SAI_STATUS_SUCCESS;
    };

    // Same per-RID logic; PORT uses getStats, but mock this as a safety net.
    sai->mock_getStatsExt = [&](sai_object_type_t, sai_object_id_t rid,
                                uint32_t count, const sai_stat_id_t *ids,
                                sai_stats_mode_t, uint64_t *counters) -> sai_status_t
    {
        if (!isAllSupported(rid, count, ids))
            return SAI_STATUS_FAILURE;
        for (uint32_t i = 0; i < count; i++)
            counters[i] = computeValue(rid, static_cast<sai_port_stat_t>(ids[i]));
        return SAI_STATUS_SUCCESS;
    };

    // Skip HW capability query; counter support is determined by getStats probing.
    sai->mock_queryStatsCapability = [](sai_object_id_t, sai_object_type_t,
                                        sai_stat_capability_list_t *)
    {
        return SAI_STATUS_FAILURE;
    };

    // Bulk path: succeed for any counter set so that the largest counter group
    // is polled via bulkGetStats.  Return the same deterministic values as the
    // single-object path so verification is identical.
    sai->mock_bulkGetStats = [&](sai_object_id_t,
                                 sai_object_type_t,
                                 uint32_t object_count,
                                 const sai_object_key_t *object_keys,
                                 uint32_t number_of_counters,
                                 const sai_stat_id_t *counter_ids,
                                 sai_stats_mode_t,
                                 sai_status_t *object_statuses,
                                 uint64_t *counters) -> sai_status_t
    {
        for (uint32_t i = 0; i < object_count; i++)
        {
            sai_object_id_t rid = object_keys[i].key.object_id;
            object_statuses[i] = SAI_STATUS_SUCCESS;
            for (uint32_t j = 0; j < number_of_counters; j++)
            {
                counters[i * number_of_counters + j] =
                    computeValue(rid, static_cast<sai_port_stat_t>(counter_ids[j]));
            }
        }
        return SAI_STATUS_SUCCESS;
    };

    FlexCounter fc("test", sai, "COUNTERS_DB");

    std::vector<swss::FieldValueTuple> pluginValues;
    pluginValues.emplace_back(POLL_INTERVAL_FIELD, "1000");
    pluginValues.emplace_back(FLEX_COUNTER_STATUS_FIELD, "enable");
    pluginValues.emplace_back(STATS_MODE_FIELD, STATS_MODE_READ);
    fc.addCounterPlugin(pluginValues);

    std::vector<swss::FieldValueTuple> counterValues;
    counterValues.emplace_back(PORT_COUNTER_ID_LIST, join(allCounterNames));

    fc.bulkAddCounter(SAI_OBJECT_TYPE_PORT, oids, oids, counterValues);

    EXPECT_FALSE(fc.isEmpty());

    swss::DBConnector db("COUNTERS_DB", 0);
    swss::RedisPipeline pipeline(&db);
    swss::Table countersTable(&pipeline, COUNTERS_TABLE, false);

    waitForCounterKeys(countersTable, 4);

    // Verify each port has exactly its supported counters with correct values,
    // and unsupported counters are absent.
    std::set<sai_port_stat_t> allStats = {
        SAI_PORT_STAT_IF_IN_OCTETS,
        SAI_PORT_STAT_IF_OUT_OCTETS,
        SAI_PORT_STAT_IF_IN_UCAST_PKTS,
        SAI_PORT_STAT_IF_OUT_UCAST_PKTS,
        SAI_PORT_STAT_IF_IN_ERRORS,
        SAI_PORT_STAT_IF_OUT_ERRORS
    };

    for (size_t p = 0; p < oids.size(); p++)
    {
        std::string key = toOid(oids[p]);
        const auto &supported = supportedMap[oids[p]];

        // Wait for one of the supported counters to be populated
        sai_port_stat_t firstSupported = *supported.begin();
        std::string firstField = sai_serialize_port_stat(firstSupported);
        std::string expectedFirstVal = std::to_string(computeValue(oids[p], firstSupported));
        waitForCounterValues(countersTable, key, {firstField}, {expectedFirstVal});

        // Verify all supported counters have correct values
        for (auto stat : supported)
        {
            std::string field = sai_serialize_port_stat(stat);
            std::string value;
            ASSERT_TRUE(countersTable.hget(key, field, value))
                << "Port " << p << " missing supported counter " << field;
            std::string expected = std::to_string(computeValue(oids[p], stat));
            EXPECT_EQ(value, expected)
                << "Port " << p << " counter " << field << " value mismatch";
        }

        // Verify unsupported counters are absent
        for (auto stat : allStats)
        {
            if (supported.count(stat))
                continue;
            std::string field = sai_serialize_port_stat(stat);
            std::string value;
            EXPECT_FALSE(countersTable.hget(key, field, value))
                << "Port " << p << " should NOT have unsupported counter " << field
                << " but found value '" << value << "'";
        }
    }

    // Cleanup
    for (auto oid : oids)
    {
        fc.removeCounter(oid);
        countersTable.del(toOid(oid));
    }
    EXPECT_TRUE(fc.isEmpty());

    std::vector<std::string> keys;
    countersTable.getKeys(keys);
    removeTimeStamp(keys, countersTable);
    ASSERT_TRUE(keys.empty());
}

TEST(FlexCounter, failedPollsCountAndCleanUp)
{
    // This test verifies that:
    //
    // 1. When getStats starts failing after successful polls on an already-added object,
    //    counter DB values go stale (stop updating) and the poll continues.
    // 2. After removing and re-adding the same object, the failure count resets
    //    (m_failedPolls cleanup on remove) so it polls successfully again.

    std::atomic<bool> failGetStats{false};
    std::atomic<uint32_t> pollCycleCount{0};

    sai->mock_queryStatsCapability = [](sai_object_id_t, sai_object_type_t,
                                        sai_stat_capability_list_t *)
    {
        return SAI_STATUS_FAILURE;
    };

    sai->mock_getStats = [&](sai_object_type_t, sai_object_id_t,
                             uint32_t number_of_counters, const sai_stat_id_t *,
                             uint64_t *counters) -> sai_status_t
    {
        if (failGetStats.load())
        {
            pollCycleCount++;
            return SAI_STATUS_FAILURE;
        }
        for (uint32_t i = 0; i < number_of_counters; i++)
        {
            counters[i] = (i + 1) * 100;
        }
        return SAI_STATUS_SUCCESS;
    };

    sai->mock_bulkGetStats = [](sai_object_id_t, sai_object_type_t, uint32_t,
                                const sai_object_key_t *, uint32_t,
                                const sai_stat_id_t *, sai_stats_mode_t,
                                sai_status_t *, uint64_t *)
    {
        return SAI_STATUS_FAILURE;
    };

    test_syncd::mockVidManagerObjectTypeQuery(SAI_OBJECT_TYPE_PORT);

    sai_object_id_t oid{0x1000000000000};
    std::string expectedKey = toOid(oid);

    FlexCounter fc("test", sai, "COUNTERS_DB");

    std::vector<swss::FieldValueTuple> pluginValues;
    pluginValues.emplace_back(POLL_INTERVAL_FIELD, "1000");
    pluginValues.emplace_back(FLEX_COUNTER_STATUS_FIELD, "enable");
    pluginValues.emplace_back(STATS_MODE_FIELD, STATS_MODE_READ);
    fc.addCounterPlugin(pluginValues);

    swss::DBConnector db("COUNTERS_DB", 0);
    swss::RedisPipeline pipeline(&db);
    swss::Table countersTable(&pipeline, COUNTERS_TABLE, false);

    // Verify counters will stop updating DB after 3 or more failed polls
    // Add object and verify counters in DB
    std::vector<swss::FieldValueTuple> counterValues;
    counterValues.emplace_back(PORT_COUNTER_ID_LIST, "SAI_PORT_STAT_IF_IN_OCTETS,SAI_PORT_STAT_IF_IN_ERRORS");
    fc.addCounter(oid, oid, counterValues);
    EXPECT_FALSE(fc.isEmpty());

    waitForCounterKeys(countersTable, 1);
    waitForCounterValues(countersTable, expectedKey,
                      {"SAI_PORT_STAT_IF_IN_OCTETS", "SAI_PORT_STAT_IF_IN_ERRORS"},
                      {"100", "200"});

    // getStats starts failing and DB values should go stale (not updated, not cleared).
    failGetStats = true;
    pollCycleCount = 0;

    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(5000);
    while (std::chrono::steady_clock::now() < deadline && pollCycleCount.load() < 3)
    {
        usleep(100 * 1000);
    }
    EXPECT_GE(pollCycleCount.load(), 3u);

    // Stale values should remain in the DB, unchanged
    std::string value;
    countersTable.hget(expectedKey, "SAI_PORT_STAT_IF_IN_OCTETS", value);
    EXPECT_EQ(value, "100");
    countersTable.hget(expectedKey, "SAI_PORT_STAT_IF_IN_ERRORS", value);
    EXPECT_EQ(value, "200");

    // Verify m_failedPolls is cleaned up on removeObject so the
    // re-added object can poll fresh without stale failing state.
    // Remove and re-add with getStats succeeding
    fc.removeCounter(oid);
    countersTable.del(expectedKey);
    EXPECT_TRUE(fc.isEmpty());

    failGetStats = false;

    fc.addCounter(oid, oid, counterValues);
    EXPECT_FALSE(fc.isEmpty());

    waitForCounterKeys(countersTable, 1);
    waitForCounterValues(countersTable, expectedKey,
                      {"SAI_PORT_STAT_IF_IN_OCTETS", "SAI_PORT_STAT_IF_IN_ERRORS"},
                      {"100", "200"});

    // Cleanup
    fc.removeCounter(oid);
    countersTable.del(expectedKey);
    EXPECT_TRUE(fc.isEmpty());

    std::vector<std::string> keys;
    countersTable.getKeys(keys);
    removeTimeStamp(keys, countersTable);
    ASSERT_TRUE(keys.empty());
}

namespace
{
    // A failure that deliberately does not derive from std::exception, standing
    // in for a vendor SAI implementation that throws a type of its own.
    struct VendorSpecificFailure
    {
        int code;
    };
}

TEST(FlexCounter, pollLoopSurvivesThrowAndRecovers)
{
    // Regression test for the syncd SIGABRT on a COUNTERS_DB failure.
    //
    // flexCounterThreadRunFunction() runs on a std::thread with no handler, so an
    // exception escaping the poll cycle called std::terminate() and took the whole
    // process down. A Redis protocol error ("Protocol error: expected '$', got ...")
    // raised from the swss layer mid-poll was enough to do it.
    //
    // The poll loop must instead absorb the failure, rebuild its COUNTERS_DB handles
    // and resume publishing. Note the shape of this test: without the guard it does
    // not fail, it *aborts the test binary* -- which is exactly the defect.
    //
    // The injected failures alternate between a std::exception and a type that
    // does not derive from it, so a single run covers both of the loop's
    // handlers -- the catch (...) one included.

    std::atomic<bool> throwFromGetStats{false};
    std::atomic<uint32_t> throwCount{0};
    std::atomic<uint64_t> counterBase{100};

    sai->mock_queryStatsCapability = [](sai_object_id_t, sai_object_type_t,
                                        sai_stat_capability_list_t *)
    {
        return SAI_STATUS_FAILURE;
    };

    sai->mock_bulkGetStats = [](sai_object_id_t, sai_object_type_t, uint32_t,
                                const sai_object_key_t *, uint32_t,
                                const sai_stat_id_t *, sai_stats_mode_t,
                                sai_status_t *, uint64_t *)
    {
        return SAI_STATUS_FAILURE;
    };

    sai->mock_getStats = [&](sai_object_type_t, sai_object_id_t,
                             uint32_t number_of_counters, const sai_stat_id_t *,
                             uint64_t *counters) -> sai_status_t
    {
        if (throwFromGetStats.load())
        {
            uint32_t failureNumber = throwCount++;

            if ((failureNumber % 2) == 0)
            {
                // Stand-in for the real failure. The swss Redis layer raises a
                // RedisError, which derives from std::runtime_error, so throwing
                // the base type here exercises the same catch path. The string
                // below is the protocol error text that the real exception message
                // embeds; it is not the whole message.
                throw std::runtime_error("Protocol error: expected '$', got 'N'");
            }

            // Nothing guarantees a failure arrives as a std::exception. Without the
            // catch (...) handler this one would reach the thread boundary and
            // terminate the process, so it must be covered too.
            throw VendorSpecificFailure{42};
        }

        for (uint32_t i = 0; i < number_of_counters; i++)
        {
            counters[i] = (i + 1) * counterBase.load();
        }

        return SAI_STATUS_SUCCESS;
    };

    test_syncd::mockVidManagerObjectTypeQuery(SAI_OBJECT_TYPE_PORT);

    sai_object_id_t oid{0x1000000000000};
    std::string expectedKey = toOid(oid);

    FlexCounter fc("test", sai, "COUNTERS_DB");

    std::vector<swss::FieldValueTuple> pluginValues;
    pluginValues.emplace_back(POLL_INTERVAL_FIELD, "1000");
    pluginValues.emplace_back(FLEX_COUNTER_STATUS_FIELD, "enable");
    pluginValues.emplace_back(STATS_MODE_FIELD, STATS_MODE_READ);
    fc.addCounterPlugin(pluginValues);

    swss::DBConnector db("COUNTERS_DB", 0);
    swss::RedisPipeline pipeline(&db);
    swss::Table countersTable(&pipeline, COUNTERS_TABLE, false);

    std::vector<swss::FieldValueTuple> counterValues;
    counterValues.emplace_back(PORT_COUNTER_ID_LIST, "SAI_PORT_STAT_IF_IN_OCTETS,SAI_PORT_STAT_IF_IN_ERRORS");
    fc.addCounter(oid, oid, counterValues);
    EXPECT_FALSE(fc.isEmpty());

    waitForCounterKeys(countersTable, 1);
    waitForCounterValues(countersTable, expectedKey,
                      {"SAI_PORT_STAT_IF_IN_OCTETS", "SAI_PORT_STAT_IF_IN_ERRORS"},
                      {"100", "200"});

    // Arm the throw. Every poll cycle from here raises out of collectCounters().
    throwFromGetStats = true;
    throwCount = 0;

    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(10000);
    while (std::chrono::steady_clock::now() < deadline && throwCount.load() < 3)
    {
        usleep(100 * 1000);
    }

    // Reaching this line at all is the primary assertion: the poll thread threw
    // repeatedly and the process is still running.
    EXPECT_GE(throwCount.load(), 3u);

    // The last good values must still be there -- a failed poll goes stale, it
    // does not clear the counters.
    std::string value;
    countersTable.hget(expectedKey, "SAI_PORT_STAT_IF_IN_OCTETS", value);
    EXPECT_EQ(value, "100");
    countersTable.hget(expectedKey, "SAI_PORT_STAT_IF_IN_ERRORS", value);
    EXPECT_EQ(value, "200");

    // Disarm and publish distinguishable values. Seeing them proves the loop kept
    // polling and rebuilt a usable COUNTERS_DB connection, rather than merely
    // swallowing the exception and going idle.
    counterBase = 300;
    throwFromGetStats = false;

    waitForCounterValues(countersTable, expectedKey,
                      {"SAI_PORT_STAT_IF_IN_OCTETS", "SAI_PORT_STAT_IF_IN_ERRORS"},
                      {"300", "600"}, 10000);

    // Cleanup
    fc.removeCounter(oid);
    countersTable.del(expectedKey);
    EXPECT_TRUE(fc.isEmpty());

    std::vector<std::string> keys;
    countersTable.getKeys(keys);
    removeTimeStamp(keys, countersTable);
    ASSERT_TRUE(keys.empty());
}

/*
 * Forces every COUNTERS_DB connect attempt to fail by pointing the database at
 * a unix socket that does not exist. ENOENT is deterministic, unlike an
 * unreachable TCP endpoint, which depends on whether the sandbox refuses the
 * connection or silently drops it.
 */
class FlexCounterCountersDbUnreachable : public ::testing::Test
{
protected:
    static constexpr const char *configPath = "/tmp/test_counters_db_unreachable_config.json";
    static constexpr const char *sockPath = "/tmp/test_counters_db_unreachable.sock";

    void SetUp() override
    {
        const std::string configContent = R"({
            "INSTANCES": {
                "redis": {
                    "hostname": "127.0.0.1",
                    "port": 6379,
                    "unix_socket_path": ")" + std::string(sockPath) + R"("
                }
            },
            "DATABASES": {
                "COUNTERS_DB": {
                    "id": 2,
                    "separator": ":",
                    "instance": "redis"
                }
            },
            "VERSION": "1.0"
        })";

        std::remove(sockPath);

        std::ofstream ofs(configPath);
        ofs << configContent;
        ofs.close();

        swss::SonicDBConfig::reset();
        swss::SonicDBConfig::initialize(configPath);
    }

    void TearDown() override
    {
        std::remove(configPath);
        swss::SonicDBConfig::reset();
        swss::SonicDBConfig::initialize();
    }
};

TEST_F(FlexCounterCountersDbUnreachable, pollLoopSurvivesCountersDbConnectFailure)
{
    // The poll loop drops its COUNTERS_DB handles after a failed cycle and
    // rebuilds them on the next pass, so the reconnect can fail in its own
    // right -- COUNTERS_DB is frequently still down when the retry lands. That
    // reconnect runs on the same std::thread as the poll, with no handler above
    // it, so an exception escaping it would call std::terminate() and abort
    // syncd exactly as the unguarded poll failure did.
    //
    // This test has the same shape as pollLoopSurvivesThrowAndRecovers: without
    // the guard it does not fail, it aborts the test binary.

    ASSERT_FALSE(swss::SonicDBConfig::getDbSock("COUNTERS_DB").empty())
        << "fixture must select unix socket mode, otherwise the connect would "
           "fall back to TCP and could succeed against a live redis";

    FlexCounter fc("test_connect_failure", sai, "COUNTERS_DB");

    // Shortens the reconnect backoff from FLEX_COUNTER_RECONNECT_BACKOFF_MS to
    // 100ms. setPollInterval() notifies the sleep condition variable, so the
    // wait already under way is cut short instead of running to completion.
    std::vector<swss::FieldValueTuple> pluginValues;
    pluginValues.emplace_back(POLL_INTERVAL_FIELD, "100");
    pluginValues.emplace_back(FLEX_COUNTER_STATUS_FIELD, "enable");
    pluginValues.emplace_back(STATS_MODE_FIELD, STATS_MODE_READ);
    fc.addCounterPlugin(pluginValues);

    // Nothing is published while COUNTERS_DB is unreachable, so there is no
    // event to poll for and the wait is a fixed one. That is sound here because
    // the property under test is that retries keep happening: overshooting only
    // adds retries, it cannot turn a pass into a failure.
    usleep(1000 * 1000);

    // Also asserts the reconnect path stays outside the counter mutex, as the
    // comment on the connect in flexCounterThreadRunFunction() promises: this
    // call takes that mutex, so it would block here for as long as COUNTERS_DB
    // stayed down if the backoff wait were ever moved inside it.
    EXPECT_TRUE(fc.isEmpty());
}

TEST(FlexCounter, malformedBulkChunkSizePerPrefix)
{
    // Malformed BULK_CHUNK_SIZE_PER_PREFIX values (missing "name:size") must not
    // crash; parse errors are caught and leave FlexCounter usable.

    FlexCounter fc("test", sai, "COUNTERS_DB");

    sai_object_id_t oid{0x1000000000000};
    std::vector<swss::FieldValueTuple> values;
    values.emplace_back(PORT_COUNTER_ID_LIST, "SAI_PORT_STAT_IF_IN_OCTETS");

    test_syncd::mockVidManagerObjectTypeQuery(SAI_OBJECT_TYPE_PORT);
    sai->mock_getStats = [](sai_object_type_t, sai_object_id_t, uint32_t number_of_counters, const sai_stat_id_t *, uint64_t *counters) {
        for (uint32_t i = 0; i < number_of_counters; i++)
        {
            counters[i] = 1;
        }
        return SAI_STATUS_SUCCESS;
    };
    sai->mock_bulkGetStats = [](sai_object_id_t, sai_object_type_t, uint32_t, const sai_object_key_t *, uint32_t, const sai_stat_id_t *, sai_stats_mode_t, sai_status_t *, uint64_t *) {
        return SAI_STATUS_FAILURE;
    };
    sai->mock_queryStatsCapability = [](sai_object_id_t, sai_object_type_t, sai_stat_capability_list_t *) {
        return SAI_STATUS_FAILURE;
    };

    fc.m_statsMode = SAI_STATS_MODE_READ;
    fc.addCounter(oid, oid, values);
    EXPECT_EQ(fc.isEmpty(), false);

    std::string malformedPrefixMaps[] = {
        "bad_token",
        "SAI_PORT_STAT_IF_OUT_QLEN",
        "SAI_PORT_STAT_IF_OUT_QLEN:",
        "SAI_PORT_STAT_IF_OUT_QLEN:not_a_number",
        "SAI_PORT_STAT_IF_OUT_QLEN:0;bad_token"};
    for (auto &bad : malformedPrefixMaps)
    {
        values.clear();
        values.emplace_back(BULK_CHUNK_SIZE_PER_PREFIX_FIELD, bad);
        fc.addCounterPlugin(values);
        EXPECT_EQ(fc.isEmpty(), false);
    }

    // Valid map after malformed input should still be accepted
    values.clear();
    values.emplace_back(BULK_CHUNK_SIZE_PER_PREFIX_FIELD, "SAI_PORT_STAT_IF_OUT_QLEN:0");
    fc.addCounterPlugin(values);

    fc.removeCounter(oid);
    EXPECT_EQ(fc.isEmpty(), true);
}

TEST(FlexCounter, queueAttrBulkGet)
{
    FlexCounter fc("test", sai, "COUNTERS_DB");

    sai_object_id_t counterVid = generateOids(1, SAI_OBJECT_TYPE_QUEUE).at(0);
    sai_object_id_t counterRid = counterVid;

    sai->mock_bulkGet = [](sai_object_type_t objectType,
                           uint32_t objectCount,
                           const sai_object_id_t *,
                           const uint32_t *attrCount,
                           sai_attribute_t **attrList,
                           sai_bulk_op_error_mode_t,
                           sai_status_t *objectStatuses) {
        EXPECT_EQ(objectType, SAI_OBJECT_TYPE_QUEUE);

        for (uint32_t i = 0; i < objectCount; i++)
        {
            for (uint32_t j = 0; j < attrCount[i]; j++)
            {
                if (attrList[i][j].id == SAI_QUEUE_ATTR_PAUSE_STATUS)
                {
                    attrList[i][j].value.booldata = true;
                }
            }

            objectStatuses[i] = SAI_STATUS_SUCCESS;
        }

        return SAI_STATUS_SUCCESS;
    };

    std::vector<swss::FieldValueTuple> values;
    values.emplace_back(QUEUE_ATTR_ID_LIST, "SAI_QUEUE_ATTR_PAUSE_STATUS");
    fc.addCounter(counterVid, counterRid, values);

    values.clear();
    values.emplace_back(POLL_INTERVAL_FIELD, "1000");
    values.emplace_back(FLEX_COUNTER_STATUS_FIELD, "enable");
    values.emplace_back(STATS_MODE_FIELD, STATS_MODE_READ);
    fc.addCounterPlugin(values);

    swss::DBConnector db("COUNTERS_DB", 0);
    swss::RedisPipeline pipeline(&db);
    swss::Table countersTable(&pipeline, COUNTERS_TABLE, false);

    waitForCounterKeys(countersTable, 1);

    std::string value;
    EXPECT_TRUE(countersTable.hget(toOid(counterVid), "SAI_QUEUE_ATTR_PAUSE_STATUS", value));
    EXPECT_EQ(value, "true");

    sai->mock_bulkGet = nullptr;
    fc.removeCounter(counterVid);
    countersTable.del(toOid(counterVid));
}

TEST(FlexCounter, queueAttrBulkGetFailureFallsBack)
{
    FlexCounter fc("test", sai, "COUNTERS_DB");

    sai_object_id_t counterVid = generateOids(1, SAI_OBJECT_TYPE_QUEUE).at(0);
    sai_object_id_t counterRid = counterVid;

    sai->mock_bulkGet = [](sai_object_type_t,
                           uint32_t,
                           const sai_object_id_t *,
                           const uint32_t *,
                           sai_attribute_t **,
                           sai_bulk_op_error_mode_t,
                           sai_status_t *) {
        return SAI_STATUS_FAILURE;
    };

    sai->mock_get = [](sai_object_type_t, sai_object_id_t, uint32_t attrCount, sai_attribute_t *attrList) {
        for (uint32_t i = 0; i < attrCount; i++)
        {
            if (attrList[i].id == SAI_QUEUE_ATTR_PAUSE_STATUS)
            {
                attrList[i].value.booldata = false;
            }
        }

        return SAI_STATUS_SUCCESS;
    };

    std::vector<swss::FieldValueTuple> values;
    values.emplace_back(QUEUE_ATTR_ID_LIST, "SAI_QUEUE_ATTR_PAUSE_STATUS");
    fc.addCounter(counterVid, counterRid, values);

    values.clear();
    values.emplace_back(POLL_INTERVAL_FIELD, "1000");
    values.emplace_back(FLEX_COUNTER_STATUS_FIELD, "enable");
    values.emplace_back(STATS_MODE_FIELD, STATS_MODE_READ);
    fc.addCounterPlugin(values);

    swss::DBConnector db("COUNTERS_DB", 0);
    swss::RedisPipeline pipeline(&db);
    swss::Table countersTable(&pipeline, COUNTERS_TABLE, false);

    waitForCounterKeys(countersTable, 1);

    std::string value;
    EXPECT_TRUE(countersTable.hget(toOid(counterVid), "SAI_QUEUE_ATTR_PAUSE_STATUS", value));
    EXPECT_EQ(value, "false");

    sai->mock_bulkGet = nullptr;
    fc.removeCounter(counterVid);
    countersTable.del(toOid(counterVid));
}

namespace
{
    // The first SDK call represents a read already in progress when a registry
    // update arrives. Every following call has a separate gate: a full-sweep
    // mutex would prevent the updater from finishing until that gate is opened.
    class FlexCounterSdkGate
    {
        public:
            void arm()
            {
                SWSS_LOG_ENTER();
                std::lock_guard<std::mutex> lock(m_mutex);
                m_armed = true;
            }

            void read(sai_object_id_t oid, uint32_t objectCount, bool updatePending)
            {
                SWSS_LOG_ENTER();
                std::unique_lock<std::mutex> lock(m_mutex);
                if (!m_armed)
                {
                    return;
                }
                if (m_firstEntered && std::this_thread::get_id() != m_pollThread)
                {
                    // Registration can probe statistics synchronously on the
                    // updater's thread; only polling-thread reads form a sweep.
                    return;
                }

                m_reads.push_back(oid);
                m_objectCounts.push_back(objectCount);
                m_condition.notify_all();
                if (m_reads.size() == 1)
                {
                    m_pollThread = std::this_thread::get_id();
                    m_firstEntered = true;
                    m_condition.notify_all();
                    m_condition.wait_for(lock, std::chrono::seconds(5),
                            [&] { return m_releaseFirst; });
                }
                else
                {
                    m_readWhileUpdatePending |= updatePending;
                    m_condition.wait_for(lock, std::chrono::seconds(5),
                            [&] { return m_releaseRest; });
                }
            }

            void clear(sai_object_id_t oid)
            {
                SWSS_LOG_ENTER();
                std::lock_guard<std::mutex> lock(m_mutex);
                if (m_armed && std::this_thread::get_id() == m_pollThread)
                {
                    m_clears.push_back(oid);
                }
            }

            bool waitForFirst()
            {
                SWSS_LOG_ENTER();
                std::unique_lock<std::mutex> lock(m_mutex);
                return m_condition.wait_for(lock, std::chrono::seconds(5),
                        [&] { return m_firstEntered; });
            }

            bool waitForReadCount(size_t count)
            {
                SWSS_LOG_ENTER();
                std::unique_lock<std::mutex> lock(m_mutex);
                return m_condition.wait_for(lock, std::chrono::seconds(5),
                        [&] { return m_reads.size() >= count; });
            }

            void releaseFirst()
            {
                SWSS_LOG_ENTER();
                std::lock_guard<std::mutex> lock(m_mutex);
                m_releaseFirst = true;
                m_condition.notify_all();
            }

            void releaseAll()
            {
                SWSS_LOG_ENTER();
                std::lock_guard<std::mutex> lock(m_mutex);
                m_releaseFirst = m_releaseRest = true;
                m_condition.notify_all();
            }

            std::vector<sai_object_id_t> reads()
            {
                SWSS_LOG_ENTER();
                std::lock_guard<std::mutex> lock(m_mutex);
                return m_reads;
            }

            std::vector<sai_object_id_t> clears()
            {
                SWSS_LOG_ENTER();
                std::lock_guard<std::mutex> lock(m_mutex);
                return m_clears;
            }

            std::vector<uint32_t> objectCounts()
            {
                SWSS_LOG_ENTER();
                std::lock_guard<std::mutex> lock(m_mutex);
                return m_objectCounts;
            }

            bool readWhileUpdatePending()
            {
                SWSS_LOG_ENTER();
                std::lock_guard<std::mutex> lock(m_mutex);
                return m_readWhileUpdatePending;
            }

            bool isPollingThread()
            {
                SWSS_LOG_ENTER();
                std::lock_guard<std::mutex> lock(m_mutex);
                return m_armed && m_firstEntered && std::this_thread::get_id() == m_pollThread;
            }

        private:
            std::mutex m_mutex;
            std::condition_variable m_condition;
            bool m_armed = false;
            bool m_firstEntered = false;
            bool m_releaseFirst = false;
            bool m_releaseRest = false;
            bool m_readWhileUpdatePending = false;
            std::thread::id m_pollThread;
            std::vector<sai_object_id_t> m_reads;
            std::vector<sai_object_id_t> m_clears;
            std::vector<uint32_t> m_objectCounts;
    };

    enum class FlexCounterBulkResult
    {
        SuccessWithoutObjectStatuses,
        FailureWithoutObjectStatuses,
        PartialFailure,
        PartialFailureWithUntouchedObject,
    };

    void testBulkResultContract(FlexCounterBulkResult result)
    {
        SWSS_LOG_ENTER();
        auto vendor = std::make_shared<MockableSaiInterface>();
        FlexCounterSdkGate gate;
        const auto objects = generateOids(3, SAI_OBJECT_TYPE_PORT);
        std::atomic<uint32_t> runtimeCalls{0};
        vendor->mock_queryStatsCapability = [](sai_object_id_t, sai_object_type_t,
                sai_stat_capability_list_t* capability)
        {
            if (capability->count < 1)
            {
                capability->count = 1;
                return SAI_STATUS_BUFFER_OVERFLOW;
            }
            capability->list[0].stat_enum = SAI_PORT_STAT_IF_IN_OCTETS;
            capability->list[0].stat_modes = SAI_STATS_MODE_READ | SAI_STATS_MODE_READ_AND_CLEAR |
                SAI_STATS_MODE_BULK_READ | SAI_STATS_MODE_BULK_READ_AND_CLEAR;
            return SAI_STATUS_SUCCESS;
        };
        vendor->mock_getStats = [](sai_object_type_t, sai_object_id_t,
                uint32_t count, const sai_stat_id_t*, uint64_t* values)
        {
            std::fill(values, values + count, 1);
            return SAI_STATUS_SUCCESS;
        };
        vendor->mock_bulkGetStats = [&](sai_object_id_t, sai_object_type_t,
                uint32_t count, const sai_object_key_t* keys, uint32_t statCount,
                const sai_stat_id_t*, sai_stats_mode_t mode,
                sai_status_t* statuses, uint64_t* values)
        {
            ++runtimeCalls;
            EXPECT_EQ(count, objects.size());
            EXPECT_EQ(statCount, 1u);
            EXPECT_EQ(mode, SAI_STATS_MODE_BULK_READ_AND_CLEAR);
            gate.read(keys[0].key.object_id, count, false);
            for (uint32_t i = 0; i < count; ++i)
            {
                const auto index = std::find(objects.begin(), objects.end(), keys[i].key.object_id) - objects.begin();
                values[i] = 111 * (index + 1);
                if (result == FlexCounterBulkResult::PartialFailure)
                {
                    statuses[i] = keys[i].key.object_id == objects[1] ? SAI_STATUS_FAILURE : SAI_STATUS_SUCCESS;
                }
                else if (result == FlexCounterBulkResult::PartialFailureWithUntouchedObject && index != 2)
                {
                    statuses[i] = index == 0 ? SAI_STATUS_SUCCESS : SAI_STATUS_FAILURE;
                }
                // SAI allows a successful overall result without touching the
                // per-object statuses. On overall failure untouched statuses
                // must not turn initialized or stale buffer contents into data.
            }
            return result == FlexCounterBulkResult::SuccessWithoutObjectStatuses ?
                SAI_STATUS_SUCCESS : SAI_STATUS_FAILURE;
        };
        test_syncd::mockVidManagerObjectTypeQuery(SAI_OBJECT_TYPE_PORT);
        FlexCounter fc("bulk_result_contract", vendor, "COUNTERS_DB", true);
        fc.addCounterPlugin({
                {POLL_INTERVAL_FIELD, "60000"},
                {STATS_MODE_FIELD, STATS_MODE_READ_AND_CLEAR},
                {BULK_CHUNK_SIZE_FIELD, "0"},
                {FLEX_COUNTER_STATUS_FIELD, "disable"},
        });
        const std::string name = "SAI_PORT_STAT_IF_IN_OCTETS";
        swss::DBConnector db("COUNTERS_DB", 0);
        swss::Table table(&db, COUNTERS_TABLE);
        for (size_t i = 0; i < objects.size(); ++i)
        {
            fc.addCounter(objects[i], objects[i], {{PORT_COUNTER_ID_LIST, name}});
            table.del(toOid(objects[i]));
            table.set(toOid(objects[i]), {{name, std::to_string(1001 + i)}});
        }
        gate.arm();
        fc.addCounterPlugin({{FLEX_COUNTER_STATUS_FIELD, "enable"}});
        if (!gate.waitForFirst())
        {
            gate.releaseAll();
            ADD_FAILURE() << "The runtime bulk call did not start";
            return;
        }
        gate.releaseAll();
        // Joining also verifies completed READ_AND_CLEAR values were flushed
        // before a shutdown checkpoint, rather than only left in a pipeline.
        fc.endFlexCounterThread();
        EXPECT_EQ(runtimeCalls.load(), 1u);
        for (size_t i = 0; i < objects.size(); ++i)
        {
            const bool published = result == FlexCounterBulkResult::SuccessWithoutObjectStatuses ||
                (result == FlexCounterBulkResult::PartialFailure && i != 1) ||
                (result == FlexCounterBulkResult::PartialFailureWithUntouchedObject && i == 0);
            std::string value;
            EXPECT_TRUE(table.hget(toOid(objects[i]), name, value));
            EXPECT_EQ(value, std::to_string(published ? 111 * (i + 1) : 1001 + i));
            fc.removeCounter(objects[i]);
            table.del(toOid(objects[i]));
        }
        table.del("TIME_STAMP");
    }

    enum class FlexCounterPendingUpdate
    {
        Remove,
        RemoveCurrent,
        RemoveMultiple,
        Add,
        Reenroll,
        RemoveAndReadd,
        AddHybrid,
        RefreshHybrid,
        FailedHybrid,
        FailedHybridScalar,
        FailedHybridBulk,
        Refresh,
        UpdateAttribute,
        SetStatsMode,
        Disable,
        Shutdown,
    };

    void testPendingUpdatePreemptsSweep(
            bool bulk,
            bool readAndClear,
            FlexCounterPendingUpdate update,
            bool unchunkedBulk = false)
    {
        SWSS_LOG_ENTER();
        // Use a private mock rather than modifying the global SAI callbacks:
        // each blocked worker belongs to this counter's lifetime only.
        auto vendor = std::make_shared<MockableSaiInterface>();
        FlexCounterSdkGate gate;
        FlexCounter* counter = nullptr;
        sai_object_id_t failedObject = SAI_NULL_OBJECT_ID;
        std::atomic<uint32_t> zeroCounterPolls{0};
        bool queueAttributeUpdate = update == FlexCounterPendingUpdate::UpdateAttribute;
        bool failedScalar = update == FlexCounterPendingUpdate::FailedHybrid ||
            update == FlexCounterPendingUpdate::FailedHybridScalar;
        bool failedBulk = update == FlexCounterPendingUpdate::FailedHybrid ||
            update == FlexCounterPendingUpdate::FailedHybridBulk;
        bool failedHybrid = failedScalar || failedBulk;
        bool hybrid = update == FlexCounterPendingUpdate::AddHybrid ||
            update == FlexCounterPendingUpdate::RefreshHybrid || failedHybrid;
        bool refresh = update == FlexCounterPendingUpdate::Refresh;
        bool deferredRefresh = refresh || queueAttributeUpdate || failedHybrid ||
            update == FlexCounterPendingUpdate::RefreshHybrid;
        auto objectType = queueAttributeUpdate ? SAI_OBJECT_TYPE_QUEUE : SAI_OBJECT_TYPE_PORT;
        auto statId = queueAttributeUpdate ? static_cast<sai_stat_id_t>(SAI_QUEUE_STAT_PACKETS) :
            static_cast<sai_stat_id_t>(SAI_PORT_STAT_IF_IN_OCTETS);
        std::string counterName = queueAttributeUpdate ? "SAI_QUEUE_STAT_PACKETS" : "SAI_PORT_STAT_IF_IN_OCTETS";

        vendor->mock_queryStatsCapability = [bulk, hybrid, refresh, statId](sai_object_id_t,
                sai_object_type_t, sai_stat_capability_list_t* capability)
        {
            uint32_t required = (hybrid || refresh) ? 2 : 1;
            if (capability->count < required)
            {
                capability->count = required;
                return SAI_STATUS_BUFFER_OVERFLOW;
            }
            capability->list[0].stat_enum = statId;
            capability->list[0].stat_modes = SAI_STATS_MODE_READ | SAI_STATS_MODE_READ_AND_CLEAR;
            if (bulk && !hybrid)
            {
                capability->list[0].stat_modes |= SAI_STATS_MODE_BULK_READ | SAI_STATS_MODE_BULK_READ_AND_CLEAR;
            }
            if (hybrid || refresh)
            {
                capability->list[1].stat_enum = SAI_PORT_STAT_IF_IN_UCAST_PKTS;
                capability->list[1].stat_modes = SAI_STATS_MODE_READ | SAI_STATS_MODE_READ_AND_CLEAR;
                if (bulk)
                {
                    capability->list[1].stat_modes |= SAI_STATS_MODE_BULK_READ | SAI_STATS_MODE_BULK_READ_AND_CLEAR;
                }
            }
            return SAI_STATUS_SUCCESS;
        };
        vendor->mock_getStats = [&](sai_object_type_t, sai_object_id_t oid,
                uint32_t count, const sai_stat_id_t*, uint64_t* counters)
        {
            gate.read(oid, 1, counter && counter->m_pendingUpdates.load() != 0);
            if (failedScalar && oid == failedObject && gate.isPollingThread())
            {
                return SAI_STATUS_FAILURE;
            }
            std::fill(counters, counters + count, 1234);
            return SAI_STATUS_SUCCESS;
        };
        vendor->mock_clearStats = [&](sai_object_type_t, sai_object_id_t oid,
                uint32_t, const sai_stat_id_t*)
        {
            gate.clear(oid);
            return SAI_STATUS_SUCCESS;
        };
        vendor->mock_get = [](sai_object_type_t, sai_object_id_t,
                uint32_t count, sai_attribute_t* attrs)
        {
            for (uint32_t i = 0; i < count; ++i)
            {
                attrs[i].value.booldata = true;
            }
            return SAI_STATUS_SUCCESS;
        };
        vendor->mock_bulkGetStats = [&](sai_object_id_t, sai_object_type_t,
                uint32_t count, const sai_object_key_t* objects,
                uint32_t statCount, const sai_stat_id_t* ids, sai_stats_mode_t mode,
                sai_status_t* statuses, uint64_t* counters)
        {
            // Prefix partitioning also creates an empty default partition.
            // Zero-counter SDK calls are no-ops, not hardware reads/clears.
            if (statCount == 0)
            {
                if (gate.isPollingThread())
                {
                    ++zeroCounterPolls;
                }
                std::fill(statuses, statuses + count, SAI_STATUS_SUCCESS);
                return SAI_STATUS_SUCCESS;
            }
            if (!bulk || (hybrid &&
                    std::find(ids, ids + statCount, SAI_PORT_STAT_IF_IN_OCTETS) != ids + statCount))
            {
                return SAI_STATUS_NOT_SUPPORTED;
            }
            gate.read(objects[0].key.object_id, count,
                    counter && counter->m_pendingUpdates.load() != 0);
            bool anyFailed = false;
            for (uint32_t i = 0; i < count; ++i)
            {
                statuses[i] = failedBulk && gate.isPollingThread() && objects[i].key.object_id == failedObject ?
                    SAI_STATUS_FAILURE : SAI_STATUS_SUCCESS;
                anyFailed |= statuses[i] != SAI_STATUS_SUCCESS;
                if (statuses[i] == SAI_STATUS_SUCCESS && mode == SAI_STATS_MODE_BULK_READ_AND_CLEAR)
                {
                    gate.clear(objects[i].key.object_id);
                }
            }
            std::fill(counters, counters + count * statCount, hybrid ? 5678 : 1234);
            // Per-object failures are meaningful only when overall status fails.
            return anyFailed ? SAI_STATUS_FAILURE : SAI_STATUS_SUCCESS;
        };

        test_syncd::mockVidManagerObjectTypeQuery(objectType);
        auto objects = generateOids(4, objectType);
        auto addedObject = objects.back();
        objects.pop_back();
        failedObject = objects.front();
        // Registration must not invoke a bulk capability SDK probe while the
        // updater is being timed. Runtime bulk polling is still gated below.
        FlexCounter fc("pending_update", vendor, "COUNTERS_DB", bulk && !hybrid);
        counter = &fc;
        fc.addCounterPlugin({
                {POLL_INTERVAL_FIELD, "60000"},
                {STATS_MODE_FIELD, readAndClear ? STATS_MODE_READ_AND_CLEAR : STATS_MODE_READ},
                {BULK_CHUNK_SIZE_FIELD, unchunkedBulk ? "0" : "1"},
                {FLEX_COUNTER_STATUS_FIELD, "disable"},
                // Chunk-size and prefix options apply to existing contexts.
                // Create the context through the normal API before setting
                // the prefixes; an empty SHA adds no actual plugin.
                {queueAttributeUpdate ? QUEUE_PLUGIN_FIELD : PORT_PLUGIN_FIELD, ""},
        });
        if (hybrid)
        {
            fc.addCounterPlugin({{BULK_CHUNK_SIZE_PER_PREFIX_FIELD,
                    "SAI_PORT_STAT_IF_IN_OCTETS:1;SAI_PORT_STAT_IF_IN_UCAST_PKTS:1"}});
        }
        std::vector<swss::FieldValueTuple> values = {
            {queueAttributeUpdate ? QUEUE_COUNTER_ID_LIST : PORT_COUNTER_ID_LIST,
                hybrid ? counterName + ",SAI_PORT_STAT_IF_IN_UCAST_PKTS" : counterName},
        };
        if (hybrid)
        {
            fc.bulkAddCounter(objectType, objects, objects, values);
        }
        else
        {
            for (auto oid : objects)
            {
                fc.addCounter(oid, oid, values);
                if (queueAttributeUpdate)
                {
                    fc.addCounter(oid, oid, {{QUEUE_ATTR_ID_LIST, "SAI_QUEUE_ATTR_PAUSE_STATUS"}});
                }
            }
        }

        const auto initialRegistrations = fc.m_objectRegistrations;
        swss::DBConnector db("COUNTERS_DB", 0);
        swss::Table countersTable(&db, COUNTERS_TABLE);
        for (auto oid : objects)
        {
            countersTable.del(toOid(oid));
        }
        countersTable.del(toOid(addedObject));
        if (update == FlexCounterPendingUpdate::RefreshHybrid)
        {
            // Model a previous sweep's bulk value. The current READ_AND_CLEAR
            // sweep must overwrite it before the accumulator can consume this
            // object's newly cleared scalar sample.
            countersTable.set(toOid(objects.front()), {{"SAI_PORT_STAT_IF_IN_UCAST_PKTS", "999"}});
        }
        if (failedHybrid)
        {
            countersTable.set(toOid(objects.front()), {
                    {counterName, "888"}, {"SAI_PORT_STAT_IF_IN_UCAST_PKTS", "999"}});
        }
        bool verifyPlugin = readAndClear && !unchunkedBulk &&
            (update == FlexCounterPendingUpdate::Remove ||
             update == FlexCounterPendingUpdate::RemoveCurrent ||
             update == FlexCounterPendingUpdate::Add ||
             update == FlexCounterPendingUpdate::Reenroll ||
             update == FlexCounterPendingUpdate::RemoveAndReadd ||
             update == FlexCounterPendingUpdate::UpdateAttribute || hybrid || refresh);
        std::string pluginValue = hybrid ? "6912" : "1234";
        const std::string pluginKey = "pending_update_plugin";
        if (verifyPlugin)
        {
            countersTable.del(pluginKey);
            // Hybrid contexts can pass a VID once per scalar/bulk subset.
            // Remove duplicate keys so this accumulator specifically checks fresh
            // deltas, rather than the pre-existing duplicate-key behavior.
            const std::string script =
                "redis.call('SELECT', ARGV[1]); "
                "local result = ARGV[2] .. ':pending_update_plugin'; "
                "local seen = {}; "
                "for _, key in ipairs(KEYS) do "
                " if not seen[key] then "
                "  seen[key] = true; "
                "  local value = tonumber(redis.call('HGET', ARGV[2] .. ':' .. key, '" + counterName + "') or '0'); " +
                (hybrid ? "  value = value + tonumber(redis.call('HGET', ARGV[2] .. ':' .. key, 'SAI_PORT_STAT_IF_IN_UCAST_PKTS') or '0'); " : "") +
                "  redis.call('HINCRBY', result, key, value); "
                " end; "
                "end; "
                "redis.call('HINCRBY', result, 'invocations', 1); "
                "return {}";
            fc.addCounterPlugin({{queueAttributeUpdate ? QUEUE_PLUGIN_FIELD : PORT_PLUGIN_FIELD,
                    swss::loadRedisScript(&db, script)}});
        }

        gate.arm();
        fc.addCounterPlugin({{FLEX_COUNTER_STATUS_FIELD, "enable"}});
        bool firstEntered = gate.waitForFirst();
        if (!firstEntered)
        {
            // Release every gate before reporting a failed precondition, so the
            // worker and FlexCounter destructor never depend on a test assertion.
            gate.releaseAll();
            ADD_FAILURE() << "The polling thread did not enter its first SDK read";
            return;
        }

        auto updater = std::async(std::launch::async, [&]
        {
            if (update == FlexCounterPendingUpdate::Remove ||
                    update == FlexCounterPendingUpdate::RemoveMultiple)
            {
                fc.removeCounter(objects.back());
            }
            else if (update == FlexCounterPendingUpdate::RemoveCurrent)
            {
                // Remove the object whose destructive read is in flight, not
                // merely an unread object later in the sweep.
                fc.removeCounter(objects.front());
            }
            else if (update == FlexCounterPendingUpdate::Add)
            {
                fc.addCounter(addedObject, addedObject, values);
            }
            else if (update == FlexCounterPendingUpdate::Reenroll)
            {
                fc.addCounter(objects.front(), addedObject, values);
            }
            else if (update == FlexCounterPendingUpdate::RemoveAndReadd)
            {
                // Keep the poll paused across two real registry operations so
                // this test deterministically exercises the same-VID/same-RID
                // ABA case, without depending on mutex waiter scheduling.
                struct PendingBatch
                {
                    explicit PendingBatch(FlexCounter& instance): m_counter(instance)
                    {
                        SWSS_LOG_ENTER();
                        ++m_counter.m_pendingUpdates;
                    }
                    ~PendingBatch()
                    {
                        SWSS_LOG_ENTER();
                        std::lock_guard<std::mutex> lock(m_counter.m_mtx);
                        --m_counter.m_pendingUpdates;
                        m_counter.m_updateCond.notify_all();
                    }
                    FlexCounter& m_counter;
                } batch(fc);
                fc.removeCounter(objects.front());
                fc.addCounter(objects.front(), objects.front(), values);
            }
            else if (hybrid)
            {
                auto oid = update == FlexCounterPendingUpdate::AddHybrid ? addedObject :
                    failedHybrid ? objects[1] : objects.front();
                fc.bulkAddCounter(objectType, {oid}, {oid}, values);
            }
            else if (refresh)
            {
                fc.addCounter(objects.front(), objects.front(), {{PORT_COUNTER_ID_LIST,
                        counterName + ",SAI_PORT_STAT_IF_IN_UCAST_PKTS"}});
            }
            else if (update == FlexCounterPendingUpdate::UpdateAttribute)
            {
                fc.addCounter(objects.front(), objects.front(),
                        {{QUEUE_ATTR_ID_LIST, "SAI_QUEUE_ATTR_PAUSE_STATUS"}});
            }
            else if (update == FlexCounterPendingUpdate::SetStatsMode)
            {
                fc.addCounterPlugin({{STATS_MODE_FIELD, STATS_MODE_READ}});
            }
            else if (update == FlexCounterPendingUpdate::Disable)
            {
                fc.addCounterPlugin({{FLEX_COUNTER_STATUS_FIELD, "disable"}});
            }
            else
            {
                fc.endFlexCounterThread();
            }
        });
        std::future<void> secondUpdater;
        if (update == FlexCounterPendingUpdate::RemoveMultiple || refresh)
        {
            secondUpdater = std::async(std::launch::async, [&]
            {
                if (refresh)
                {
                    fc.addCounter(objects.back(), objects.back(), {{PORT_COUNTER_ID_LIST,
                            counterName + ",SAI_PORT_STAT_IF_IN_UCAST_PKTS"}});
                }
                else
                {
                    fc.removeCounter(objects[1]);
                }
            });
        }

        uint32_t expectedPending = secondUpdater.valid() ||
            update == FlexCounterPendingUpdate::RemoveAndReadd ? 2 : 1;
        auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
        while (fc.m_pendingUpdates.load() < expectedPending && std::chrono::steady_clock::now() < deadline)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        bool updateQueued = fc.m_pendingUpdates.load() == expectedPending;
        EXPECT_TRUE(updateQueued);
        EXPECT_EQ(updater.wait_for(std::chrono::milliseconds(0)), std::future_status::timeout)
                << "An in-flight SDK read must remain protected from object removal";

        gate.releaseFirst();
        if (deferredRefresh)
        {
            // Existing-object refresh can run destructive capability probes.
            // Keep their original ordering after the complete sweep and plugin,
            // rather than applying them between two old-plan SDK reads.
            EXPECT_TRUE(gate.waitForReadCount(2));
            EXPECT_EQ(updater.wait_for(std::chrono::milliseconds(0)), std::future_status::timeout);
            if (secondUpdater.valid())
            {
                EXPECT_EQ(secondUpdater.wait_for(std::chrono::milliseconds(0)), std::future_status::timeout);
            }
            gate.releaseAll();
        }
        bool updateFinished = updater.wait_for(std::chrono::seconds(2)) == std::future_status::ready;
        if (secondUpdater.valid())
        {
            updateFinished &= secondUpdater.wait_for(std::chrono::seconds(2)) == std::future_status::ready;
        }
        if (!updateFinished)
        {
            // On the original implementation the second read keeps the group
            // lock, so the update cannot complete. Unblock that read before
            // waiting on the future or running cleanup.
            gate.releaseAll();
            if (update == FlexCounterPendingUpdate::Shutdown)
            {
                // Also release a buggy implementation that loses the shutdown
                // notification and enters its long post-poll sleep afterwards.
                auto cleanupDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
                while (updater.wait_for(std::chrono::milliseconds(10)) != std::future_status::ready &&
                        std::chrono::steady_clock::now() < cleanupDeadline)
                {
                    fc.m_cvSleep.notify_all();
                }
            }
        }
        updater.get();
        if (secondUpdater.valid())
        {
            secondUpdater.get();
        }
        EXPECT_TRUE(updateFinished) << "The updater waited for the rest of the polling sweep";
        EXPECT_FALSE(gate.readWhileUpdatePending())
                << "A new SDK read started before queued registry updates drained";

        // Registry writers have joined; the gated polling thread only reads
        // this map. Do not acquire its mutex while a later SDK read is blocked.
        const auto updatedRegistrations = fc.m_objectRegistrations;
        for (const auto& initial : initialRegistrations)
        {
            const auto current = updatedRegistrations.find(initial.first);
            const auto vid = initial.first.second;
            const bool removed =
                ((update == FlexCounterPendingUpdate::Remove ||
                  update == FlexCounterPendingUpdate::RemoveMultiple) && vid == objects.back()) ||
                (update == FlexCounterPendingUpdate::RemoveMultiple && vid == objects[1]) ||
                (update == FlexCounterPendingUpdate::RemoveCurrent && vid == objects.front());
            if (removed)
            {
                EXPECT_EQ(current, updatedRegistrations.end());
                continue;
            }
            EXPECT_NE(current, updatedRegistrations.end());
            if (current == updatedRegistrations.end())
            {
                continue;
            }
            if ((update == FlexCounterPendingUpdate::Reenroll ||
                 update == FlexCounterPendingUpdate::RemoveAndReadd) && vid == objects.front())
            {
                EXPECT_NE(current->second.epoch, initial.second.epoch)
                    << "Replacement or remove/re-add retained the old polling epoch";
                EXPECT_EQ(current->second.rid,
                        update == FlexCounterPendingUpdate::Reenroll ? addedObject : vid);
            }
            else
            {
                EXPECT_EQ(current->second.epoch, initial.second.epoch)
                    << "An unchanged or deferred same-RID registration changed its polling epoch";
                EXPECT_EQ(current->second.rid, initial.second.rid);
            }
        }

        // The completed sample must be flushed before yielding the lock,
        // including a READ_AND_CLEAR sample whose hardware value is now gone.
        std::string value;
        EXPECT_TRUE(countersTable.hget(toOid(objects.front()), counterName, value));
        EXPECT_EQ(value, failedScalar ? "888" : "1234");
        if (readAndClear)
        {
            auto clears = gate.clears();
            EXPECT_EQ(std::count(clears.begin(), clears.end(), objects.front()),
                    hybrid && deferredRefresh ? static_cast<int>(!failedScalar) + static_cast<int>(!failedBulk) : 1);
        }
        if (verifyPlugin && updateFinished)
        {
            // A checkpoint publishes completed deltas, but must not invoke a
            // full-group plugin using stale samples from the unread objects.
            if (deferredRefresh)
            {
                EXPECT_TRUE(countersTable.hget(pluginKey, "invocations", value));
                EXPECT_EQ(value, "1");
            }
            else
            {
                EXPECT_FALSE(countersTable.hget(pluginKey, "invocations", value));
            }
        }
        if (update == FlexCounterPendingUpdate::RefreshHybrid && updateFinished)
        {
            EXPECT_TRUE(countersTable.hget(toOid(objects.front()), "SAI_PORT_STAT_IF_IN_UCAST_PKTS", value));
            EXPECT_EQ(value, "5678") << "The original bulk subset must finish before existing-object refresh";
        }

        if (update == FlexCounterPendingUpdate::Shutdown)
        {
            EXPECT_EQ(gate.reads().size(), 1u);
            EXPECT_FALSE(fc.m_runFlexCounterThread);
        }

        gate.releaseAll();
        if (update == FlexCounterPendingUpdate::SetStatsMode)
        {
            waitForCounterValues(countersTable, toOid(objects.back()), {counterName}, {"1234"});
            auto clears = gate.clears();
            EXPECT_EQ(std::count(clears.begin(), clears.end(), objects[1]), 1)
                    << "Changing group mode mixed READ and READ_AND_CLEAR in one logical sweep";
            EXPECT_EQ(std::count(clears.begin(), clears.end(), objects.back()), 1);
        }
        if (hybrid)
        {
            waitForCounterValues(countersTable, toOid(objects[1]),
                    {counterName, "SAI_PORT_STAT_IF_IN_UCAST_PKTS"}, {"1234", "5678"});
            if (update == FlexCounterPendingUpdate::AddHybrid)
            {
                EXPECT_FALSE(countersTable.hget(toOid(addedObject), "SAI_PORT_STAT_IF_IN_UCAST_PKTS", value))
                        << "A new registration was partially sampled by the bulk phase of an old sweep";
            }
            else if (!failedHybrid)
            {
                waitForCounterValues(countersTable, toOid(objects.front()),
                        {counterName, "SAI_PORT_STAT_IF_IN_UCAST_PKTS"}, {"1234", "5678"});
            }
            auto reads = gate.reads();
            EXPECT_EQ(std::count(reads.begin(), reads.end(), objects.front()), 2);
            EXPECT_EQ(std::count(reads.begin(), reads.end(), addedObject), 0);
        }
        if (verifyPlugin)
        {
            // Resuming the same logical sweep must run the plugin exactly once
            // and preserve the first object's cleared sample until it is used.
            waitForCounterValues(countersTable, pluginKey,
                    {"invocations", toOid(objects[1])}, {"1", pluginValue});
            if (update == FlexCounterPendingUpdate::Reenroll ||
                    update == FlexCounterPendingUpdate::RemoveAndReadd ||
                    update == FlexCounterPendingUpdate::RemoveCurrent ||
                    (failedScalar && failedBulk))
            {
                EXPECT_FALSE(countersTable.hget(pluginKey, toOid(objects.front()), value))
                        << "An unsampled or re-enrolled VID exposed a stale cleared sample";
            }
            else
            {
                EXPECT_TRUE(countersTable.hget(pluginKey, toOid(objects.front()), value));
                // A yielded hybrid sweep still preserves baseline fragment-level
                // semantics: a successful fragment can admit the same object's
                // stale field from another failed fragment. This test records
                // that limitation, rather than promising whole-sample freshness.
                EXPECT_EQ(value, failedScalar ? "6566" : failedBulk ? "2233" : pluginValue);
            }
            if (update == FlexCounterPendingUpdate::Remove)
            {
                EXPECT_FALSE(countersTable.hget(pluginKey, toOid(objects.back()), value));
            }
            else
            {
                EXPECT_TRUE(countersTable.hget(pluginKey, toOid(objects.back()), value));
                EXPECT_EQ(value, pluginValue);
            }
            EXPECT_FALSE(countersTable.hget(pluginKey, toOid(addedObject), value))
                    << "A newly enrolled VID reached the plugin without a current sample";
            auto reads = gate.reads();
            auto clears = gate.clears();
            EXPECT_EQ(std::count(reads.begin(), reads.end(), objects.front()), hybrid ? 2 : 1)
                    << "Resuming restarted the sweep and overwrote its cleared first sample";
            EXPECT_EQ(std::count(clears.begin(), clears.end(), objects.front()),
                    hybrid ? static_cast<int>(!failedScalar) + static_cast<int>(!failedBulk) : 1);
            if (refresh)
            {
                EXPECT_EQ(std::count(reads.begin(), reads.end(), objects.back()), 1)
                        << "A same-RID refresh must complete the original counter plan";
                EXPECT_FALSE(countersTable.hget(toOid(objects.back()), "SAI_PORT_STAT_IF_IN_UCAST_PKTS", value))
                        << "A new counter list took effect in the middle of the original sweep";
                EXPECT_FALSE(countersTable.hget(toOid(objects.front()), "SAI_PORT_STAT_IF_IN_UCAST_PKTS", value));
            }
        }
        if (update != FlexCounterPendingUpdate::Shutdown)
        {
            auto unaffected = update == FlexCounterPendingUpdate::RemoveMultiple || unchunkedBulk ?
                objects.front() : objects[1];
            auto previousReads = gate.reads();
            auto previousReadCount = std::count(previousReads.begin(), previousReads.end(), unaffected);
            fc.addCounterPlugin({
                    {POLL_INTERVAL_FIELD, "10"},
                    {FLEX_COUNTER_STATUS_FIELD, "enable"},
            });
            // Prove the long sleep was interrupted and telemetry really resumed;
            // a pre-existing Redis value is not evidence of a subsequent poll.
            auto resumeDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
            while (std::chrono::steady_clock::now() < resumeDeadline)
            {
                auto reads = gate.reads();
                if (std::count(reads.begin(), reads.end(), unaffected) > previousReadCount)
                {
                    break;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            auto resumedReads = gate.reads();
            EXPECT_GT(std::count(resumedReads.begin(), resumedReads.end(), unaffected), previousReadCount);
            waitForCounterValues(countersTable, toOid(unaffected),
                    {counterName}, {"1234"});
            if (hybrid && !failedHybrid)
            {
                auto enrolled = update == FlexCounterPendingUpdate::AddHybrid ? addedObject : objects.front();
                waitForCounterValues(countersTable, toOid(enrolled),
                        {counterName, "SAI_PORT_STAT_IF_IN_UCAST_PKTS"}, {"1234", "5678"});
            }
            if (update == FlexCounterPendingUpdate::Reenroll ||
                    update == FlexCounterPendingUpdate::RemoveAndReadd)
            {
                waitForNonZeroCounterValue(countersTable, pluginKey, toOid(objects.front()));
            }
            if (refresh)
            {
                waitForCounterValues(countersTable, toOid(objects.front()),
                        {counterName, "SAI_PORT_STAT_IF_IN_UCAST_PKTS"}, {"1234", "1234"});
                waitForCounterValues(countersTable, toOid(objects.back()),
                        {counterName, "SAI_PORT_STAT_IF_IN_UCAST_PKTS"}, {"1234", "1234"});
            }
            if (update == FlexCounterPendingUpdate::SetStatsMode)
            {
                // The new mode applies when the next complete sweep begins.
                auto nextSweepDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
                while (std::chrono::steady_clock::now() < nextSweepDeadline)
                {
                    auto reads = gate.reads();
                    if (std::count(reads.begin(), reads.end(), objects.front()) >= 2)
                    {
                        break;
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
                auto reads = gate.reads();
                auto clears = gate.clears();
                EXPECT_GE(std::count(reads.begin(), reads.end(), objects.front()), 2);
                EXPECT_EQ(std::count(clears.begin(), clears.end(), objects.front()), 1);
            }
        }

        if (!unchunkedBulk && (update == FlexCounterPendingUpdate::Remove ||
                update == FlexCounterPendingUpdate::RemoveMultiple))
        {
            auto reads = gate.reads();
            EXPECT_EQ(std::count(reads.begin(), reads.end(), objects.back()), 0)
                    << "The stable sweep snapshot polled an object after its removal";
            if (update == FlexCounterPendingUpdate::RemoveMultiple)
            {
                EXPECT_EQ(std::count(reads.begin(), reads.end(), objects[1]), 0);
            }
        }
        if (bulk)
        {
            EXPECT_EQ(zeroCounterPolls.load(), 0u)
                    << "An empty bulk plan must not establish a successful current sample";
            auto counts = gate.objectCounts();
            if (unchunkedBulk)
            {
                ASSERT_FALSE(counts.empty());
                EXPECT_EQ(counts.front(), 3u);
            }
            for (auto count : counts)
            {
                if (unchunkedBulk)
                {
                    EXPECT_LE(count, 3u);
                }
                else
                {
                    EXPECT_EQ(count, 1u) << "Each bulk chunk must provide an update checkpoint";
                }
            }
        }

        fc.endFlexCounterThread();
        for (auto oid : objects)
        {
            fc.removeCounter(oid);
            countersTable.del(toOid(oid));
        }
        if (update == FlexCounterPendingUpdate::Add || update == FlexCounterPendingUpdate::AddHybrid)
        {
            fc.removeCounter(addedObject);
        }
        countersTable.del(toOid(addedObject));
        countersTable.del("TIME_STAMP");
        if (verifyPlugin)
        {
            countersTable.del(pluginKey);
        }
    }

    enum class FlexCounterRefreshEnd
    {
        Complete,
        Shutdown,
        PollException,
        UpdateException,
        RemovePlugins,
        AddPlugin,
    };

    enum class FlexCounterRejectedReplacement
    {
        None,
        Scalar,
        ThrowingScalar,
        PartialBulk,
        ThrowingPartialBulk,
    };

    void testDestructiveRefreshPreservesSamples(bool bulk, bool hybrid,
            bool removeDuringRefresh = false, FlexCounterRefreshEnd ending = FlexCounterRefreshEnd::Complete,
            bool mixedBatch = false, bool perPortDiscovery = false,
            FlexCounterRejectedReplacement replacement = FlexCounterRejectedReplacement::None)
    {
        SWSS_LOG_ENTER();
        auto vendor = std::make_shared<MockableSaiInterface>();
        FlexCounterSdkGate gate;
        FlexCounter* counter = nullptr;
        const bool port = bulk || perPortDiscovery;
        auto objectType = port ? SAI_OBJECT_TYPE_PORT : SAI_OBJECT_TYPE_QUEUE;
        auto statId = port ? static_cast<sai_stat_id_t>(SAI_PORT_STAT_IF_IN_OCTETS) :
            static_cast<sai_stat_id_t>(SAI_QUEUE_STAT_PACKETS);
        auto extraId = port ? static_cast<sai_stat_id_t>(SAI_PORT_STAT_IF_IN_UCAST_PKTS) :
            static_cast<sai_stat_id_t>(SAI_QUEUE_STAT_BYTES);
        const std::string counterName = port ? "SAI_PORT_STAT_IF_IN_OCTETS" : "SAI_QUEUE_STAT_PACKETS";
        const std::string extraName = port ? "SAI_PORT_STAT_IF_IN_UCAST_PKTS" : "SAI_QUEUE_STAT_BYTES";
        const bool rejectReplacement = replacement != FlexCounterRejectedReplacement::None;
        const bool partialReplacement = replacement == FlexCounterRejectedReplacement::PartialBulk ||
            replacement == FlexCounterRejectedReplacement::ThrowingPartialBulk;
        const bool throwsReplacement = replacement == FlexCounterRejectedReplacement::ThrowingScalar ||
            replacement == FlexCounterRejectedReplacement::ThrowingPartialBulk;
        const auto originalCount = removeDuringRefresh || mixedBatch ? 3u : 2u;
        auto objects = generateOids(originalCount + (rejectReplacement ? partialReplacement ? 2 : 1 : 0), objectType);
        const auto rejectedRid = rejectReplacement ? objects.back() : SAI_NULL_OBJECT_ID;
        if (rejectReplacement)
        {
            objects.pop_back();
        }
        const auto acceptedRid = partialReplacement ? objects.back() : SAI_NULL_OBJECT_ID;
        if (partialReplacement)
        {
            objects.pop_back();
        }
        const auto addedObject = mixedBatch ? objects.back() : SAI_NULL_OBJECT_ID;
        if (mixedBatch)
        {
            objects.pop_back();
        }
        const auto target = hybrid ? objects.front() : objects[1];
        const std::string pluginKey = "destructive_refresh_plugin";
        const std::string newPluginKey = "destructive_refresh_new_plugin";
        swss::DBConnector db("COUNTERS_DB", 0);
        swss::Table table(&db, COUNTERS_TABLE);
        std::mutex hardwareMutex;
        std::map<std::pair<sai_object_id_t, sai_stat_id_t>, uint64_t> hardware;
        std::atomic<bool> armed{false};
        std::atomic<uint32_t> destructiveProbes{0};
        std::atomic<bool> probeBeforePlugin{false};
        std::atomic<bool> pollThrew{false};
        std::atomic<bool> updateThrew{false};

        auto observeProbe = [&]
        {
            if (armed && !gate.isPollingThread())
            {
                ++destructiveProbes;
                std::string invocations;
                if (!table.hget(pluginKey, "invocations", invocations) || invocations != "1")
                {
                    probeBeforePlugin = true;
                }
            }
        };
        vendor->mock_queryStatsCapability = [=](sai_object_id_t, sai_object_type_t,
                sai_stat_capability_list_t* capability)
        {
            if (capability->count < 2)
            {
                capability->count = 2;
                return SAI_STATUS_BUFFER_OVERFLOW;
            }
            capability->list[0].stat_enum = statId;
            capability->list[1].stat_enum = extraId;
            for (uint32_t i = 0; i < 2; ++i)
            {
                capability->list[i].stat_modes = SAI_STATS_MODE_READ | SAI_STATS_MODE_READ_AND_CLEAR;
                if (bulk && (!hybrid || i == 1))
                {
                    capability->list[i].stat_modes |= SAI_STATS_MODE_BULK_READ | SAI_STATS_MODE_BULK_READ_AND_CLEAR;
                }
            }
            return SAI_STATUS_SUCCESS;
        };
        vendor->mock_getStats = [&](sai_object_type_t, sai_object_id_t oid,
                uint32_t count, const sai_stat_id_t* ids, uint64_t* values)
        {
            if (oid == rejectedRid)
            {
                if (throwsReplacement)
                {
                    throw std::runtime_error("Injected rejected replacement probe exception");
                }
                return SAI_STATUS_FAILURE;
            }
            gate.read(oid, 1, counter && counter->m_pendingUpdates.load() != 0);
            if (ending == FlexCounterRefreshEnd::UpdateException && armed && !gate.isPollingThread())
            {
                throw std::runtime_error("Injected registration probe failure");
            }
            if (ending == FlexCounterRefreshEnd::PollException && armed && gate.isPollingThread() &&
                    oid == objects[1] && !pollThrew.exchange(true))
            {
                throw std::runtime_error("Injected SDK polling failure");
            }
            std::lock_guard<std::mutex> lock(hardwareMutex);
            for (uint32_t i = 0; i < count; ++i)
            {
                values[i] = hardware[{oid, ids[i]}];
            }
            return SAI_STATUS_SUCCESS;
        };
        vendor->mock_clearStats = [&](sai_object_type_t, sai_object_id_t oid,
                uint32_t count, const sai_stat_id_t* ids)
        {
            observeProbe();
            gate.clear(oid);
            std::lock_guard<std::mutex> lock(hardwareMutex);
            for (uint32_t i = 0; i < count; ++i)
            {
                hardware[{oid, ids[i]}] = 0;
            }
            return SAI_STATUS_SUCCESS;
        };
        vendor->mock_bulkGetStats = [&](sai_object_id_t, sai_object_type_t,
                uint32_t count, const sai_object_key_t* keys, uint32_t statCount,
                const sai_stat_id_t* ids, sai_stats_mode_t mode,
                sai_status_t* statuses, uint64_t* values)
        {
            if (!bulk || (hybrid && std::find(ids, ids + statCount, statId) != ids + statCount))
            {
                return SAI_STATUS_NOT_SUPPORTED;
            }
            if (statCount == 0)
            {
                std::fill(statuses, statuses + count, SAI_STATUS_SUCCESS);
                return SAI_STATUS_SUCCESS;
            }
            gate.read(keys[0].key.object_id, count,
                    counter && counter->m_pendingUpdates.load() != 0);
            if (mode == SAI_STATS_MODE_BULK_READ_AND_CLEAR)
            {
                observeProbe();
            }
            std::lock_guard<std::mutex> lock(hardwareMutex);
            for (uint32_t i = 0; i < count; ++i)
            {
                statuses[i] = SAI_STATUS_SUCCESS;
                for (uint32_t j = 0; j < statCount; ++j)
                {
                    auto key = std::make_pair(keys[i].key.object_id, ids[j]);
                    values[i * statCount + j] = hardware[key];
                    if (mode == SAI_STATS_MODE_BULK_READ_AND_CLEAR)
                    {
                        hardware[key] = 0;
                    }
                }
            }
            return SAI_STATUS_SUCCESS;
        };

        if (perPortDiscovery)
        {
            // Real per-port discovery probes read and clear matching supported
            // groups while checking an existing physical port's counter list.
            auto options = std::make_shared<VendorSaiOptions>();
            options->m_enablePerPortCounterDiscovery = true;
            vendor->setOptions(VendorSaiOptions::OPTIONS_KEY, options);
        }
        test_syncd::mockVidManagerObjectTypeQuery(objectType);
        // Leave double-checking enabled: the updater's real destructive probes
        // are part of this regression, not excluded mock setup work.
        FlexCounter fc("destructive_refresh", vendor, "COUNTERS_DB");
        counter = &fc;
        fc.addCounterPlugin({
                {POLL_INTERVAL_FIELD, "60000"},
                {STATS_MODE_FIELD, STATS_MODE_READ_AND_CLEAR},
                {BULK_CHUNK_SIZE_FIELD, "1"},
                {FLEX_COUNTER_STATUS_FIELD, "disable"},
                {port ? PORT_PLUGIN_FIELD : QUEUE_PLUGIN_FIELD, ""},
        });
        if (hybrid)
        {
            fc.addCounterPlugin({{BULK_CHUNK_SIZE_PER_PREFIX_FIELD,
                    "SAI_PORT_STAT_IF_IN_OCTETS:1;SAI_PORT_STAT_IF_IN_UCAST_PKTS:1"}});
        }
        const auto field = port ? PORT_COUNTER_ID_LIST : QUEUE_COUNTER_ID_LIST;
        std::vector<swss::FieldValueTuple> original = {{field,
                hybrid ? counterName + "," + extraName : counterName}};
        if (hybrid)
        {
            fc.bulkAddCounter(objectType, objects, objects, original);
        }
        else
        {
            for (auto oid : objects)
            {
                fc.addCounter(oid, oid, original);
            }
        }
        const auto initialRegistrations = fc.m_objectRegistrations;
        if (rejectReplacement)
        {
            bool threw = false;
            try
            {
                if (partialReplacement)
                {
                    // Accept a new batch member while rejecting replacement of
                    // another member's existing physical object.
                    fc.bulkAddCounter(objectType,
                            {acceptedRid, throwsReplacement ? rejectedRid : target},
                            {acceptedRid, rejectedRid}, original);
                }
                else
                {
                    fc.addCounter(target, rejectedRid, original);
                }
            }
            catch (const std::runtime_error&)
            {
                threw = true;
            }
            EXPECT_EQ(threw, throwsReplacement);
            if (partialReplacement)
            {
                objects.push_back(acceptedRid);
            }
            bool retained = false;
            bool accepted = !partialReplacement;
            for (const auto& registration : fc.m_objectRegistrations)
            {
                if (registration.first.second == target)
                {
                    retained = true;
                    EXPECT_EQ(registration.second.rid, target)
                        << "Rejected replacement must not overwrite the retained live physical identity";
                    const auto previous = initialRegistrations.find(registration.first);
                    EXPECT_NE(previous, initialRegistrations.end());
                    if (previous != initialRegistrations.end())
                    {
                        EXPECT_EQ(registration.second.epoch, previous->second.epoch)
                            << "Rejected replacement must preserve the retained object's cleared-sample epoch";
                    }
                }
                if (partialReplacement && registration.first.second == acceptedRid)
                {
                    accepted = true;
                    EXPECT_EQ(registration.second.rid, acceptedRid)
                        << "A later rejection must not roll back an already accepted batch member";
                }
            }
            EXPECT_TRUE(retained);
            EXPECT_TRUE(accepted);
            if (replacement == FlexCounterRejectedReplacement::ThrowingPartialBulk)
            {
                EXPECT_TRUE(std::none_of(fc.m_objectRegistrations.begin(), fc.m_objectRegistrations.end(),
                            [&](const BaseCounterContext::ObjectRegistrationMap::value_type& registration)
                            { return registration.first.second == rejectedRid; }))
                    << "A throwing fresh enrollment must not create a fictitious registration";
            }
        }
        table.del(pluginKey);
        table.del(newPluginKey);
        for (size_t i = 0; i < objects.size(); ++i)
        {
            table.del(toOid(objects[i]));
            const auto rid = objects[i];
            hardware[{rid, statId}] = 111 * (i + 1);
            hardware[{rid, extraId}] = 1000 * (i + 1);
        }
        if (mixedBatch)
        {
            table.del(toOid(addedObject));
            hardware[{addedObject, statId}] = 777;
        }
        const std::string script =
            "redis.call('SELECT', ARGV[1]); "
            "local result = ARGV[2] .. ':destructive_refresh_plugin'; "
            "local seen = {}; "
            "for _, key in ipairs(KEYS) do "
            " if not seen[key] then "
            "  seen[key] = true; "
            "  local value = tonumber(redis.call('HGET', ARGV[2] .. ':' .. key, '" + counterName + "') or '0'); " +
            (hybrid ? "  value = value + tonumber(redis.call('HGET', ARGV[2] .. ':' .. key, '" + extraName + "') or '0'); " : "") +
            "  redis.call('HINCRBY', result, key, value); "
            " end; "
            "end; "
            "redis.call('HINCRBY', result, 'invocations', 1); return {}";
        fc.addCounterPlugin({{port ? PORT_PLUGIN_FIELD : QUEUE_PLUGIN_FIELD, swss::loadRedisScript(&db, script)}});
        auto newScript = script;
        const std::string scriptResult = "destructive_refresh_plugin";
        newScript.replace(newScript.find(scriptResult), scriptResult.size(), newPluginKey);
        const auto newPluginSha = swss::loadRedisScript(&db, newScript);
        armed = true;
        gate.arm();
        fc.addCounterPlugin({{FLEX_COUNTER_STATUS_FIELD, "enable"}});
        if (!gate.waitForFirst())
        {
            gate.releaseAll();
            ADD_FAILURE() << "The destructive-probe regression did not enter its first poll";
            return;
        }

        auto updater = std::async(std::launch::async, [&]
        {
            try
            {
                if (ending == FlexCounterRefreshEnd::RemovePlugins)
                {
                    fc.removeCounterPlugins();
                    return;
                }
                if (ending == FlexCounterRefreshEnd::AddPlugin)
                {
                    fc.addCounterPlugin({{port ? PORT_PLUGIN_FIELD : QUEUE_PLUGIN_FIELD, newPluginSha}});
                    return;
                }
                std::vector<swss::FieldValueTuple> refreshed = {{field,
                        counterName + "," + extraName}};
                if (hybrid || mixedBatch)
                {
                    std::vector<sai_object_id_t> refreshedObjects = {target};
                    if (mixedBatch)
                    {
                        refreshedObjects.push_back(addedObject);
                    }
                    fc.bulkAddCounter(objectType, refreshedObjects, refreshedObjects, refreshed);
                }
                else
                {
                    fc.addCounter(target, target, refreshed);
                }
            }
            catch (const std::exception&)
            {
                updateThrew = true;
            }
        });
        std::future<void> remover;
        if (removeDuringRefresh)
        {
            remover = std::async(std::launch::async, [&] { fc.removeCounter(objects.back()); });
        }
        auto pendingDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
        const uint32_t pending = removeDuringRefresh ? 2 : 1;
        while (fc.m_pendingUpdates.load() < pending && std::chrono::steady_clock::now() < pendingDeadline)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        EXPECT_EQ(fc.m_pendingUpdates.load(), pending);
        gate.releaseFirst();
        EXPECT_TRUE(gate.waitForReadCount(2));
        EXPECT_EQ(updater.wait_for(std::chrono::milliseconds(0)), std::future_status::timeout)
                << "Existing-object refresh must follow the complete old sweep and plugin";
        EXPECT_EQ(destructiveProbes.load(), 0u);
        if (remover.valid())
        {
            bool removed = remover.wait_for(std::chrono::seconds(2)) == std::future_status::ready;
            if (!removed)
            {
                gate.releaseAll();
            }
            remover.get();
            EXPECT_TRUE(removed) << "A deferred refresh blocked unrelated object removal";
        }
        std::future<void> stopper;
        if (ending == FlexCounterRefreshEnd::Shutdown)
        {
            stopper = std::async(std::launch::async, [&] { fc.endFlexCounterThread(); });
            auto shutdownDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
            while (fc.m_pendingUpdates.load() == 0 && std::chrono::steady_clock::now() < shutdownDeadline)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            EXPECT_NE(fc.m_pendingUpdates.load(), 0u);
        }
        gate.releaseAll();
        bool updateFinished = updater.wait_for(std::chrono::seconds(2)) == std::future_status::ready;
        if (!updateFinished)
        {
            // Ensure a broken guard cannot strand a test waiter indefinitely:
            // normal shutdown withdraws the requirement to finish the old poll.
            fc.endFlexCounterThread();
        }
        updater.get();
        EXPECT_TRUE(updateFinished) << "A stopped or failed polling sweep stranded its deferred refresh";
        if (stopper.valid())
        {
            EXPECT_EQ(stopper.wait_for(std::chrono::seconds(2)), std::future_status::ready);
            stopper.get();
        }
        EXPECT_EQ(updateThrew.load(), ending == FlexCounterRefreshEnd::UpdateException);
        EXPECT_EQ(pollThrew.load(), ending == FlexCounterRefreshEnd::PollException);
        const auto updatedRegistrations = fc.m_objectRegistrations;
        for (const auto& initial : initialRegistrations)
        {
            const auto current = updatedRegistrations.find(initial.first);
            if (removeDuringRefresh && initial.first.second == objects.back())
            {
                EXPECT_EQ(current, updatedRegistrations.end());
                continue;
            }
            EXPECT_NE(current, updatedRegistrations.end());
            if (current != updatedRegistrations.end())
            {
                EXPECT_EQ(current->second.rid, initial.second.rid);
                EXPECT_EQ(current->second.epoch, initial.second.epoch)
                    << "Deferred refresh changed an existing registration's lifecycle epoch";
            }
        }
        if (ending == FlexCounterRefreshEnd::Complete || ending == FlexCounterRefreshEnd::UpdateException ||
                ending == FlexCounterRefreshEnd::RemovePlugins || ending == FlexCounterRefreshEnd::AddPlugin)
        {
            if (ending == FlexCounterRefreshEnd::Complete)
            {
                EXPECT_GT(destructiveProbes.load(), 0u) << "The fixture did not execute a real destructive registration probe";
                EXPECT_FALSE(probeBeforePlugin.load()) << "A registration probe cleared an old-plan delta before plugin publication";
            }
            waitForCounterValues(table, pluginKey,
                    {"invocations", toOid(objects.front()), toOid(objects[1])},
                    {"1", hybrid ? "1111" : "111", hybrid ? "2222" : "222"});
        }
        if (ending == FlexCounterRefreshEnd::RemovePlugins || ending == FlexCounterRefreshEnd::AddPlugin)
        {
            std::string value;
            EXPECT_FALSE(table.hget(newPluginKey, "invocations", value))
                    << "New plugin membership took effect in a partially cleared old sweep";
            const auto previous = gate.reads().size();
            fc.addCounterPlugin({{POLL_INTERVAL_FIELD, "1000"}});
            EXPECT_TRUE(gate.waitForReadCount(previous + 1));
            if (ending == FlexCounterRefreshEnd::AddPlugin)
            {
                waitForCounterValues(table, newPluginKey,
                        {"invocations", toOid(objects.front()), toOid(objects[1])}, {"1", "0", "0"});
            }
            else
            {
                waitForCounterValues(table, toOid(objects[1]), {counterName}, {"0"});
                EXPECT_TRUE(table.hget(pluginKey, "invocations", value));
                EXPECT_EQ(value, "1") << "A removed plugin was still invoked by the subsequent sweep";
            }
        }
        if (ending == FlexCounterRefreshEnd::PollException || ending == FlexCounterRefreshEnd::UpdateException)
        {
            auto previous = gate.reads().size();
            fc.addCounterPlugin({{POLL_INTERVAL_FIELD, "10"}});
            EXPECT_TRUE(gate.waitForReadCount(previous + 1))
                    << "A recovered poll could not start after a deferred operation threw";
        }
        if (removeDuringRefresh)
        {
            std::string value;
            EXPECT_FALSE(table.hget(pluginKey, toOid(objects.back()), value));
            auto reads = gate.reads();
            EXPECT_EQ(std::count(reads.begin(), reads.end(), objects.back()), 0);
        }
        if (mixedBatch)
        {
            std::string value;
            EXPECT_FALSE(table.hget(pluginKey, toOid(addedObject), value))
                    << "A newly enrolled object entered the old sweep's accumulator";
        }
        fc.endFlexCounterThread();
        EXPECT_EQ(fc.m_pendingUpdates.load(), 0u);
        EXPECT_EQ(fc.m_deferredUpdates, 0u);
        EXPECT_FALSE(fc.m_pollInProgress);
        armed = false;
        for (auto oid : objects)
        {
            fc.removeCounter(oid);
            table.del(toOid(oid));
        }
        if (mixedBatch)
        {
            fc.removeCounter(addedObject);
            table.del(toOid(addedObject));
        }
        table.del(pluginKey);
        table.del(newPluginKey);
        table.del("TIME_STAMP");
    }

    // Exercise the public bulk enrollment API against real ordinary queue
    // contexts. Keep support-probe failures separate from failures occurring
    // after addObject has removed the old physical enrollment.
    class FlexCounterBulkReplacementFixture
    {
        public:
            enum class ProbeFailure { None, Reject, Throw };

            explicit FlexCounterBulkReplacementFixture(bool supportsBulk = false):
                vendor(std::make_shared<MockableSaiInterface>()),
                objects(generateOids(9, SAI_OBJECT_TYPE_QUEUE)),
                db("COUNTERS_DB", 0), table(&db, COUNTERS_TABLE), m_supportsBulk(supportsBulk)
            {
                SWSS_LOG_ENTER();
                vendor->mock_queryStatsCapability = [this](sai_object_id_t, sai_object_type_t,
                        sai_stat_capability_list_t* capability)
                {
                    if (capability->count < 1)
                    {
                        capability->count = 1;
                        return SAI_STATUS_BUFFER_OVERFLOW;
                    }
                    capability->list[0].stat_enum = SAI_QUEUE_STAT_PACKETS;
                    capability->list[0].stat_modes = SAI_STATS_MODE_READ | SAI_STATS_MODE_READ_AND_CLEAR;
                    if (m_supportsBulk)
                    {
                        capability->list[0].stat_modes |= SAI_STATS_MODE_BULK_READ | SAI_STATS_MODE_BULK_READ_AND_CLEAR;
                    }
                    return SAI_STATUS_SUCCESS;
                };
                vendor->mock_getStats = [this](sai_object_type_t, sai_object_id_t rid,
                        uint32_t count, const sai_stat_id_t*, uint64_t* counterValues)
                {
                    gate.read(rid, 1, counter && counter->m_pendingUpdates.load() != 0);
                    if (probeFailure.load() != ProbeFailure::None && rid == failingRid.load())
                    {
                        ++failedProbeCalls;
                        if (probeFailure.load() == ProbeFailure::Throw)
                        {
                            throw std::runtime_error("Injected replacement support-probe exception");
                        }
                        return SAI_STATUS_FAILURE;
                    }
                    std::lock_guard<std::mutex> lock(m_hardwareMutex);
                    std::fill(counterValues, counterValues + count, m_hardware[rid]);
                    return SAI_STATUS_SUCCESS;
                };
                vendor->mock_clearStats = [this](sai_object_type_t, sai_object_id_t rid,
                        uint32_t, const sai_stat_id_t*)
                {
                    gate.clear(rid);
                    std::lock_guard<std::mutex> lock(m_hardwareMutex);
                    m_hardware[rid] = 0;
                    return SAI_STATUS_SUCCESS;
                };
                vendor->mock_bulkGetStats = [this](sai_object_id_t, sai_object_type_t,
                        uint32_t count, const sai_object_key_t* keys, uint32_t statCount,
                        const sai_stat_id_t*, sai_stats_mode_t mode, sai_status_t* statuses, uint64_t* counterValues)
                {
                    std::vector<sai_object_id_t> rids;
                    for (uint32_t i = 0; i < count; ++i)
                    {
                        rids.push_back(keys[i].key.object_id);
                        if (throwAfterRemovalRid.load() != SAI_NULL_OBJECT_ID &&
                                keys[i].key.object_id == throwAfterRemovalRid.load())
                        {
                            throw std::runtime_error("Injected post-removal replacement exception");
                        }
                    }
                    {
                        std::lock_guard<std::mutex> lock(m_hardwareMutex);
                        bulkCalls.push_back(rids);
                    }
                    if (!m_supportsBulk)
                    {
                        return SAI_STATUS_NOT_SUPPORTED;
                    }
                    gate.read(keys[0].key.object_id, count, counter && counter->m_pendingUpdates.load() != 0);
                    std::lock_guard<std::mutex> lock(m_hardwareMutex);
                    for (uint32_t i = 0; i < count; ++i)
                    {
                        statuses[i] = SAI_STATUS_SUCCESS;
                        std::fill(counterValues + i * statCount, counterValues + (i + 1) * statCount, m_hardware[rids[i]]);
                        if (mode == SAI_STATS_MODE_BULK_READ_AND_CLEAR)
                        {
                            m_hardware[rids[i]] = 0;
                        }
                    }
                    return SAI_STATUS_SUCCESS;
                };
                test_syncd::mockVidManagerObjectTypeQuery(SAI_OBJECT_TYPE_QUEUE);
                counter = std::make_unique<FlexCounter>("bulk_rid_replacement", vendor, "COUNTERS_DB");
                counter->addCounterPlugin({{POLL_INTERVAL_FIELD, "60000"},
                        {STATS_MODE_FIELD, STATS_MODE_READ_AND_CLEAR}, {FLEX_COUNTER_STATUS_FIELD, "disable"}});
                for (const auto vid : objects)
                {
                    table.del(toOid(vid));
                }
                table.del(pluginKey);
            }

            ~FlexCounterBulkReplacementFixture()
            {
                SWSS_LOG_ENTER();
                gate.releaseAll();
                counter->endFlexCounterThread();
                for (const auto vid : objects)
                {
                    counter->removeCounter(vid);
                    table.del(toOid(vid));
                }
                table.del(pluginKey);
                table.del("TIME_STAMP");
            }

            BaseCounterContext::ObjectRidMap actual(const std::vector<sai_object_id_t>& vids)
            {
                SWSS_LOG_ENTER();
                std::lock_guard<std::mutex> lock(counter->m_mtx);
                return counter->m_counterContext.begin()->second->getObjectRids(vids);
            }

            BaseCounterContext::ObjectRegistration registration(sai_object_id_t vid)
            {
                SWSS_LOG_ENTER();
                std::lock_guard<std::mutex> lock(counter->m_mtx);
                return counter->m_objectRegistrations.at({counter->m_counterContext.begin()->first, vid});
            }

            void setHardware(sai_object_id_t rid, uint64_t value)
            {
                SWSS_LOG_ENTER();
                std::lock_guard<std::mutex> lock(m_hardwareMutex);
                m_hardware[rid] = value;
            }

            void enroll(const std::vector<sai_object_id_t>& vids, const std::vector<sai_object_id_t>& rids)
            {
                SWSS_LOG_ENTER();
                counter->bulkAddCounter(SAI_OBJECT_TYPE_QUEUE, vids, rids, values);
            }

            std::shared_ptr<MockableSaiInterface> vendor;
            const std::vector<sai_object_id_t> objects;
            FlexCounterSdkGate gate;
            swss::DBConnector db;
            swss::Table table;
            std::unique_ptr<FlexCounter> counter;
            const std::vector<swss::FieldValueTuple> values = {{QUEUE_COUNTER_ID_LIST, "SAI_QUEUE_STAT_PACKETS"}};
            const std::string pluginKey = "bulk_rejected_replacement_plugin";
            std::atomic<ProbeFailure> probeFailure{ProbeFailure::None};
            std::atomic<sai_object_id_t> failingRid{SAI_NULL_OBJECT_ID};
            std::atomic<sai_object_id_t> throwAfterRemovalRid{SAI_NULL_OBJECT_ID};
            std::atomic<uint32_t> failedProbeCalls{0};
            std::vector<std::vector<sai_object_id_t>> bulkCalls;

        private:
            const bool m_supportsBulk;
            std::mutex m_hardwareMutex;
            std::map<sai_object_id_t, uint64_t> m_hardware;
    };

    void testRejectedBulkReplacementPreservesDelta(bool throws)
    {
        SWSS_LOG_ENTER();
        FlexCounterBulkReplacementFixture fixture;
        auto& fc = *fixture.counter;
        const auto a = fixture.objects[0], b = fixture.objects[1], rejected = fixture.objects[2];
        fixture.enroll({a, b}, {a, b});
        const auto original = fixture.registration(b);
        fixture.failingRid = rejected;
        fixture.probeFailure = throws ? FlexCounterBulkReplacementFixture::ProbeFailure::Throw :
            FlexCounterBulkReplacementFixture::ProbeFailure::Reject;
        if (throws)
        {
            EXPECT_THROW(fixture.enroll({b}, {rejected}), std::runtime_error);
        }
        else
        {
            EXPECT_NO_THROW(fixture.enroll({b}, {rejected}));
        }
        EXPECT_GT(fixture.failedProbeCalls.load(), 0u) << "The changed RID was silently rejected before its actual probe";
        ASSERT_EQ(fixture.actual({b}).at(b).rid, b);
        const auto retained = fixture.registration(b);
        EXPECT_EQ(retained.rid, b);
        EXPECT_EQ(retained.epoch, original.epoch);

        const std::string script = "redis.call('SELECT',ARGV[1]); local result=ARGV[2]..':" + fixture.pluginKey +
            "'; for _,key in ipairs(KEYS) do redis.call('HINCRBY',result,key,"
            "tonumber(redis.call('HGET',ARGV[2]..':'..key,'SAI_QUEUE_STAT_PACKETS') or '0')); end; "
            "redis.call('HINCRBY',result,'invocations',1); return {}";
        fc.addCounterPlugin({{QUEUE_PLUGIN_FIELD, swss::loadRedisScript(&fixture.db, script)}});
        fixture.setHardware(a, 111);
        fixture.setHardware(b, 222);
        fixture.gate.arm();
        fc.addCounterPlugin({{FLEX_COUNTER_STATUS_FIELD, "enable"}});
        if (!fixture.gate.waitForFirst())
        {
            fixture.gate.releaseAll();
            ADD_FAILURE() << "The retained-object polling sweep did not start";
            return;
        }
        auto refresh = std::async(std::launch::async, [&] { fixture.enroll({b}, {b}); });
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
        while (fc.m_pendingUpdates.load() == 0 && std::chrono::steady_clock::now() < deadline)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        EXPECT_NE(fc.m_pendingUpdates.load(), 0u);
        fixture.gate.releaseFirst();
        EXPECT_TRUE(fixture.gate.waitForReadCount(2));
        EXPECT_EQ(refresh.wait_for(std::chrono::milliseconds(0)), std::future_status::timeout)
            << "The retained RID's destructive refresh overtook the old sample/plugin";
        fixture.gate.releaseAll();
        const bool finished = refresh.wait_for(std::chrono::seconds(2)) == std::future_status::ready;
        if (!finished)
        {
            fc.endFlexCounterThread();
        }
        refresh.get();
        EXPECT_TRUE(finished);
        fc.endFlexCounterThread();
        // The refresh can wake another sweep. Its cleared zero values must not
        // duplicate or erase the delta aggregated by the original sweep.
        waitForCounterValues(fixture.table, fixture.pluginKey, {toOid(a), toOid(b)}, {"111", "222"});
        std::string invocations;
        EXPECT_TRUE(fixture.table.hget(fixture.pluginKey, "invocations", invocations));
        EXPECT_FALSE(invocations.empty());
        EXPECT_EQ(fc.m_pendingUpdates.load(), 0u);
        EXPECT_EQ(fc.m_deferredUpdates, 0u);
    }

    enum class FlexCounterSpecializedContext
    {
        Attribute,
        Phy,
        Serdes,
        PerPortDiscovery,
        DashMeter,
    };

    void testSpecializedReplacementRequiresRemoval(FlexCounterSpecializedContext kind)
    {
        SWSS_LOG_ENTER();
        auto vendor = std::make_shared<MockableSaiInterface>();
        FlexCounterSdkGate gate;
        const bool phy = kind == FlexCounterSpecializedContext::Phy;
        const bool serdes = kind == FlexCounterSpecializedContext::Serdes;
        const bool attribute = kind == FlexCounterSpecializedContext::Attribute;
        const bool perPort = kind == FlexCounterSpecializedContext::PerPortDiscovery;
        const bool dash = kind == FlexCounterSpecializedContext::DashMeter;
        const auto type = attribute ? SAI_OBJECT_TYPE_QUEUE : serdes ? SAI_OBJECT_TYPE_PORT_SERDES :
            dash ? static_cast<sai_object_type_t>(SAI_OBJECT_TYPE_ENI) : SAI_OBJECT_TYPE_PORT;
        const auto objects = generateOids(2, type);
        const auto vid = objects.front();
        const auto replacementRid = objects.back();
        const auto portRid = static_cast<sai_object_id_t>(0x10000000000f);
        std::atomic<uint32_t> replacementCalls{0};
        vendor->mock_queryStatsCapability = [=](sai_object_id_t, sai_object_type_t,
                sai_stat_capability_list_t* capability)
        {
            if (capability->count < 1)
            {
                capability->count = 1;
                return SAI_STATUS_BUFFER_OVERFLOW;
            }
            capability->list[0].stat_enum = dash ? static_cast<sai_stat_id_t>(SAI_METER_BUCKET_ENTRY_STAT_INBOUND_BYTES) :
                static_cast<sai_stat_id_t>(SAI_PORT_STAT_IF_IN_OCTETS);
            capability->list[0].stat_modes = SAI_STATS_MODE_READ | SAI_STATS_MODE_READ_AND_CLEAR;
            if (dash)
            {
                capability->list[0].stat_modes |= SAI_STATS_MODE_BULK_READ | SAI_STATS_MODE_BULK_READ_AND_CLEAR;
            }
            return SAI_STATUS_SUCCESS;
        };
        vendor->mock_getStats = [&](sai_object_type_t, sai_object_id_t rid,
                uint32_t count, const sai_stat_id_t*, uint64_t* values)
        {
            replacementCalls += rid == replacementRid;
            gate.read(rid, 1, false);
            std::fill(values, values + count, 7);
            return SAI_STATUS_SUCCESS;
        };
        vendor->mock_clearStats = [](sai_object_type_t, sai_object_id_t, uint32_t, const sai_stat_id_t*)
        {
            return SAI_STATUS_SUCCESS;
        };
        vendor->mock_bulkGetStats = [&](sai_object_id_t, sai_object_type_t objectType,
                uint32_t count, const sai_object_key_t* keys, uint32_t statCount,
                const sai_stat_id_t*, sai_stats_mode_t, sai_status_t* statuses, uint64_t* values)
        {
            const auto rid = objectType == static_cast<sai_object_type_t>(SAI_OBJECT_TYPE_METER_BUCKET_ENTRY) ?
                keys[0].key.meter_bucket_entry.eni_id : keys[0].key.object_id;
            replacementCalls += rid == replacementRid;
            if (!dash)
            {
                return SAI_STATUS_NOT_SUPPORTED;
            }
            gate.read(rid, count, false);
            std::fill(statuses, statuses + count, SAI_STATUS_SUCCESS);
            std::fill(values, values + count * statCount, 7);
            return SAI_STATUS_SUCCESS;
        };
        vendor->mock_get = [&](sai_object_type_t objectType, sai_object_id_t rid,
                uint32_t count, sai_attribute_t* attrs)
        {
            replacementCalls += rid == replacementRid;
            if (dash)
            {
                attrs[0].value.u32 = 1;
                return SAI_STATUS_SUCCESS;
            }
            if (serdes && objectType == SAI_OBJECT_TYPE_PORT_SERDES)
            {
                if (attrs[0].id == SAI_PORT_SERDES_ATTR_PORT_ID)
                {
                    attrs[0].value.oid = portRid;
                    return SAI_STATUS_SUCCESS;
                }
                if (attrs[0].id == SAI_PORT_SERDES_ATTR_TX_FIR_COUNT)
                {
                    attrs[0].value.u32 = 1;
                    return SAI_STATUS_SUCCESS;
                }
            }
            if (serdes && objectType == SAI_OBJECT_TYPE_PORT && attrs[0].id == SAI_PORT_ATTR_HW_LANE_LIST)
            {
                attrs[0].value.u32list.count = 1;
                return SAI_STATUS_BUFFER_OVERFLOW;
            }
            if (phy && attrs[0].value.portsnrlist.list == nullptr)
            {
                attrs[0].value.portsnrlist.count = 1;
                return SAI_STATUS_BUFFER_OVERFLOW;
            }
            gate.read(rid, 1, false);
            for (uint32_t i = 0; i < count; ++i)
            {
                if (phy)
                {
                    attrs[i].value.portsnrlist.list[0].lane = 0;
                    attrs[i].value.portsnrlist.list[0].snr = 256;
                }
                else if (serdes)
                {
                    attrs[i].value.u32list.list[0] = 7;
                }
                else
                {
                    attrs[i].value.booldata = true;
                }
            }
            return SAI_STATUS_SUCCESS;
        };
        vendor->mock_switchIdQuery = [](sai_object_id_t rid) { return VidManager::switchIdQuery(rid); };
        if (perPort)
        {
            auto options = std::make_shared<VendorSaiOptions>();
            options->m_enablePerPortCounterDiscovery = true;
            vendor->setOptions(VendorSaiOptions::OPTIONS_KEY, options);
        }
        swss::DBConnector db("COUNTERS_DB", 0);
        swss::Table table(&db, phy || serdes ? PORT_PHY_ATTR_TABLE : COUNTERS_TABLE);
        swss::Table mapping(&db, "COUNTERS_PORT_SERDES_ID_TO_PORT_ID_MAP");
        if (serdes)
        {
            mapping.set("", {{toOid(vid), toOid(portRid)}});
        }
        test_syncd::mockVidManagerObjectTypeQuery(type);
        FlexCounter fc("specialized_replacement", vendor, "COUNTERS_DB");
        fc.addCounterPlugin({{POLL_INTERVAL_FIELD, "60000"}, {STATS_MODE_FIELD, STATS_MODE_READ},
                {FLEX_COUNTER_STATUS_FIELD, "disable"}});
        const std::vector<swss::FieldValueTuple> values = {{
            attribute ? QUEUE_ATTR_ID_LIST : phy ? PORT_PHY_ATTR_ID_LIST :
                serdes ? PORT_PHY_SERDES_ATTR_ID_LIST : dash ? DASH_METER_COUNTER_ID_LIST : PORT_COUNTER_ID_LIST,
            attribute ? "SAI_QUEUE_ATTR_PAUSE_STATUS" : phy ? "SAI_PORT_ATTR_RX_SNR" :
                serdes ? "SAI_PORT_SERDES_ATTR_RX_VGA" : dash ? "SAI_METER_BUCKET_ENTRY_STAT_INBOUND_BYTES" :
                "SAI_PORT_STAT_IF_IN_OCTETS",
        }};
        auto enroll = [&](sai_object_id_t rid)
        {
            fc.addCounter(vid, rid, values);
        };
        enroll(vid);
        ASSERT_FALSE(fc.isEmpty());
        ASSERT_EQ(fc.m_objectRegistrations.size(), 1u);
        const auto originalRegistration = fc.m_objectRegistrations.begin()->second;
        enroll(replacementRid);
        EXPECT_EQ(replacementCalls.load(), 0u)
            << "Unsupported physical replacement must be rejected before probing the new SDK object";
        ASSERT_EQ(fc.m_objectRegistrations.size(), 1u);
        EXPECT_EQ(fc.m_objectRegistrations.begin()->second.rid, vid);
        EXPECT_EQ(fc.m_objectRegistrations.begin()->second.epoch, originalRegistration.epoch);
        table.del(toOid(vid));
        if (serdes)
        {
            // Serdes samples are stored under the associated parent-port VID.
            table.del(toOid(portRid));
        }
        gate.arm();
        fc.addCounterPlugin({{FLEX_COUNTER_STATUS_FIELD, "enable"}});
        const bool firstEntered = gate.waitForFirst();
        gate.releaseAll();
        EXPECT_TRUE(firstEntered);
        fc.addCounterPlugin({{FLEX_COUNTER_STATUS_FIELD, "disable"}});
        const auto originalReads = gate.reads();
        ASSERT_FALSE(originalReads.empty());
        EXPECT_EQ(originalReads.front(), vid) << "Rejected replacement changed the live polling RID";
        fc.removeCounter(vid);
        enroll(replacementRid);
        ASSERT_EQ(fc.m_objectRegistrations.size(), 1u);
        EXPECT_EQ(fc.m_objectRegistrations.begin()->second.rid, replacementRid);
        fc.addCounterPlugin({{FLEX_COUNTER_STATUS_FIELD, "enable"}});
        EXPECT_TRUE(gate.waitForReadCount(originalReads.size() + 1));
        fc.endFlexCounterThread();
        const auto replacementReads = gate.reads();
        ASSERT_GT(replacementReads.size(), originalReads.size());
        EXPECT_EQ(replacementReads.back(), replacementRid)
            << "Explicit removal and re-enrollment did not admit the new physical object";
        fc.removeCounter(vid);
        table.del(toOid(vid));
        table.del("TIME_STAMP");
        if (serdes)
        {
            table.del(toOid(portRid));
            mapping.del("");
        }
    }

    void testPendingRemovalPreemptsAttributeSweep(bool phy)
    {
        SWSS_LOG_ENTER();
        auto vendor = std::make_shared<MockableSaiInterface>();
        FlexCounterSdkGate gate;
        FlexCounter* counter = nullptr;
        auto objectType = phy ? SAI_OBJECT_TYPE_PORT : SAI_OBJECT_TYPE_QUEUE;
        auto objects = generateOids(3, objectType);

        vendor->mock_get = [&](sai_object_type_t, sai_object_id_t oid,
                uint32_t count, sai_attribute_t* attrs)
        {
            // PHY registration first discovers lane counts; gate only the real
            // SDK read after its output buffers have been allocated.
            if (phy && attrs[0].value.portsnrlist.list == nullptr)
            {
                attrs[0].value.portsnrlist.count = 1;
                return SAI_STATUS_BUFFER_OVERFLOW;
            }
            gate.read(oid, 1, counter && counter->m_pendingUpdates.load() != 0);
            for (uint32_t i = 0; i < count; ++i)
            {
                if (phy)
                {
                    attrs[i].value.portsnrlist.list[0].lane = 0;
                    attrs[i].value.portsnrlist.list[0].snr = 256;
                }
                else
                {
                    attrs[i].value.booldata = true;
                }
            }
            return SAI_STATUS_SUCCESS;
        };

        test_syncd::mockVidManagerObjectTypeQuery(objectType);
        FlexCounter fc("pending_attribute_update", vendor, "COUNTERS_DB");
        counter = &fc;
        fc.addCounterPlugin({
                {POLL_INTERVAL_FIELD, "60000"},
                {FLEX_COUNTER_STATUS_FIELD, "disable"},
        });
        std::vector<swss::FieldValueTuple> values = {{
            phy ? PORT_PHY_ATTR_ID_LIST : QUEUE_ATTR_ID_LIST,
            phy ? "SAI_PORT_ATTR_RX_SNR" : "SAI_QUEUE_ATTR_PAUSE_STATUS",
        }};
        for (auto oid : objects)
        {
            fc.addCounter(oid, oid, values);
        }

        swss::DBConnector db("COUNTERS_DB", 0);
        swss::Table table(&db, phy ? PORT_PHY_ATTR_TABLE : COUNTERS_TABLE);
        for (auto oid : objects)
        {
            table.del(toOid(oid));
        }
        gate.arm();
        fc.addCounterPlugin({{FLEX_COUNTER_STATUS_FIELD, "enable"}});
        if (!gate.waitForFirst())
        {
            gate.releaseAll();
            ADD_FAILURE() << "The first attribute SDK read did not start";
            return;
        }

        auto updater = std::async(std::launch::async, [&]
        {
            fc.removeCounter(objects.back());
        });
        auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
        while (fc.m_pendingUpdates.load() == 0 && std::chrono::steady_clock::now() < deadline)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        EXPECT_NE(fc.m_pendingUpdates.load(), 0u);
        gate.releaseFirst();
        bool updateFinished = updater.wait_for(std::chrono::seconds(2)) == std::future_status::ready;
        if (!updateFinished)
        {
            gate.releaseAll();
        }
        updater.get();
        EXPECT_TRUE(updateFinished);
        EXPECT_FALSE(gate.readWhileUpdatePending());

        const std::string field = phy ? "rx_snr" : "SAI_QUEUE_ATTR_PAUSE_STATUS";
        const std::string expected = phy ? "{\"0\":1.0}" : "true";
        std::string value;
        // PHY owns a separate Redis pipeline: flushing COUNTERS alone at the
        // checkpoint would leave this completed attribute unpublished.
        EXPECT_TRUE(table.hget(toOid(objects.front()), field, value));
        EXPECT_EQ(value, expected);
        gate.releaseAll();
        waitForCounterValues(table, toOid(objects[1]), {field}, {expected});
        fc.endFlexCounterThread();
        auto reads = gate.reads();
        EXPECT_EQ(std::count(reads.begin(), reads.end(), objects.back()), 0);
        for (auto oid : objects)
        {
            fc.removeCounter(oid);
            table.del(toOid(oid));
        }
    }

    void testPendingAttributeBulkAdmission(bool unsupportedBulk)
    {
        SWSS_LOG_ENTER();
        auto vendor = std::make_shared<MockableSaiInterface>();
        FlexCounterSdkGate gate;
        FlexCounter* counter = nullptr;
        const auto port = generateOids(1, SAI_OBJECT_TYPE_PORT).front();
        const auto queues = generateOids(3, SAI_OBJECT_TYPE_QUEUE);
        const auto removed = queues[0], survivor = queues[1], added = queues[2];
        std::vector<std::vector<sai_object_id_t>> bulkReads;
        std::vector<sai_object_id_t> scalarReads;
        vendor->mock_queryStatsCapability = [](sai_object_id_t, sai_object_type_t,
                sai_stat_capability_list_t* capability)
        {
            if (capability->count < 1)
            {
                capability->count = 1;
                return SAI_STATUS_BUFFER_OVERFLOW;
            }
            capability->list[0].stat_enum = SAI_PORT_STAT_IF_IN_OCTETS;
            capability->list[0].stat_modes = SAI_STATS_MODE_READ;
            return SAI_STATUS_SUCCESS;
        };
        vendor->mock_getStats = [&](sai_object_type_t, sai_object_id_t rid,
                uint32_t count, const sai_stat_id_t*, uint64_t* counters)
        {
            gate.read(rid, 1, counter && counter->m_pendingUpdates.load() != 0);
            std::fill(counters, counters + count, 11);
            return SAI_STATUS_SUCCESS;
        };
        vendor->mock_bulkGetStats = [](sai_object_id_t, sai_object_type_t, uint32_t,
                const sai_object_key_t*, uint32_t, const sai_stat_id_t*,
                sai_stats_mode_t, sai_status_t*, uint64_t*)
        {
            return SAI_STATUS_NOT_SUPPORTED;
        };
        vendor->mock_bulkGet = [&](sai_object_type_t type, uint32_t count,
                const sai_object_id_t* rids, const uint32_t* attrCounts,
                sai_attribute_t** attrs, sai_bulk_op_error_mode_t mode, sai_status_t* statuses)
        {
            EXPECT_EQ(type, SAI_OBJECT_TYPE_QUEUE);
            EXPECT_EQ(mode, SAI_BULK_OP_ERROR_MODE_IGNORE_ERROR);
            bulkReads.emplace_back(rids, rids + count);
            if (unsupportedBulk)
            {
                return SAI_STATUS_NOT_SUPPORTED;
            }
            gate.read(rids[0], count, counter->m_pendingUpdates.load() != 0);
            for (uint32_t i = 0; i < count; ++i)
            {
                EXPECT_EQ(attrCounts[i], 1u);
                EXPECT_EQ(attrs[i][0].id, SAI_QUEUE_ATTR_PAUSE_STATUS);
                attrs[i][0].value.booldata = true;
                statuses[i] = SAI_STATUS_SUCCESS;
            }
            return SAI_STATUS_SUCCESS;
        };
        vendor->mock_get = [&](sai_object_type_t type, sai_object_id_t rid,
                uint32_t count, sai_attribute_t* attrs)
        {
            EXPECT_EQ(type, SAI_OBJECT_TYPE_QUEUE);
            EXPECT_EQ(count, 1u);
            scalarReads.push_back(rid);
            gate.read(rid, 1, counter->m_pendingUpdates.load() != 0);
            attrs[0].value.booldata = true;
            return SAI_STATUS_SUCCESS;
        };

        FlexCounter fc("pending_attribute_bulk_admission", vendor, "COUNTERS_DB");
        counter = &fc;
        fc.addCounterPlugin({{POLL_INTERVAL_FIELD, "60000"},
                {FLEX_COUNTER_STATUS_FIELD, "disable"}, {STATS_MODE_FIELD, STATS_MODE_READ}});
        test_syncd::mockVidManagerObjectTypeQuery(SAI_OBJECT_TYPE_PORT);
        fc.addCounter(port, port, {{PORT_COUNTER_ID_LIST, "SAI_PORT_STAT_IF_IN_OCTETS"}});
        test_syncd::mockVidManagerObjectTypeQuery(SAI_OBJECT_TYPE_QUEUE);
        const std::vector<swss::FieldValueTuple> attributes = {{QUEUE_ATTR_ID_LIST, "SAI_QUEUE_ATTR_PAUSE_STATUS"}};
        fc.addCounter(removed, removed, attributes);
        fc.addCounter(survivor, survivor, attributes);
        swss::DBConnector db("COUNTERS_DB", 0);
        swss::Table table(&db, COUNTERS_TABLE);
        const std::string pluginKey = "pending_attribute_bulk_plugin";
        const auto sha = swss::loadRedisScript(&db,
                "redis.call('SELECT',ARGV[1]); local result=ARGV[2]..':" + pluginKey +
                "'; for _,key in ipairs(KEYS) do redis.call('HSET',result,key,"
                "redis.call('HGET',ARGV[2]..':'..key,'SAI_QUEUE_ATTR_PAUSE_STATUS')); end; "
                "redis.call('HINCRBY',result,'invocations',1); return {}");
        // No public QUEUE_ATTRIBUTE_PLUGIN field exists. Exercise the real
        // inherited plugin path through its context API while polling is off.
        {
            std::lock_guard<std::mutex> lock(fc.m_mtx);
            fc.m_counterContext.at("Queue Attribute")->addPlugins({sha});
        }
        table.del(toOid(port));
        for (const auto oid : queues)
        {
            table.del(toOid(oid));
        }
        table.del(pluginKey);
        gate.arm();
        fc.addCounterPlugin({{FLEX_COUNTER_STATUS_FIELD, "enable"}});
        if (!gate.waitForFirst())
        {
            gate.releaseAll();
            fc.endFlexCounterThread();
            ADD_FAILURE() << "The preceding scalar context did not start";
            return;
        }
        auto remover = std::async(std::launch::async, [&] { fc.removeCounter(removed); });
        auto adder = std::async(std::launch::async, [&] { fc.addCounter(added, added, attributes); });
        auto disabler = std::async(std::launch::async, [&]
        {
            fc.addCounterPlugin({{FLEX_COUNTER_STATUS_FIELD, "disable"}});
        });
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
        while (fc.m_pendingUpdates.load() < 3 && std::chrono::steady_clock::now() < deadline)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        EXPECT_EQ(fc.m_pendingUpdates.load(), 3u);
        gate.releaseFirst();
        const bool removedAtCheckpoint = remover.wait_for(std::chrono::seconds(2)) == std::future_status::ready;
        const bool addedAtCheckpoint = adder.wait_for(std::chrono::seconds(2)) == std::future_status::ready;
        const bool disabledAtCheckpoint = disabler.wait_for(std::chrono::seconds(2)) == std::future_status::ready;
        if (!removedAtCheckpoint || !addedAtCheckpoint || !disabledAtCheckpoint)
        {
            gate.releaseAll();
        }
        remover.get();
        adder.get();
        disabler.get();
        EXPECT_TRUE(removedAtCheckpoint);
        EXPECT_TRUE(addedAtCheckpoint);
        EXPECT_TRUE(disabledAtCheckpoint);
        EXPECT_TRUE(gate.waitForReadCount(2));
        gate.releaseAll();
        waitForCounterValues(table, pluginKey, {"invocations", toOid(survivor)}, {"1", "true"});
        fc.endFlexCounterThread();
        EXPECT_EQ(bulkReads, (std::vector<std::vector<sai_object_id_t>>{{survivor}}));
        EXPECT_EQ(scalarReads, unsupportedBulk ? std::vector<sai_object_id_t>{survivor} :
                std::vector<sai_object_id_t>{});
        EXPECT_EQ(gate.reads(), (std::vector<sai_object_id_t>{port, survivor}));
        EXPECT_FALSE(gate.readWhileUpdatePending());
        std::string value;
        EXPECT_FALSE(table.hget(pluginKey, toOid(removed), value));
        EXPECT_FALSE(table.hget(pluginKey, toOid(added), value));
        EXPECT_FALSE(table.hget(toOid(added), "SAI_QUEUE_ATTR_PAUSE_STATUS", value));

        if (unsupportedBulk)
        {
            // A second logical sweep must use the latched scalar fallback and
            // can now admit the object enrolled during the previous pass.
            fc.startFlexCounterThread();
            fc.addCounterPlugin({{FLEX_COUNTER_STATUS_FIELD, "enable"}});
            waitForCounterValues(table, toOid(added), {"SAI_QUEUE_ATTR_PAUSE_STATUS"}, {"true"});
            fc.endFlexCounterThread();
            EXPECT_EQ(bulkReads.size(), 1u);
            EXPECT_EQ(std::count(scalarReads.begin(), scalarReads.end(), added), 1);
        }
        fc.removeCounter(port);
        table.del(toOid(port));
        for (const auto oid : queues)
        {
            fc.removeCounter(oid);
            table.del(toOid(oid));
        }
        table.del(pluginKey);
        table.del("TIME_STAMP");
    }
}

TEST(FlexCounter, attributeOnlyGroupDefaultsToReadStatsMode)
{
    auto vendor = std::make_shared<MockableSaiInterface>();
    const auto oid = generateOids(1, SAI_OBJECT_TYPE_QUEUE).front();
    std::atomic<uint32_t> reads{0};
    vendor->mock_get = [&](sai_object_type_t type, sai_object_id_t objectId,
            uint32_t count, sai_attribute_t* attrs)
    {
        if (type != SAI_OBJECT_TYPE_QUEUE || objectId != oid || count != 1 ||
                attrs[0].id != SAI_QUEUE_ATTR_PAUSE_STATUS)
        {
            return SAI_STATUS_FAILURE;
        }
        attrs[0].value.booldata = true;
        ++reads;
        return SAI_STATUS_SUCCESS;
    };

    test_syncd::mockVidManagerObjectTypeQuery(SAI_OBJECT_TYPE_QUEUE);
    FlexCounter fc("attribute_default_stats_mode", vendor, "COUNTERS_DB");
    EXPECT_EQ(fc.m_statsMode, SAI_STATS_MODE_READ);
    fc.addCounterPlugin({{POLL_INTERVAL_FIELD, "60000"}});
    fc.addCounter(oid, oid, {{QUEUE_ATTR_ID_LIST, "SAI_QUEUE_ATTR_PAUSE_STATUS"}});

    swss::DBConnector db("COUNTERS_DB", 0);
    swss::Table table(&db, COUNTERS_TABLE);
    table.del(toOid(oid));
    // Attribute-only groups need not provide a STATS_MODE field at any point.
    fc.addCounterPlugin({{FLEX_COUNTER_STATUS_FIELD, "enable"}});
    waitForCounterValues(table, toOid(oid), {"SAI_QUEUE_ATTR_PAUSE_STATUS"}, {"true"});
    fc.endFlexCounterThread();

    EXPECT_GT(reads.load(), 0u);
    ASSERT_EQ(fc.m_counterContext.size(), 1u);
    EXPECT_EQ(fc.m_counterContext.begin()->second->m_pollStatsMode, SAI_STATS_MODE_READ);
    fc.removeCounter(oid);
    table.del(toOid(oid));
}

TEST(FlexCounter, replacementExceptionAfterRemovalErasesObsoleteRegistration)
{
    auto vendor = std::make_shared<MockableSaiInterface>();
    const auto objects = generateOids(2, SAI_OBJECT_TYPE_QUEUE);
    const auto vid = objects.front();
    const auto replacementRid = objects.back();
    bool throwAfterRemoval = false;
    vendor->mock_queryStatsCapability = [](sai_object_id_t, sai_object_type_t,
            sai_stat_capability_list_t* capability)
    {
        if (capability->count < 1)
        {
            capability->count = 1;
            return SAI_STATUS_BUFFER_OVERFLOW;
        }
        capability->list[0].stat_enum = SAI_QUEUE_STAT_PACKETS;
        capability->list[0].stat_modes = SAI_STATS_MODE_READ;
        return SAI_STATUS_SUCCESS;
    };
    vendor->mock_getStats = [](sai_object_type_t, sai_object_id_t,
            uint32_t count, const sai_stat_id_t*, uint64_t* values)
    {
        std::fill(values, values + count, 222);
        return SAI_STATUS_SUCCESS;
    };
    vendor->mock_bulkGetStats = [&](sai_object_id_t, sai_object_type_t,
            uint32_t count, const sai_object_key_t* keys, uint32_t,
            const sai_stat_id_t*, sai_stats_mode_t, sai_status_t*, uint64_t*)
    {
        // The ordinary scalar add path removes the previous enrollment before
        // this bulk-capability probe. A throw must reconcile that actual absence.
        if (throwAfterRemoval && count == 1 && keys[0].key.object_id == replacementRid)
        {
            throw std::runtime_error("Injected post-removal capability exception");
        }
        return SAI_STATUS_NOT_SUPPORTED;
    };
    test_syncd::mockVidManagerObjectTypeQuery(SAI_OBJECT_TYPE_QUEUE);
    FlexCounter fc("post_removal_exception", vendor, "COUNTERS_DB");
    fc.addCounterPlugin({{FLEX_COUNTER_STATUS_FIELD, "disable"}, {STATS_MODE_FIELD, STATS_MODE_READ}});
    const std::vector<swss::FieldValueTuple> values = {{QUEUE_COUNTER_ID_LIST, "SAI_QUEUE_STAT_PACKETS"}};
    fc.addCounter(vid, vid, values);
    EXPECT_FALSE(fc.isEmpty());
    throwAfterRemoval = true;
    EXPECT_THROW(fc.addCounter(vid, replacementRid, values), std::runtime_error);
    EXPECT_TRUE(fc.isEmpty());
    EXPECT_TRUE(std::none_of(fc.m_objectRegistrations.begin(), fc.m_objectRegistrations.end(),
                [&](const BaseCounterContext::ObjectRegistrationMap::value_type& registration)
                { return registration.first.second == vid; }))
        << "Neither the old RID nor the failed replacement is enrolled after the exception";
    EXPECT_EQ(fc.m_pendingUpdates.load(), 0u);
    EXPECT_EQ(fc.m_deferredUpdates, 0u);
    throwAfterRemoval = false;
    fc.addCounter(vid, replacementRid, values);
    EXPECT_FALSE(fc.isEmpty());
    ASSERT_EQ(fc.m_objectRegistrations.size(), 1u);
    EXPECT_EQ(fc.m_objectRegistrations.begin()->second.rid, replacementRid);
    fc.removeCounter(vid);
}

TEST(FlexCounter, invalidBulkEnrollmentDoesNotProbeOrMutateLiveRegistrations)
{
    auto vendor = std::make_shared<MockableSaiInterface>();
    const auto objects = generateOids(2, SAI_OBJECT_TYPE_PORT);
    uint32_t calls = 0;
    std::vector<uint32_t> bulkCounts;
    vendor->mock_queryStatsCapability = [&](sai_object_id_t, sai_object_type_t,
            sai_stat_capability_list_t* capability)
    {
        ++calls;
        if (capability->count < 1)
        {
            capability->count = 1;
            return SAI_STATUS_BUFFER_OVERFLOW;
        }
        capability->list[0].stat_enum = SAI_PORT_STAT_IF_IN_OCTETS;
        capability->list[0].stat_modes = SAI_STATS_MODE_READ | SAI_STATS_MODE_BULK_READ;
        return SAI_STATUS_SUCCESS;
    };
    vendor->mock_getStats = [&](sai_object_type_t, sai_object_id_t,
            uint32_t count, const sai_stat_id_t*, uint64_t* values)
    {
        ++calls;
        std::fill(values, values + count, 7);
        return SAI_STATUS_SUCCESS;
    };
    vendor->mock_bulkGetStats = [&](sai_object_id_t, sai_object_type_t,
            uint32_t count, const sai_object_key_t*, uint32_t statCount,
            const sai_stat_id_t*, sai_stats_mode_t, sai_status_t* statuses, uint64_t* values)
    {
        ++calls;
        bulkCounts.push_back(count);
        std::fill(statuses, statuses + count, SAI_STATUS_SUCCESS);
        std::fill(values, values + count * statCount, 7);
        return SAI_STATUS_SUCCESS;
    };
    test_syncd::mockVidManagerObjectTypeQuery(SAI_OBJECT_TYPE_PORT);
    FlexCounter fc("invalid_bulk_enrollment", vendor, "COUNTERS_DB", true);
    fc.addCounterPlugin({{FLEX_COUNTER_STATUS_FIELD, "disable"}, {STATS_MODE_FIELD, STATS_MODE_READ}});
    const std::vector<swss::FieldValueTuple> values = {{PORT_COUNTER_ID_LIST, "SAI_PORT_STAT_IF_IN_OCTETS"}};
    for (const auto oid : objects)
    {
        fc.addCounter(oid, oid, values);
    }
    const auto initial = fc.m_objectRegistrations;
    calls = 0;
    bulkCounts.clear();
    fc.bulkAddCounter(SAI_OBJECT_TYPE_PORT, {}, {}, values);
    fc.bulkAddCounter(SAI_OBJECT_TYPE_PORT, objects, {objects.front()}, values);
    fc.bulkAddCounter(SAI_OBJECT_TYPE_PORT, {objects.front()}, objects, values);
    fc.bulkAddCounter(SAI_OBJECT_TYPE_PORT, {objects.front(), objects.front()}, objects, values);
    EXPECT_EQ(calls, 0u);
    ASSERT_EQ(fc.m_objectRegistrations.size(), initial.size());
    for (const auto& registration : initial)
    {
        const auto current = fc.m_objectRegistrations.find(registration.first);
        ASSERT_NE(current, fc.m_objectRegistrations.end());
        EXPECT_EQ(current->second.rid, registration.second.rid);
        EXPECT_EQ(current->second.epoch, registration.second.epoch);
    }
    fc.bulkAddCounter(SAI_OBJECT_TYPE_PORT, {objects.front(), objects.front()},
            {objects.front(), objects.front()}, values);
    EXPECT_FALSE(bulkCounts.empty());
    for (const auto count : bulkCounts)
    {
        EXPECT_EQ(count, 1u) << "Exact duplicate bulk inputs must be deduplicated before SDK calls";
    }
    for (const auto oid : objects)
    {
        fc.removeCounter(oid);
    }
}

TEST(FlexCounter, conflictingPhysicalFragmentsAreQuarantinedTogether)
{
    auto vendor = std::make_shared<MockableSaiInterface>();
    const auto objects = generateOids(2, SAI_OBJECT_TYPE_PORT);
    const auto vid = objects.front();
    uint32_t calls = 0;
    vendor->mock_queryStatsCapability = [&](sai_object_id_t, sai_object_type_t,
            sai_stat_capability_list_t* capability)
    {
        ++calls;
        if (capability->count < 2)
        {
            capability->count = 2;
            return SAI_STATUS_BUFFER_OVERFLOW;
        }
        capability->list[0].stat_enum = SAI_PORT_STAT_IF_IN_OCTETS;
        capability->list[1].stat_enum = SAI_PORT_STAT_IF_IN_UCAST_PKTS;
        for (uint32_t i = 0; i < 2; ++i)
        {
            capability->list[i].stat_modes = SAI_STATS_MODE_READ | SAI_STATS_MODE_BULK_READ;
        }
        return SAI_STATUS_SUCCESS;
    };
    vendor->mock_bulkGetStats = [&](sai_object_id_t, sai_object_type_t,
            uint32_t count, const sai_object_key_t*, uint32_t statCount,
            const sai_stat_id_t*, sai_stats_mode_t, sai_status_t* statuses, uint64_t* values)
    {
        ++calls;
        std::fill(statuses, statuses + count, SAI_STATUS_SUCCESS);
        std::fill(values, values + count * statCount, 7);
        return SAI_STATUS_SUCCESS;
    };
    test_syncd::mockVidManagerObjectTypeQuery(SAI_OBJECT_TYPE_PORT);
    FlexCounter fc("conflicting_fragments", vendor, "COUNTERS_DB", true);
    fc.addCounterPlugin({{FLEX_COUNTER_STATUS_FIELD, "disable"}, {STATS_MODE_FIELD, STATS_MODE_READ}});
    const std::vector<swss::FieldValueTuple> original = {{PORT_COUNTER_ID_LIST, "SAI_PORT_STAT_IF_IN_OCTETS"}};
    fc.addCounter(vid, vid, original);
    ASSERT_EQ(fc.m_counterContext.size(), 1u);
    const auto context = fc.m_counterContext.begin()->second;
    // Inject inherited malformed state directly at the context API: ordinary
    // dispatch rejects this replacement, so it cannot create the bad state.
    {
        std::lock_guard<std::mutex> lock(fc.m_mtx);
        context->bulkAddObject({vid}, {objects.back()}, {"SAI_PORT_STAT_IF_IN_UCAST_PKTS"}, "");
    }
    const auto conflict = context->getObjectRids({vid});
    ASSERT_EQ(conflict.size(), 1u);
    EXPECT_FALSE(conflict.begin()->second.consistent);
    const auto previousCalls = calls;
    fc.addCounter(vid, vid, original);
    EXPECT_EQ(calls, previousCalls) << "Quarantine must run before further support probes";
    EXPECT_TRUE(fc.isEmpty()) << "All scalar and bulk fragments must be removed, not only the first match";
    EXPECT_TRUE(context->getObjectRids({vid}).empty());
    EXPECT_TRUE(fc.m_objectRegistrations.empty());
    // Once the malformed registration is gone, a normal fresh enrollment works.
    fc.addCounter(vid, vid, original);
    EXPECT_FALSE(fc.isEmpty());
    ASSERT_EQ(fc.m_objectRegistrations.size(), 1u);
    EXPECT_EQ(fc.m_objectRegistrations.begin()->second.rid, vid);
    fc.removeCounter(vid);
}

TEST(FlexCounter, pendingModeChangeWakesNextSweepWithoutIntervalUpdate)
{
    auto vendor = std::make_shared<MockableSaiInterface>();
    FlexCounterSdkGate gate;
    FlexCounter* counter = nullptr;
    std::atomic<uint32_t> clears{0};
    const auto objects = generateOids(2, SAI_OBJECT_TYPE_PORT);
    vendor->mock_queryStatsCapability = [](sai_object_id_t, sai_object_type_t,
            sai_stat_capability_list_t* capability)
    {
        if (capability->count < 1)
        {
            capability->count = 1;
            return SAI_STATUS_BUFFER_OVERFLOW;
        }
        capability->list[0].stat_enum = SAI_PORT_STAT_IF_IN_OCTETS;
        capability->list[0].stat_modes = SAI_STATS_MODE_READ | SAI_STATS_MODE_READ_AND_CLEAR;
        return SAI_STATUS_SUCCESS;
    };
    vendor->mock_getStats = [&](sai_object_type_t, sai_object_id_t oid,
            uint32_t count, const sai_stat_id_t*, uint64_t* values)
    {
        gate.read(oid, 1, counter && counter->m_pendingUpdates.load() != 0);
        std::fill(values, values + count, 1234);
        return SAI_STATUS_SUCCESS;
    };
    vendor->mock_clearStats = [&](sai_object_type_t, sai_object_id_t,
            uint32_t, const sai_stat_id_t*)
    {
        ++clears;
        return SAI_STATUS_SUCCESS;
    };
    vendor->mock_bulkGetStats = [](sai_object_id_t, sai_object_type_t, uint32_t,
            const sai_object_key_t*, uint32_t, const sai_stat_id_t*,
            sai_stats_mode_t, sai_status_t*, uint64_t*)
    {
        return SAI_STATUS_NOT_IMPLEMENTED;
    };

    test_syncd::mockVidManagerObjectTypeQuery(SAI_OBJECT_TYPE_PORT);
    FlexCounter fc("pending_mode_wake_up", vendor, "COUNTERS_DB");
    counter = &fc;
    fc.addCounterPlugin({
            {POLL_INTERVAL_FIELD, "60000"},
            {STATS_MODE_FIELD, STATS_MODE_READ},
    });
    swss::DBConnector db("COUNTERS_DB", 0);
    swss::Table table(&db, COUNTERS_TABLE);
    for (auto oid : objects)
    {
        fc.addCounter(oid, oid, {{PORT_COUNTER_ID_LIST, "SAI_PORT_STAT_IF_IN_OCTETS"}});
        table.del(toOid(oid));
    }
    clears = 0;
    gate.arm();
    fc.addCounterPlugin({{FLEX_COUNTER_STATUS_FIELD, "enable"}});
    if (!gate.waitForFirst())
    {
        gate.releaseAll();
        ADD_FAILURE() << "The first SDK read did not start";
        return;
    }

    auto updater = std::async(std::launch::async, [&]
    {
        fc.addCounterPlugin({{STATS_MODE_FIELD, STATS_MODE_READ_AND_CLEAR}});
    });
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (fc.m_pendingUpdates.load() == 0 && std::chrono::steady_clock::now() < deadline)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    EXPECT_NE(fc.m_pendingUpdates.load(), 0u);
    gate.releaseFirst();
    const bool updatedAtCheckpoint = updater.wait_for(std::chrono::seconds(2)) == std::future_status::ready;
    if (!updatedAtCheckpoint)
    {
        gate.releaseAll();
    }
    updater.get();
    EXPECT_TRUE(updatedAtCheckpoint);
    EXPECT_EQ(clears.load(), 0u) << "The original READ sweep changed mode mid-cycle";
    gate.releaseAll();

    // No further interval/status/configuration notification may force this
    // wake up: the mode update made at the checkpoint must survive the sweep.
    const bool nextSweepStarted = gate.waitForReadCount(3);
    fc.endFlexCounterThread();
    EXPECT_TRUE(nextSweepStarted) << "The checkpoint's mode notification was lost before sleeping";
    EXPECT_GT(clears.load(), 0u) << "The next sweep did not apply READ_AND_CLEAR";
    for (auto oid : objects)
    {
        fc.removeCounter(oid);
        table.del(toOid(oid));
    }
}

TEST(FlexCounter, pendingRemovalPreemptsScalarSweep)
{
    testPendingUpdatePreemptsSweep(false, false, FlexCounterPendingUpdate::Remove);
}

TEST(FlexCounter, pendingRemovalPreservesReadAndClearSample)
{
    testPendingUpdatePreemptsSweep(false, true, FlexCounterPendingUpdate::Remove);
}

TEST(FlexCounter, removalOfSampledObjectFlushesRawDeltaWithoutFinalAggregation)
{
    testPendingUpdatePreemptsSweep(false, true, FlexCounterPendingUpdate::RemoveCurrent);
}

TEST(FlexCounter, removalOfBulkSampledObjectFlushesRawDeltaWithoutFinalAggregation)
{
    testPendingUpdatePreemptsSweep(true, true, FlexCounterPendingUpdate::RemoveCurrent);
}

TEST(FlexCounter, pendingRemovalsDrainBeforeScalarSweepResumes)
{
    testPendingUpdatePreemptsSweep(false, false, FlexCounterPendingUpdate::RemoveMultiple);
}

TEST(FlexCounter, pendingRemovalPreemptsBulkChunks)
{
    testPendingUpdatePreemptsSweep(true, false, FlexCounterPendingUpdate::Remove);
}

TEST(FlexCounter, pendingRemovalWaitsForIndivisibleUnchunkedBulkCall)
{
    testPendingUpdatePreemptsSweep(true, true, FlexCounterPendingUpdate::Remove, true);
}

TEST(FlexCounter, bulkOverallSuccessDoesNotRequireObjectStatuses)
{
    testBulkResultContract(FlexCounterBulkResult::SuccessWithoutObjectStatuses);
}

TEST(FlexCounter, bulkOverallFailureWithoutObjectStatusesPreservesPreviousValues)
{
    testBulkResultContract(FlexCounterBulkResult::FailureWithoutObjectStatuses);
}

TEST(FlexCounter, bulkOverallFailurePublishesOnlySuccessfulObjects)
{
    testBulkResultContract(FlexCounterBulkResult::PartialFailure);
}

TEST(FlexCounter, bulkPartialFailureSkipsUntouchedObjectStatus)
{
    testBulkResultContract(FlexCounterBulkResult::PartialFailureWithUntouchedObject);
}

TEST(FlexCounter, pendingRemovalPreservesBulkReadAndClearSample)
{
    testPendingUpdatePreemptsSweep(true, true, FlexCounterPendingUpdate::Remove);
}

TEST(FlexCounter, pendingEnrollmentExcludesUnsampledPluginKey)
{
    testPendingUpdatePreemptsSweep(false, true, FlexCounterPendingUpdate::Add);
}

TEST(FlexCounter, pendingReenrollmentInvalidatesClearedPluginSample)
{
    testPendingUpdatePreemptsSweep(false, true, FlexCounterPendingUpdate::Reenroll);
}

TEST(FlexCounter, pendingBulkEnrollmentExcludesUnsampledPluginKey)
{
    testPendingUpdatePreemptsSweep(true, true, FlexCounterPendingUpdate::Add);
}

TEST(FlexCounter, pendingBulkReenrollmentInvalidatesClearedPluginSample)
{
    testPendingUpdatePreemptsSweep(true, true, FlexCounterPendingUpdate::Reenroll);
}

TEST(FlexCounter, pendingRemovalAndSameRidReaddInvalidatesClearedSample)
{
    testPendingUpdatePreemptsSweep(false, true, FlexCounterPendingUpdate::RemoveAndReadd);
}

TEST(FlexCounter, pendingAttributeUpdatePreservesQueueCounterSample)
{
    testPendingUpdatePreemptsSweep(false, true, FlexCounterPendingUpdate::UpdateAttribute);
}

TEST(FlexCounter, pendingHybridEnrollmentWaitsForWholeNextSweep)
{
    testPendingUpdatePreemptsSweep(true, true, FlexCounterPendingUpdate::AddHybrid);
}

TEST(FlexCounter, pendingHybridRefreshPreservesCompleteReadAndClearDelta)
{
    testPendingUpdatePreemptsSweep(true, true, FlexCounterPendingUpdate::RefreshHybrid);
}

TEST(FlexCounter, pendingHybridFailedReadsDoNotPublishStalePluginSample)
{
    testPendingUpdatePreemptsSweep(true, true, FlexCounterPendingUpdate::FailedHybrid);
}

TEST(FlexCounter, pendingHybridScalarFailurePreservesBaselineFragmentSemantics)
{
    testPendingUpdatePreemptsSweep(true, true, FlexCounterPendingUpdate::FailedHybridScalar);
}

TEST(FlexCounter, pendingHybridBulkFailurePreservesBaselineFragmentSemantics)
{
    testPendingUpdatePreemptsSweep(true, true, FlexCounterPendingUpdate::FailedHybridBulk);
}

TEST(FlexCounter, pendingCounterListRefreshPreservesScalarDelta)
{
    testPendingUpdatePreemptsSweep(false, true, FlexCounterPendingUpdate::Refresh);
}

TEST(FlexCounter, pendingCounterListRefreshPreservesBulkDelta)
{
    testPendingUpdatePreemptsSweep(true, true, FlexCounterPendingUpdate::Refresh);
}

TEST(FlexCounter, pendingModeChangeAppliesToNextScalarSweep)
{
    testPendingUpdatePreemptsSweep(false, true, FlexCounterPendingUpdate::SetStatsMode);
}

TEST(FlexCounter, pendingModeChangeAppliesToNextBulkSweep)
{
    testPendingUpdatePreemptsSweep(true, true, FlexCounterPendingUpdate::SetStatsMode);
}

TEST(FlexCounter, pendingDisablePreemptsScalarSweep)
{
    testPendingUpdatePreemptsSweep(false, false, FlexCounterPendingUpdate::Disable);
}

TEST(FlexCounter, pendingShutdownPreemptsScalarSweep)
{
    testPendingUpdatePreemptsSweep(false, true, FlexCounterPendingUpdate::Shutdown);
}

TEST(FlexCounter, pendingRemovalPreemptsAttributeSweep)
{
    testPendingRemovalPreemptsAttributeSweep(false);
}

TEST(FlexCounter, pendingRemovalFlushesPhyPipelineBeforeYield)
{
    testPendingRemovalPreemptsAttributeSweep(true);
}

TEST(FlexCounter, pendingAttributeBulkGetFiltersOriginalPassAndPublishesPlugin)
{
    testPendingAttributeBulkAdmission(false);
}

TEST(FlexCounter, pendingAttributeBulkNotSupportedLatchesCheckpointedFallback)
{
    testPendingAttributeBulkAdmission(true);
}

TEST(FlexCounter, pendingPluginArgumentsKeepSixPositionsAndFrozenCycleConfiguration)
{
    auto vendor = std::make_shared<MockableSaiInterface>();
    FlexCounterSdkGate gate;
    FlexCounter* counter = nullptr;
    const auto objects = generateOids(2, SAI_OBJECT_TYPE_PORT);
    vendor->mock_queryStatsCapability = [](sai_object_id_t, sai_object_type_t,
            sai_stat_capability_list_t* capability)
    {
        if (capability->count < 1)
        {
            capability->count = 1;
            return SAI_STATUS_BUFFER_OVERFLOW;
        }
        capability->list[0].stat_enum = SAI_PORT_STAT_IF_IN_OCTETS;
        capability->list[0].stat_modes = SAI_STATS_MODE_READ;
        return SAI_STATUS_SUCCESS;
    };
    vendor->mock_getStats = [&](sai_object_type_t, sai_object_id_t rid,
            uint32_t count, const sai_stat_id_t*, uint64_t* counters)
    {
        gate.read(rid, 1, counter && counter->m_pendingUpdates.load() != 0);
        std::fill(counters, counters + count, 17);
        return SAI_STATUS_SUCCESS;
    };
    vendor->mock_bulkGetStats = [](sai_object_id_t, sai_object_type_t, uint32_t,
            const sai_object_key_t*, uint32_t, const sai_stat_id_t*,
            sai_stats_mode_t, sai_status_t*, uint64_t*)
    {
        return SAI_STATUS_NOT_SUPPORTED;
    };
    test_syncd::mockVidManagerObjectTypeQuery(SAI_OBJECT_TYPE_PORT);
    FlexCounter fc("pending_plugin_cycle_arguments", vendor, "COUNTERS_DB");
    counter = &fc;
    swss::DBConnector db("COUNTERS_DB", 0);
    swss::Table table(&db, COUNTERS_TABLE);
    const std::string pluginKey = "pending_plugin_cycle_arguments";
    table.del(pluginKey);
    const auto sha = swss::loadRedisScript(&db,
            "redis.call('SELECT',ARGV[1]); local result=ARGV[2]..':" + pluginKey +
            "'; local n=redis.call('HINCRBY',result,'invocations',1); "
            "if n<=2 then redis.call('HSET',result,n..':count',#ARGV); "
            "for i,arg in ipairs(ARGV) do redis.call('HSET',result,n..':'..i,arg); end; end; return {}");
    fc.addCounterPlugin({{POLL_INTERVAL_FIELD, "60000"}, {SECONDARY_POLL_FACTOR_FIELD, "0"},
            {FLEX_COUNTER_STATUS_FIELD, "disable"}, {STATS_MODE_FIELD, STATS_MODE_READ}, {PORT_PLUGIN_FIELD, sha}});
    for (const auto oid : objects)
    {
        fc.addCounter(oid, oid, {{PORT_COUNTER_ID_LIST, "SAI_PORT_STAT_IF_IN_OCTETS"}});
        table.del(toOid(oid));
    }
    gate.arm();
    fc.addCounterPlugin({{FLEX_COUNTER_STATUS_FIELD, "enable"}});
    if (!gate.waitForFirst())
    {
        gate.releaseAll();
        fc.endFlexCounterThread();
        ADD_FAILURE() << "The plugin's first logical sweep did not start";
        return;
    }
    auto updater = std::async(std::launch::async, [&]
    {
        fc.addCounterPlugin({{POLL_INTERVAL_FIELD, "30000"}, {SECONDARY_POLL_FACTOR_FIELD, "7"}});
    });
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (fc.m_pendingUpdates.load() == 0 && std::chrono::steady_clock::now() < deadline)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    EXPECT_NE(fc.m_pendingUpdates.load(), 0u);
    gate.releaseFirst();
    const bool updated = updater.wait_for(std::chrono::seconds(2)) == std::future_status::ready;
    if (!updated)
    {
        gate.releaseAll();
    }
    updater.get();
    EXPECT_TRUE(updated);
    EXPECT_TRUE(gate.waitForReadCount(2));
    gate.releaseAll();
    // runRedisScript appends its pre-existing literal "''" sentinel after the
    // six caller arguments; it is not another FlexCounter plugin parameter.
    waitForCounterValues(table, pluginKey, {"invocations", "1:count", "2:count", "1:7", "2:7"},
            {"2", "7", "7", "''", "''"});
    fc.endFlexCounterThread();
    waitForCounterValues(table, pluginKey, {"1:1", "1:2", "1:3", "1:4", "2:3", "2:4"},
            {std::to_string(db.getDbId()), COUNTERS_TABLE, "60000", "0", "30000", "7"});
    std::string firstStart, firstCollect, secondStart, secondCollect;
    EXPECT_TRUE(table.hget(pluginKey, "1:5", firstStart));
    EXPECT_TRUE(table.hget(pluginKey, "1:6", firstCollect));
    EXPECT_TRUE(table.hget(pluginKey, "2:5", secondStart));
    EXPECT_TRUE(table.hget(pluginKey, "2:6", secondCollect));
    EXPECT_GT(std::stoll(firstStart), 0);
    EXPECT_GT(std::stoll(firstCollect), 0);
    EXPECT_GE(std::stoll(secondStart), std::stoll(firstStart) + std::stoll(firstCollect));
    EXPECT_GE(std::stoll(secondCollect), 0);
    for (const auto oid : objects)
    {
        fc.removeCounter(oid);
        table.del(toOid(oid));
    }
    table.del(pluginKey);
    table.del("TIME_STAMP");
}

TEST(FlexCounter, attributeRidReplacementRequiresExplicitRemoval)
{
    testSpecializedReplacementRequiresRemoval(FlexCounterSpecializedContext::Attribute);
}

TEST(FlexCounter, phyRidReplacementRequiresExplicitRemoval)
{
    testSpecializedReplacementRequiresRemoval(FlexCounterSpecializedContext::Phy);
}

TEST(FlexCounter, serdesRidReplacementRequiresExplicitRemoval)
{
    testSpecializedReplacementRequiresRemoval(FlexCounterSpecializedContext::Serdes);
}

TEST(FlexCounter, perPortDiscoveryRidReplacementRequiresExplicitRemoval)
{
    testSpecializedReplacementRequiresRemoval(FlexCounterSpecializedContext::PerPortDiscovery);
}

TEST(FlexCounter, sameRidRefreshRetainsEpochUntilExplicitRemoval)
{
    FlexCounterBulkReplacementFixture fixture;
    const auto vid = fixture.objects.front();
    fixture.counter->addCounter(vid, vid, fixture.values);
    const auto initial = fixture.registration(vid);
    EXPECT_GT(initial.epoch, 0u);

    fixture.counter->addCounter(vid, vid, fixture.values);
    EXPECT_EQ(fixture.registration(vid).epoch, initial.epoch)
        << "Scalar same-RID refresh created a new registration lifetime";
    fixture.enroll({vid}, {vid});
    EXPECT_EQ(fixture.registration(vid).epoch, initial.epoch)
        << "Bulk same-RID refresh created a new registration lifetime";
    EXPECT_EQ(fixture.registration(vid).rid, vid);

    fixture.counter->removeCounter(vid);
    EXPECT_TRUE(fixture.actual({vid}).empty());
    EXPECT_TRUE(fixture.counter->m_objectRegistrations.empty());
    fixture.enroll({vid}, {vid});
    const auto reenrolled = fixture.registration(vid);
    EXPECT_EQ(reenrolled.rid, vid);
    EXPECT_GT(reenrolled.epoch, initial.epoch)
        << "Same-RID re-enrollment reused the deleted registration's epoch";
}

TEST(FlexCounter, bulkRidReplacementUsesScalarFallbackEnrollment)
{
    FlexCounterBulkReplacementFixture fixture;
    const auto vid = fixture.objects[0], replacement = fixture.objects[1];
    fixture.enroll({vid}, {vid});
    const auto original = fixture.registration(vid);
    fixture.enroll({vid}, {replacement});
    const auto actual = fixture.actual({vid});
    ASSERT_EQ(actual.size(), 1u);
    EXPECT_TRUE(actual.at(vid).consistent);
    EXPECT_EQ(actual.at(vid).rid, replacement);
    const auto replaced = fixture.registration(vid);
    EXPECT_EQ(replaced.rid, replacement);
    EXPECT_NE(replaced.epoch, original.epoch);
    fixture.setHardware(replacement, 333);
    fixture.gate.arm();
    fixture.counter->addCounterPlugin({{FLEX_COUNTER_STATUS_FIELD, "enable"}});
    const bool started = fixture.gate.waitForFirst();
    fixture.gate.releaseAll();
    EXPECT_TRUE(started);
    waitForCounterValues(fixture.table, toOid(vid), {"SAI_QUEUE_STAT_PACKETS"}, {"333"});
    fixture.counter->endFlexCounterThread();
    EXPECT_EQ(fixture.gate.reads(), std::vector<sai_object_id_t>{replacement});
}

TEST(FlexCounter, bulkRidReplacementRetiresExistingBulkFragment)
{
    FlexCounterBulkReplacementFixture fixture(true);
    const auto vid = fixture.objects[0], replacement = fixture.objects[1];
    fixture.enroll({vid}, {vid});
    const auto original = fixture.registration(vid);
    fixture.enroll({vid}, {replacement});
    const auto actual = fixture.actual({vid});
    ASSERT_EQ(actual.size(), 1u);
    EXPECT_TRUE(actual.at(vid).consistent) << "Old and new physical RID fragments coexist";
    EXPECT_EQ(actual.at(vid).rid, replacement);
    EXPECT_EQ(fixture.registration(vid).rid, replacement);
    EXPECT_NE(fixture.registration(vid).epoch, original.epoch);
    fixture.setHardware(replacement, 444);
    fixture.gate.arm();
    fixture.counter->addCounterPlugin({{FLEX_COUNTER_STATUS_FIELD, "enable"}});
    const bool started = fixture.gate.waitForFirst();
    fixture.gate.releaseAll();
    EXPECT_TRUE(started);
    waitForCounterValues(fixture.table, toOid(vid), {"SAI_QUEUE_STAT_PACKETS"}, {"444"});
    fixture.counter->endFlexCounterThread();
    EXPECT_EQ(fixture.gate.reads(), std::vector<sai_object_id_t>{replacement});
}

TEST(FlexCounter, rejectedBulkRidReplacementPreservesLiveIdentityAndPendingDelta)
{
    testRejectedBulkReplacementPreservesDelta(false);
}

TEST(FlexCounter, throwingBulkRidReplacementPreservesLiveIdentityAndPendingDelta)
{
    testRejectedBulkReplacementPreservesDelta(true);
}

TEST(FlexCounter, bulkRidReplacementExceptionAfterRemovalErasesObsoleteRegistration)
{
    FlexCounterBulkReplacementFixture fixture(true);
    const auto vid = fixture.objects[0], replacement = fixture.objects[1];
    fixture.enroll({vid}, {vid});
    fixture.throwAfterRemovalRid = replacement;
    EXPECT_THROW(fixture.enroll({vid}, {replacement}), std::runtime_error);
    EXPECT_TRUE(fixture.actual({vid}).empty());
    EXPECT_TRUE(fixture.counter->m_objectRegistrations.empty());
    EXPECT_TRUE(fixture.counter->isEmpty());
    EXPECT_EQ(fixture.counter->m_pendingUpdates.load(), 0u);
    EXPECT_EQ(fixture.counter->m_deferredUpdates, 0u);
    fixture.throwAfterRemovalRid = SAI_NULL_OBJECT_ID;
    fixture.enroll({vid}, {replacement});
    EXPECT_EQ(fixture.actual({vid}).at(vid).rid, replacement);
    EXPECT_EQ(fixture.registration(vid).rid, replacement);
}

TEST(FlexCounter, mixedBulkRidReplacementKeepsFreshAndSameRidMembersBatched)
{
    FlexCounterBulkReplacementFixture fixture(true);
    const auto changed = fixture.objects[0], same = fixture.objects[1];
    const auto fresh = fixture.objects[2], replacement = fixture.objects[3];
    fixture.enroll({changed, same}, {changed, same});
    const auto changedBefore = fixture.registration(changed), sameBefore = fixture.registration(same);
    fixture.bulkCalls.clear();
    fixture.enroll({fresh, same, changed}, {fresh, same, replacement});
    const auto actual = fixture.actual({fresh, same, changed});
    ASSERT_EQ(actual.size(), 3u);
    for (const auto& object : actual)
    {
        EXPECT_TRUE(object.second.consistent);
        EXPECT_EQ(fixture.registration(object.first).rid, object.second.rid);
    }
    EXPECT_EQ(actual.at(changed).rid, replacement);
    EXPECT_EQ(actual.at(same).rid, same);
    EXPECT_EQ(actual.at(fresh).rid, fresh);
    EXPECT_NE(fixture.registration(changed).epoch, changedBefore.epoch);
    EXPECT_EQ(fixture.registration(same).epoch, sameBefore.epoch);
    EXPECT_NE(std::find(fixture.bulkCalls.begin(), fixture.bulkCalls.end(),
                std::vector<sai_object_id_t>{fresh, same}), fixture.bulkCalls.end())
        << "Fresh and same-RID members were unnecessarily converted to per-object enrollment";
    EXPECT_EQ(std::find(fixture.bulkCalls.begin(), fixture.bulkCalls.end(),
                std::vector<sai_object_id_t>{fresh, same, replacement}), fixture.bulkCalls.end())
        << "The changed-RID member reached the old duplicate-retaining bulk path";
}

TEST(FlexCounter, mixedBulkRidReplacementExceptionReconcilesAttemptedMembersOnly)
{
    FlexCounterBulkReplacementFixture fixture;
    const auto changed = fixture.objects[0], removed = fixture.objects[1], same = fixture.objects[2];
    const auto fresh = fixture.objects[3], replacement = fixture.objects[4], throwing = fixture.objects[5];
    const auto unattempted = fixture.objects[6], laterReplacement = fixture.objects[7];
    fixture.enroll({changed, removed, same, unattempted}, {changed, removed, same, unattempted});
    const auto changedBefore = fixture.registration(changed), sameBefore = fixture.registration(same);
    const auto untouchedBefore = fixture.registration(unattempted);
    fixture.throwAfterRemovalRid = throwing;
    EXPECT_THROW(fixture.enroll({fresh, same, changed, removed, unattempted},
                {fresh, same, replacement, throwing, laterReplacement}), std::runtime_error);
    const auto actual = fixture.actual({fresh, same, changed, removed, unattempted});
    ASSERT_EQ(actual.size(), 4u);
    EXPECT_EQ(actual.count(removed), 0u);
    EXPECT_EQ(actual.at(changed).rid, replacement);
    EXPECT_EQ(actual.at(fresh).rid, fresh);
    EXPECT_EQ(actual.at(same).rid, same);
    EXPECT_EQ(actual.at(unattempted).rid, unattempted);
    ASSERT_EQ(fixture.counter->m_objectRegistrations.size(), actual.size());
    for (const auto& object : actual)
    {
        EXPECT_TRUE(object.second.consistent);
        EXPECT_EQ(fixture.registration(object.first).rid, object.second.rid);
    }
    EXPECT_NE(fixture.registration(changed).epoch, changedBefore.epoch);
    EXPECT_EQ(fixture.registration(same).epoch, sameBefore.epoch);
    const auto untouchedAfter = fixture.registration(unattempted);
    EXPECT_EQ(untouchedAfter.rid, untouchedBefore.rid);
    EXPECT_EQ(untouchedAfter.epoch, untouchedBefore.epoch);
    EXPECT_EQ(fixture.counter->m_pendingUpdates.load(), 0u);
    EXPECT_EQ(fixture.counter->m_deferredUpdates, 0u);
}

TEST(FlexCounter, bulkRidReplacementRepairsAcceptedNullPhysicalRid)
{
    FlexCounterBulkReplacementFixture fixture;
    const auto vid = fixture.objects[0], replacement = fixture.objects[1];
    fixture.counter->addCounter(vid, SAI_NULL_OBJECT_ID, fixture.values);
    const auto actualBefore = fixture.actual({vid});
    ASSERT_EQ(actualBefore.size(), 1u) << "The fixture must actually enroll the initial null SDK key";
    ASSERT_EQ(actualBefore.at(vid).rid, SAI_NULL_OBJECT_ID);
    const auto original = fixture.registration(vid);
    fixture.enroll({vid}, {replacement});
    const auto actual = fixture.actual({vid});
    ASSERT_EQ(actual.size(), 1u);
    EXPECT_TRUE(actual.at(vid).consistent);
    EXPECT_EQ(actual.at(vid).rid, replacement);
    EXPECT_EQ(fixture.registration(vid).rid, replacement);
    EXPECT_NE(fixture.registration(vid).epoch, original.epoch);
}

TEST(FlexCounter, dashMeterRidReplacementRequiresExplicitRemoval)
{
    testSpecializedReplacementRequiresRemoval(FlexCounterSpecializedContext::DashMeter);
}

TEST(FlexCounter, pendingScalarRefreshPreservesDeltaBeforeDestructiveProbe)
{
    testDestructiveRefreshPreservesSamples(false, false);
}

TEST(FlexCounter, rejectedReplacementPreservesLiveIdentityAndPendingDelta)
{
    testDestructiveRefreshPreservesSamples(false, false, false, FlexCounterRefreshEnd::Complete,
            false, false, FlexCounterRejectedReplacement::Scalar);
}

TEST(FlexCounter, throwingReplacementPreservesLiveIdentityAndPendingDelta)
{
    testDestructiveRefreshPreservesSamples(false, false, false, FlexCounterRefreshEnd::Complete,
            false, false, FlexCounterRejectedReplacement::ThrowingScalar);
}

TEST(FlexCounter, partialBatchAcceptancePreservesRejectedLiveIdentityAndDelta)
{
    testDestructiveRefreshPreservesSamples(false, false, false, FlexCounterRefreshEnd::Complete,
            false, false, FlexCounterRejectedReplacement::PartialBulk);
}

TEST(FlexCounter, partialBatchExceptionTracksAcceptedMemberWithoutInventingFailedMember)
{
    testDestructiveRefreshPreservesSamples(false, false, false, FlexCounterRefreshEnd::Complete,
            false, false, FlexCounterRejectedReplacement::ThrowingPartialBulk);
}

TEST(FlexCounter, pendingBulkRefreshPreservesDeltaBeforeDestructiveProbe)
{
    testDestructiveRefreshPreservesSamples(true, false);
}

TEST(FlexCounter, pendingHybridRefreshPreservesUnreadSubsetBeforeDestructiveProbe)
{
    testDestructiveRefreshPreservesSamples(true, true);
}

TEST(FlexCounter, pendingDeferredRefreshDoesNotBlockRemoval)
{
    testDestructiveRefreshPreservesSamples(false, false, true);
}

TEST(FlexCounter, pendingDeferredRefreshDoesNotBlockShutdown)
{
    testDestructiveRefreshPreservesSamples(false, false, false, FlexCounterRefreshEnd::Shutdown);
}

TEST(FlexCounter, pendingDeferredRefreshReleasedAfterPollException)
{
    testDestructiveRefreshPreservesSamples(false, false, false, FlexCounterRefreshEnd::PollException);
}

TEST(FlexCounter, pendingDeferredRefreshBalancesThrowingUpdater)
{
    testDestructiveRefreshPreservesSamples(false, false, false, FlexCounterRefreshEnd::UpdateException);
}

TEST(FlexCounter, pendingMixedBulkRefreshWaitsForOriginalPlugin)
{
    testDestructiveRefreshPreservesSamples(true, false, false, FlexCounterRefreshEnd::Complete, true);
}

TEST(FlexCounter, pendingPluginRemovalPreservesClearedOldSweep)
{
    testDestructiveRefreshPreservesSamples(false, false, false, FlexCounterRefreshEnd::RemovePlugins);
}

TEST(FlexCounter, pendingPluginEnrollmentStartsWithNextSweep)
{
    testDestructiveRefreshPreservesSamples(false, false, false, FlexCounterRefreshEnd::AddPlugin);
}

TEST(FlexCounter, pendingPerPortDiscoveryRefreshPreservesDeltaBeforeDestructiveProbe)
{
    testDestructiveRefreshPreservesSamples(false, false, false, FlexCounterRefreshEnd::Complete, false, true);
}
