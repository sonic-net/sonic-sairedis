#include "FlexCounter.h"
#include "MockableSaiInterface.h"
#include "MockHelper.h"

#include <gtest/gtest.h>


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

void testAddRemoveCounter(
        sai_object_id_t object_id,
        sai_object_type_t object_type,
        const std::string& counterIdFieldName,
        const std::vector<std::string>& counterIdNames,
        const std::vector<std::string>& expectedValues,
        VerifyStatsFunc verifyFunc,
        bool autoRemoveDbEntry,
        const std::string statsMode = STATS_MODE_READ)
{
    SWSS_LOG_ENTER();

    FlexCounter fc("test", sai, "COUNTERS_DB");

    test_syncd::mockVidManagerObjectTypeQuery(object_type);

    std::vector<swss::FieldValueTuple> values;
    values.emplace_back(POLL_INTERVAL_FIELD, "1000");
    fc.addCounterPlugin(values);

    values.clear();
    values.emplace_back(FLEX_COUNTER_STATUS_FIELD, "enable");
    fc.addCounterPlugin(values);

    values.clear();
    values.emplace_back(STATS_MODE_FIELD, statsMode);
    fc.addCounterPlugin(values);

    values.clear();
    values.emplace_back(counterIdFieldName, join(counterIdNames));
    fc.addCounter(object_id, object_id, values);
    EXPECT_EQ(fc.isEmpty(), false);

    usleep(1000*1050);
    swss::DBConnector db("COUNTERS_DB", 0);
    swss::RedisPipeline pipeline(&db);
    swss::Table countersTable(&pipeline, COUNTERS_TABLE, false);

    std::string expectedKey = toOid(object_id);
    std::vector<std::string> keys;
    countersTable.getKeys(keys);
    EXPECT_EQ(keys.size(), size_t(1));
    EXPECT_EQ(keys[0], expectedKey);

    verifyFunc(countersTable, expectedKey, counterIdNames, expectedValues);

    fc.removeCounter(object_id);
    EXPECT_EQ(fc.isEmpty(), true);

    if (!autoRemoveDbEntry)
    {
        countersTable.del(expectedKey);
    }

    countersTable.getKeys(keys);
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
        sai_object_id_t(0x54000000000000),
        SAI_OBJECT_TYPE_COUNTER,
        FLOW_COUNTER_ID_LIST,
        {"SAI_COUNTER_STAT_PACKETS", "SAI_COUNTER_STAT_BYTES"},
        {"100", "200"},
        counterVerifyFunc,
        true);

    testAddRemoveCounter(
        sai_object_id_t(0x5a000000000000),
        SAI_OBJECT_TYPE_MACSEC_FLOW,
        MACSEC_FLOW_COUNTER_ID_LIST,
        {"SAI_MACSEC_FLOW_STAT_CONTROL_PKTS", "SAI_MACSEC_FLOW_STAT_PKTS_UNTAGGED"},
        {"100", "200"},
        counterVerifyFunc,
        false);

    testAddRemoveCounter(
        sai_object_id_t(0x5c000000000000),
        SAI_OBJECT_TYPE_MACSEC_SA,
        MACSEC_SA_COUNTER_ID_LIST,
        {"SAI_MACSEC_SA_STAT_OCTETS_ENCRYPTED", "SAI_MACSEC_SA_STAT_OCTETS_PROTECTED"},
        {"100", "200"},
        counterVerifyFunc,
        false);

    testAddRemoveCounter(
        sai_object_id_t(0x1000000000000),
        SAI_OBJECT_TYPE_PORT,
        PORT_COUNTER_ID_LIST,
        {"SAI_PORT_STAT_IF_IN_OCTETS", "SAI_PORT_STAT_IF_IN_UCAST_PKTS"},
        {"100", "200"},
        counterVerifyFunc,
        false);

    testAddRemoveCounter(
        sai_object_id_t(0x1000000000000),
        SAI_OBJECT_TYPE_PORT,
        PORT_DEBUG_COUNTER_ID_LIST,
        {"SAI_PORT_STAT_IN_CONFIGURED_DROP_REASONS_0_DROPPED_PKTS", "SAI_PORT_STAT_IN_CONFIGURED_DROP_REASONS_1_DROPPED_PKTS"},
        {"100", "200"},
        counterVerifyFunc,
        false);

    bool clearCalled = false;
    sai->mock_clearStats = [&] (sai_object_type_t object_type, sai_object_id_t object_id, uint32_t number_of_counters, const sai_stat_id_t *counter_ids) {
        clearCalled = true;
        return SAI_STATUS_SUCCESS;
    };

    testAddRemoveCounter(
        sai_object_id_t(0x15000000000000),
        SAI_OBJECT_TYPE_QUEUE,
        QUEUE_COUNTER_ID_LIST,
        {"SAI_QUEUE_STAT_PACKETS", "SAI_QUEUE_STAT_BYTES"},
        {"100", "200"},
        counterVerifyFunc,
        false,
        STATS_MODE_READ_AND_CLEAR);
    EXPECT_EQ(true, clearCalled);

    testAddRemoveCounter(
        sai_object_id_t(0x1a000000000000),
        SAI_OBJECT_TYPE_INGRESS_PRIORITY_GROUP,
        PG_COUNTER_ID_LIST,
        {"SAI_INGRESS_PRIORITY_GROUP_STAT_PACKETS", "SAI_INGRESS_PRIORITY_GROUP_STAT_BYTES"},
        {"100", "200"},
        counterVerifyFunc,
        false);

    testAddRemoveCounter(
        sai_object_id_t(0x6000000000000),
        SAI_OBJECT_TYPE_ROUTER_INTERFACE,
        RIF_COUNTER_ID_LIST,
        {"SAI_ROUTER_INTERFACE_STAT_IN_OCTETS", "SAI_ROUTER_INTERFACE_STAT_IN_PACKETS"},
        {"100", "200"},
        counterVerifyFunc,
        false);

    testAddRemoveCounter(
        sai_object_id_t(0x21000000000000),
        SAI_OBJECT_TYPE_SWITCH,
        SWITCH_DEBUG_COUNTER_ID_LIST,
        {"SAI_SWITCH_STAT_IN_CONFIGURED_DROP_REASONS_0_DROPPED_PKTS", "SAI_SWITCH_STAT_IN_CONFIGURED_DROP_REASONS_1_DROPPED_PKTS"},
        {"100", "200"},
        counterVerifyFunc,
        false);

    testAddRemoveCounter(
        sai_object_id_t(0x2a000000000000),
        SAI_OBJECT_TYPE_TUNNEL,
        TUNNEL_COUNTER_ID_LIST,
        {"SAI_TUNNEL_STAT_IN_OCTETS", "SAI_TUNNEL_STAT_IN_PACKETS"},
        {"100", "200"},
        counterVerifyFunc,
        false);

    clearCalled = false;
    testAddRemoveCounter(
        sai_object_id_t(0x18000000000000),
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
        sai_object_id_t(0x15000000000000),
        SAI_OBJECT_TYPE_QUEUE,
        QUEUE_ATTR_ID_LIST,
        {"SAI_QUEUE_ATTR_PAUSE_STATUS"},
        {"false"},
        counterVerifyFunc,
        false);

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
        sai_object_id_t(0x1a000000000000),
        SAI_OBJECT_TYPE_INGRESS_PRIORITY_GROUP,
        PG_ATTR_ID_LIST,
        {"SAI_INGRESS_PRIORITY_GROUP_ATTR_PORT"},
        {"oid:0x1"},
        counterVerifyFunc,
        false);

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
        sai_object_id_t(0x5c000000000000),
        SAI_OBJECT_TYPE_MACSEC_SA,
        MACSEC_SA_ATTR_ID_LIST,
        {"SAI_MACSEC_SA_ATTR_CONFIGURED_EGRESS_XPN", "SAI_MACSEC_SA_ATTR_AN"},
        {"0", "1"},
        counterVerifyFunc,
        false);

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
        sai_object_id_t(0x9000000000000),
        SAI_OBJECT_TYPE_ACL_COUNTER,
        ACL_COUNTER_ATTR_ID_LIST,
        {"SAI_ACL_COUNTER_ATTR_PACKETS"},
        {"1000"},
        counterVerifyFunc,
        false);
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
        sai_object_id_t(0x1000000000000),
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
}

void testAddRemovePlugin(const std::string& pluginFieldName)
{
    SWSS_LOG_ENTER();

    FlexCounter fc("test", sai, "COUNTERS_DB");

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
                            FLOW_COUNTER_PLUGIN_FIELD};
    for (auto &field : fields)
    {
        testAddRemovePlugin(field);
    }
}

