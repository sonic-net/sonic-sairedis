#include "FlexCounter.h"
#include "sai_serialize.h"
#include "MockableSaiInterface.h"
#include "MockHelper.h"
#include "VirtualObjectIdManager.h"
#include "NumberOidIndexGenerator.h"
#include <string>
#include <gtest/gtest.h>

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
        bool forceSingleCreate = false)
{
    SWSS_LOG_ENTER();

    FlexCounter fc("test", sai, "COUNTERS_DB");

    test_syncd::mockVidManagerObjectTypeQuery(object_type);

    std::vector<sai_object_id_t> object_ids = generateOids(numOid, object_type);
    EXPECT_EQ(object_ids.size(), numOid);

    std::vector<swss::FieldValueTuple> values;
    values.emplace_back(POLL_INTERVAL_FIELD, "1000");
    values.emplace_back(FLEX_COUNTER_STATUS_FIELD, "enable");
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

    EXPECT_EQ(fc.isEmpty(), false);

    usleep(1000*1050);
    swss::DBConnector db("COUNTERS_DB", 0);
    swss::RedisPipeline pipeline(&db);
    swss::Table countersTable(&pipeline, COUNTERS_TABLE, false);

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
        false);

    // Bulk create mode to satisfy the coverage requirement
    testAddRemoveCounter(
        1,
        SAI_OBJECT_TYPE_ACL_COUNTER,
        ACL_COUNTER_ATTR_ID_LIST,
        {"SAI_ACL_COUNTER_ATTR_PACKETS"},
        {"1000"},
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

    sai->mock_getStats = [](sai_object_type_t, sai_object_id_t, uint32_t number_of_counters, const sai_stat_id_t *, uint64_t *counters) {
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

    auto counterVerifyFunc = [] (swss::Table &countersTable, const std::string& key, const std::vector<std::string>& counterIdNames, const std::vector<std::string>& expectedValues)
    {
        std::string value;
        countersTable.hget(key, "SAI_PORT_STAT_IF_IN_OCTETS", value);
        EXPECT_EQ(value, "1000");
        // SAI_PORT_STAT_IF_IN_UCAST_PKTS is not supported, shall not in countersTable
        bool ret = countersTable.hget(key, "SAI_PORT_STAT_IF_IN_UCAST_PKTS", value);
        EXPECT_EQ(false, ret);
    };

    testAddRemoveCounter(
        1,
        SAI_OBJECT_TYPE_PORT,
        PORT_COUNTER_ID_LIST,
        {"SAI_PORT_STAT_IF_IN_OCTETS", "SAI_PORT_STAT_IF_IN_UCAST_PKTS"},
        {},
        counterVerifyFunc,
        false);
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
                            WRED_PORT_PLUGIN_FIELD};
    for (auto &field : fields)
    {
        testAddRemovePlugin(field);
    }
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

    usleep(1000*1000);
    swss::DBConnector db("COUNTERS_DB", 0);
    swss::RedisPipeline pipeline(&db);
    swss::Table countersTable(&pipeline, COUNTERS_TABLE, false);

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

    usleep(1000*2000);
    countersTable.hget(expectedKey, "SAI_PORT_STAT_IF_IN_OCTETS", value);
    EXPECT_EQ(value, "100");
    countersTable.hget(expectedKey, "SAI_PORT_STAT_IF_IN_ERRORS", value);
    EXPECT_EQ(value, "200");

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
        false);

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
    int32_t partialSupportingBulkObjectFactor;
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
                if (unifiedBulkChunkSize > 0)
                {
                    if (object_count != unifiedBulkChunkSize)
                    {
                        EXPECT_EQ(object_count, unifiedBulkChunkSize);
                    }
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

    // set bulk chunk size + per counter bulk chunk size first and then create ports
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
        true);
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
    auto counterVerifyFunc = [] (swss::Table &countersTable, const std::string& key, const std::vector<std::string>& counterIdNames, const std::vector<std::string>& expectedValues)
    {
        std::string value;
        for (size_t i = 0; i < counterIdNames.size(); i++)
        {
            countersTable.hget(key, counterIdNames[i], value);
            ASSERT_EQ(value, expectedValues[i]);
        }
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

    usleep(1000*1050);
    swss::DBConnector db("COUNTERS_DB", 0);
    swss::RedisPipeline pipeline(&db);
    swss::Table countersTable(&pipeline, COUNTERS_TABLE, false);

    std::vector<std::string> keys;
    countersTable.getKeys(keys);
    EXPECT_EQ(keys.size(),1);
    std::string expectedKey = toOid(oid);
    counterVerifyFunc(countersTable,
                      expectedKey,
                      {"SAI_PORT_STAT_IF_IN_NON_UCAST_PKTS", "SAI_PORT_STAT_IF_IN_DISCARDS"},
                      {"10", "20"});

    // not support bulk to support bulk
    values.clear();
    values.emplace_back(PORT_COUNTER_ID_LIST, "SAI_PORT_STAT_IF_IN_OCTETS,SAI_PORT_STAT_IF_IN_UCAST_PKTS");
    fc.addCounter(oid, oid, values);

    usleep(1000*1050);
    counterVerifyFunc(countersTable,
                      expectedKey,
                      {"SAI_PORT_STAT_IF_IN_OCTETS", "SAI_PORT_STAT_IF_IN_UCAST_PKTS"},
                      {"100", "200"});

    // support bulk but counter id changes
    values.clear();
    values.emplace_back(PORT_COUNTER_ID_LIST, "SAI_PORT_STAT_IF_IN_OCTETS");
    fc.addCounter(oid, oid, values);

    usleep(1000*1050);
    counterVerifyFunc(countersTable,
                      expectedKey,
                      {"SAI_PORT_STAT_IF_IN_OCTETS"},
                      {"100"});

    // support bulk with different counter id
    sai_object_id_t oid1{0x1000000000001};
    values.clear();
    values.emplace_back(PORT_COUNTER_ID_LIST, "SAI_PORT_STAT_IF_IN_OCTETS,SAI_PORT_STAT_IF_IN_UCAST_PKTS");
    fc.addCounter(oid1, oid1, values);

    usleep(1000*1050);
    counterVerifyFunc(countersTable,
                      toOid(oid1),
                      {"SAI_PORT_STAT_IF_IN_OCTETS", "SAI_PORT_STAT_IF_IN_UCAST_PKTS"},
                      {"100", "200"});

    // support bulk to not support bulk
    values.clear();
    values.emplace_back(PORT_COUNTER_ID_LIST, "SAI_PORT_STAT_IF_IN_NON_UCAST_PKTS,SAI_PORT_STAT_IF_IN_UCAST_PKTS");
    fc.addCounter(oid, oid, values);

    usleep(1000*1050);
    counterVerifyFunc(countersTable,
                      expectedKey,
                      {"SAI_PORT_STAT_IF_IN_NON_UCAST_PKTS", "SAI_PORT_STAT_IF_IN_UCAST_PKTS"},
                      {"10", "20"});

    // not support bulk but counter id changes
    values.clear();
    values.emplace_back(PORT_COUNTER_ID_LIST, "SAI_PORT_STAT_IF_IN_NON_UCAST_PKTS,SAI_PORT_STAT_IF_IN_DISCARDS");
    fc.addCounter(oid, oid, values);
    usleep(1000*1050);
    counterVerifyFunc(countersTable,
                      expectedKey,
                      {"SAI_PORT_STAT_IF_IN_NON_UCAST_PKTS", "SAI_PORT_STAT_IF_IN_DISCARDS"},
                      {"10", "20"});

    // verify oid1 is still using bulk
    counterVerifyFunc(countersTable,
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

        usleep(1000*1050);
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
        for (uint32_t i = 0; i < (expectedValues.size()/counterIdNames.size()); i++)
        {
            auto entry_key = sai_meter_bucket_entry_t {.switch_id = 0, .eni_id = eni_id,
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
