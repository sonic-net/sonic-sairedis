#include "FlexCounter.h"
#include "MockableSaiInterface.h"
#include "MockHelper.h"

#include <gtest/gtest.h>


using namespace syncd;
using namespace std;

TEST(FlexCounter, addRemoveCounterForFlowCounter)
{
    std::shared_ptr<MockableSaiInterface> sai(new MockableSaiInterface());
    FlexCounter fc("test", sai, "COUNTERS_DB");

    sai_object_id_t counterVid{0x54000000000000};
    sai_object_id_t counterRid{0x54000000000000};
    std::vector<swss::FieldValueTuple> values;
    values.emplace_back(FLOW_COUNTER_ID_LIST, "SAI_COUNTER_STAT_PACKETS,SAI_COUNTER_STAT_BYTES");

    test_syncd::mockVidManagerObjectTypeQuery(SAI_OBJECT_TYPE_COUNTER);
    sai->mock_getStatsExt = [](sai_object_type_t, sai_object_id_t, uint32_t number_of_counters, const sai_stat_id_t *, sai_stats_mode_t, uint64_t *counters) {
        for (uint32_t i = 0; i < number_of_counters; i++)
        {
            counters[i] = (i + 1) * 100;
        }
        return SAI_STATUS_SUCCESS;
    };

    fc.addCounter(counterVid, counterRid, values);
    EXPECT_EQ(fc.isEmpty(), false);

    values.clear();
    values.emplace_back(POLL_INTERVAL_FIELD, "1000");
    fc.addCounterPlugin(values);

    values.clear();
    values.emplace_back(FLEX_COUNTER_STATUS_FIELD, "enable");
    fc.addCounterPlugin(values);

    usleep(1000*1000);
    swss::DBConnector db("COUNTERS_DB", 0);
    swss::RedisPipeline pipeline(&db);
    swss::Table countersTable(&pipeline, COUNTERS_TABLE, false);

    std::vector<std::string> keys;
    countersTable.getKeys(keys);
    EXPECT_EQ(keys.size(), size_t(1));
    EXPECT_EQ(keys[0], "oid:0x54000000000000");

    std::string value;
    countersTable.hget("oid:0x54000000000000", "SAI_COUNTER_STAT_PACKETS", value);
    EXPECT_EQ(value, "100");
    countersTable.hget("oid:0x54000000000000", "SAI_COUNTER_STAT_BYTES", value);
    EXPECT_EQ(value, "200");

    fc.removeCounter(counterVid);
    EXPECT_EQ(fc.isEmpty(), true);
    countersTable.getKeys(keys);

    ASSERT_TRUE(keys.empty());

}

TEST(FlexCounter, addRemoveCounterPluginForFlowCounter)
{
    std::shared_ptr<MockableSaiInterface> sai(new MockableSaiInterface());
    FlexCounter fc("test", sai, "COUNTERS_DB");

    std::vector<swss::FieldValueTuple> values;
    values.emplace_back(FLOW_COUNTER_PLUGIN_FIELD, "dummy_sha_strings");
    fc.addCounterPlugin(values);
    EXPECT_EQ(fc.isEmpty(), false);

    fc.removeCounterPlugins();
    EXPECT_EQ(fc.isEmpty(), true);
}

TEST(FlexCounter, addRemoveCounterForMACsecFlow)
{
    std::shared_ptr<MockableSaiInterface> sai(new MockableSaiInterface());
    FlexCounter fc("test", sai, "COUNTERS_DB");

    sai_object_id_t macsecFlowVid{0x5a000000000000};
    sai_object_id_t macsecFlowRid{0x5a000000000000};
    std::vector<swss::FieldValueTuple> values;
    values.emplace_back(MACSEC_FLOW_COUNTER_ID_LIST, "SAI_MACSEC_FLOW_STAT_CONTROL_PKTS,SAI_MACSEC_FLOW_STAT_PKTS_UNTAGGED");

    test_syncd::mockVidManagerObjectTypeQuery(SAI_OBJECT_TYPE_MACSEC_FLOW);
    sai->mock_getStatsExt = [](sai_object_type_t, sai_object_id_t, uint32_t number_of_counters, const sai_stat_id_t *, sai_stats_mode_t, uint64_t *counters) {
        for (uint32_t i = 0; i < number_of_counters; i++)
        {
            counters[i] = (i + 1) * 100;
        }
        return SAI_STATUS_SUCCESS;
    };

    fc.addCounter(macsecFlowVid, macsecFlowRid, values);
    EXPECT_EQ(fc.isEmpty(), false);

    values.clear();
    values.emplace_back(POLL_INTERVAL_FIELD, "1000");
    fc.addCounterPlugin(values);

    values.clear();
    values.emplace_back(FLEX_COUNTER_STATUS_FIELD, "enable");
    fc.addCounterPlugin(values);

    usleep(1000*1000);
    swss::DBConnector db("COUNTERS_DB", 0);
    swss::RedisPipeline pipeline(&db);
    swss::Table countersTable(&pipeline, COUNTERS_TABLE, false);

    std::vector<std::string> keys;
    countersTable.getKeys(keys);
    EXPECT_EQ(keys.size(), size_t(1));
    EXPECT_EQ(keys[0], "oid:0x5a000000000000");

    std::string value;
    countersTable.hget("oid:0x5a000000000000", "SAI_MACSEC_FLOW_STAT_CONTROL_PKTS", value);
    //EXPECT_EQ(value, "100");
    countersTable.hget("oid:0x5a000000000000", "SAI_MACSEC_FLOW_STAT_PKTS_UNTAGGED", value);
    //EXPECT_EQ(value, "200");

    fc.removeCounter(macsecFlowVid);
    EXPECT_EQ(fc.isEmpty(), true);
    countersTable.del("oid:0x5a000000000000");
    countersTable.getKeys(keys);

    ASSERT_TRUE(keys.empty());

}

TEST(FlexCounter, addRemoveCounterForMACsecSA)
{
    std::shared_ptr<MockableSaiInterface> sai(new MockableSaiInterface());
    FlexCounter fc("test", sai, "COUNTERS_DB");

    sai_object_id_t macsecSAVid{0x5c000000000000};
    sai_object_id_t macsecSARid{0x5c000000000000};
    std::vector<swss::FieldValueTuple> values;
    values.emplace_back(MACSEC_SA_COUNTER_ID_LIST, "SAI_MACSEC_SA_STAT_OCTETS_ENCRYPTED,SAI_MACSEC_SA_STAT_OCTETS_PROTECTED");

    test_syncd::mockVidManagerObjectTypeQuery(SAI_OBJECT_TYPE_MACSEC_SA);
    sai->mock_getStatsExt = [](sai_object_type_t, sai_object_id_t, uint32_t number_of_counters, const sai_stat_id_t *, sai_stats_mode_t, uint64_t *counters) {
        for (uint32_t i = 0; i < number_of_counters; i++)
        {
            counters[i] = (i + 1) * 100;
        }
        return SAI_STATUS_SUCCESS;
    };

    fc.addCounter(macsecSAVid, macsecSARid, values);
    EXPECT_EQ(fc.isEmpty(), false);

    values.clear();
    values.emplace_back(POLL_INTERVAL_FIELD, "1000");
    fc.addCounterPlugin(values);

    values.clear();
    values.emplace_back(FLEX_COUNTER_STATUS_FIELD, "enable");
    fc.addCounterPlugin(values);

    usleep(1000*1000);
    swss::DBConnector db("COUNTERS_DB", 0);
    swss::RedisPipeline pipeline(&db);
    swss::Table countersTable(&pipeline, COUNTERS_TABLE, false);

    std::vector<std::string> keys;
    countersTable.getKeys(keys);
    EXPECT_EQ(keys.size(), size_t(1));
    EXPECT_EQ(keys[0], "oid:0x5c000000000000");

    std::string value;
    countersTable.hget("oid:0x5c000000000000", "SAI_MACSEC_SA_STAT_OCTETS_ENCRYPTED", value);
    //EXPECT_EQ(value, "100");
    countersTable.hget("oid:0x5c000000000000", "SAI_MACSEC_SA_STAT_OCTETS_PROTECTED", value);
    //EXPECT_EQ(value, "200");

    fc.removeCounter(macsecSAVid);
    EXPECT_EQ(fc.isEmpty(), true);
    countersTable.del("oid:0x5c000000000000");
    countersTable.getKeys(keys);

    ASSERT_TRUE(keys.empty());

}

TEST(FlexCounter, addRemoveCounterForPort)
{
    std::shared_ptr<MockableSaiInterface> sai(new MockableSaiInterface());
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
    fc.addCounterPlugin(values);

    usleep(1000*1000);
    swss::DBConnector db("COUNTERS_DB", 0);
    swss::RedisPipeline pipeline(&db);
    swss::Table countersTable(&pipeline, COUNTERS_TABLE, false);

    std::vector<std::string> keys;
    countersTable.getKeys(keys);
    EXPECT_EQ(keys.size(), size_t(1));
    EXPECT_EQ(keys[0], "oid:0x1000000000000");

    std::string value;
    countersTable.hget("oid:0x1000000000000", "SAI_PORT_STAT_IF_IN_OCTETS", value);
    EXPECT_EQ(value, "100");
    countersTable.hget("oid:0x1000000000000", "SAI_PORT_STAT_IF_IN_ERRORS", value);
    EXPECT_EQ(value, "200");

    fc.removeCounter(counterVid);
    EXPECT_EQ(fc.isEmpty(), true);
    countersTable.del("oid:0x1000000000000");
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
        capability->list[1].stat_enum = SAI_PORT_STAT_IF_IN_ERRORS;
        return SAI_STATUS_SUCCESS;
    };

    values.clear();
    values.emplace_back(PORT_COUNTER_ID_LIST, "SAI_PORT_STAT_IF_IN_OCTETS,SAI_PORT_STAT_IF_IN_ERRORS");
    fc.addCounter(counterVid, counterRid, values);
    EXPECT_EQ(fc.isEmpty(), false);

    usleep(1000*1000);
    countersTable.hget("oid:0x1000000000000", "SAI_PORT_STAT_IF_IN_OCTETS", value);
    EXPECT_EQ(value, "100");
    countersTable.hget("oid:0x1000000000000", "SAI_PORT_STAT_IF_IN_ERRORS", value);
    EXPECT_EQ(value, "200");

    fc.removeCounter(counterVid);
    EXPECT_EQ(fc.isEmpty(), true);
    countersTable.del("oid:0x1000000000000");
    countersTable.getKeys(keys);
    ASSERT_TRUE(keys.empty());
}
