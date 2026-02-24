// includes -----------------------------------------------------------------------------------------------------------

#include <cstdint>
#include <memory>
#include <vector>
#include <array>
#include <set>
#include <thread>
#include <algorithm>

#include <gtest/gtest.h>

#include <swss/logger.h>
#include <swss/table.h>

#include "meta/sai_serialize.h"
#include "Sai.h"
#include "Syncd.h"
#include "MetadataLogger.h"

#include "TestSyncdLib.h"

using namespace syncd;

// functions ----------------------------------------------------------------------------------------------------------

static const char* profile_get_value(
    _In_ sai_switch_profile_id_t profile_id,
    _In_ const char* variable)
{
    SWSS_LOG_ENTER();

    return NULL;
}

static int profile_get_next_value(
    _In_ sai_switch_profile_id_t profile_id,
    _Out_ const char** variable,
    _Out_ const char** value)
{
    SWSS_LOG_ENTER();

    if (value == NULL)
    {
        SWSS_LOG_INFO("resetting profile map iterator");
        return 0;
    }

    if (variable == NULL)
    {
        SWSS_LOG_WARN("variable is null");
        return -1;
    }

    SWSS_LOG_INFO("iterator reached end");
    return -1;
}

static sai_service_method_table_t test_services = {
    profile_get_value,
    profile_get_next_value
};

// Nvidia ASIC --------------------------------------------------------------------------------------------------------

void syncdMlnxWorkerThread()
{
    SWSS_LOG_ENTER();

    swss::Logger::getInstance().setMinPrio(swss::Logger::SWSS_NOTICE);
    MetadataLogger::initialize();

    auto vendorSai = std::make_shared<VendorSai>();
    auto commandLineOptions = std::make_shared<CommandLineOptions>();
    auto isWarmStart = false;

    commandLineOptions->m_enableSyncMode= true;
    commandLineOptions->m_enableTempView = false;
    commandLineOptions->m_disableExitSleep = true;
    commandLineOptions->m_enableUnittests = false;
    commandLineOptions->m_enableSaiBulkSupport = true;
    commandLineOptions->m_startType = SAI_START_TYPE_COLD_BOOT;
    commandLineOptions->m_redisCommunicationMode = SAI_REDIS_COMMUNICATION_MODE_REDIS_SYNC;
    commandLineOptions->m_profileMapFile = "./mlnx/sai.profile";

    auto syncd = std::make_shared<Syncd>(vendorSai, commandLineOptions, isWarmStart);
    syncd->run();

    SWSS_LOG_NOTICE("Started syncd worker");
}

class SyncdMlnxTest : public ::testing::Test
{
public:
    SyncdMlnxTest() = default;
    virtual ~SyncdMlnxTest() = default;

public:
    virtual void SetUp() override
    {
        SWSS_LOG_ENTER();

        // flush ASIC DB

        flushAsicDb();

        // flush FLEX COUNTER DB
        flushFlexCounterDb();

        // start syncd worker

        m_worker = std::make_shared<std::thread>(syncdMlnxWorkerThread);

        // initialize SAI redis

        m_sairedis = std::make_shared<sairedis::Sai>();

        auto status = m_sairedis->apiInitialize(0, &test_services);
        ASSERT_EQ(status, SAI_STATUS_SUCCESS);

        // set communication mode

        sai_attribute_t attr;

        attr.id = SAI_REDIS_SWITCH_ATTR_REDIS_COMMUNICATION_MODE;
        attr.value.s32 = SAI_REDIS_COMMUNICATION_MODE_REDIS_SYNC;

        status = m_sairedis->set(SAI_OBJECT_TYPE_SWITCH, SAI_NULL_OBJECT_ID, &attr);
        ASSERT_EQ(status, SAI_STATUS_SUCCESS);

        // enable recording

        attr.id = SAI_REDIS_SWITCH_ATTR_RECORD;
        attr.value.booldata = true;

        status = m_sairedis->set(SAI_OBJECT_TYPE_SWITCH, SAI_NULL_OBJECT_ID, &attr);
        ASSERT_EQ(status, SAI_STATUS_SUCCESS);

        // create switch

        attr.id = SAI_SWITCH_ATTR_INIT_SWITCH;
        attr.value.booldata = true;

        status = m_sairedis->create(SAI_OBJECT_TYPE_SWITCH, &m_switchId, SAI_NULL_OBJECT_ID, 1, &attr);
        ASSERT_EQ(status, SAI_STATUS_SUCCESS);

        auto db = std::make_shared<swss::DBConnector>("FLEX_COUNTER_DB", 0);
        m_flexCounterGroupTable = std::make_shared<swss::Table>(db.get(), "FLEX_COUNTER_GROUP_TABLE");
        m_flexCounterTable = std::make_shared<swss::Table>(db.get(), "FLEX_COUNTER_TABLE");
    }

    virtual void TearDown() override
    {
        SWSS_LOG_ENTER();

        // uninitialize SAI redis

        auto status = m_sairedis->apiUninitialize();
        ASSERT_EQ(status, SAI_STATUS_SUCCESS);

        // stop syncd worker

        sendSyncdShutdownNotification();
        m_worker->join();
    }

protected:
    std::shared_ptr<std::thread> m_worker;
    std::shared_ptr<sairedis::Sai> m_sairedis;
    std::shared_ptr<swss::Table> m_flexCounterGroupTable;
    std::shared_ptr<swss::Table> m_flexCounterTable;

    sai_object_id_t m_switchId = SAI_NULL_OBJECT_ID;
};

TEST_F(SyncdMlnxTest, queryAttrEnumValuesCapability)
{
    sai_s32_list_t data = { .count = 0, .list = nullptr };

    auto status = m_sairedis->queryAttributeEnumValuesCapability(
        m_switchId, SAI_OBJECT_TYPE_HASH, SAI_HASH_ATTR_NATIVE_HASH_FIELD_LIST, &data
    );
    ASSERT_EQ(status, SAI_STATUS_BUFFER_OVERFLOW);

    std::vector<sai_int32_t> hfList(data.count);
    data.list = hfList.data();

    status = m_sairedis->queryAttributeEnumValuesCapability(
        m_switchId, SAI_OBJECT_TYPE_HASH, SAI_HASH_ATTR_NATIVE_HASH_FIELD_LIST, &data
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

TEST_F(SyncdMlnxTest, portBulkAddRemove)
{
    const std::uint32_t portCount = 4;
    const std::uint32_t laneCount = 4;

    // Generate port config
    sai_attribute_t attr;
    std::array<std::vector<std::uint32_t>, portCount> laneLists;
    std::array<std::vector<sai_attribute_t>, portCount> attrLists;
    std::array<std::uint32_t, portCount> attrCountList;
    std::array<const sai_attribute_t*, portCount> attrPtrList;

    uint32_t lane = 1000;
    for (auto i = 0u; i < portCount; i++)
    {
        auto &laneList = laneLists[i];
        auto &attrList = attrLists[i];
        for (auto j = 0u; j < laneCount; j++)
        {
            laneList.push_back(lane++);
        }
        attr.id = SAI_PORT_ATTR_HW_LANE_LIST;
        attr.value.u32list.count = static_cast<std::uint32_t>(laneList.size());
        attr.value.u32list.list = laneList.data();
        attrList.push_back(attr);

        attr.id = SAI_PORT_ATTR_SPEED;
        attr.value.u32 = 1000;
        attrList.push_back(attr);

        attrPtrList[i] = attrList.data();
        attrCountList[i] = static_cast<std::uint32_t>(attrList.size());
    }

    std::vector<sai_object_id_t> oidList(portCount, SAI_NULL_OBJECT_ID);
    std::vector<sai_status_t> statusList(portCount, SAI_STATUS_SUCCESS);

    // Validate port bulk add
    auto status = m_sairedis->bulkCreate(
        SAI_OBJECT_TYPE_PORT, m_switchId, portCount, attrCountList.data(), attrPtrList.data(),
        SAI_BULK_OP_ERROR_MODE_IGNORE_ERROR,
        oidList.data(), statusList.data()
    );
    ASSERT_EQ(status, SAI_STATUS_SUCCESS);

    for (std::uint32_t i = 0; i < portCount; i++)
    {
        ASSERT_EQ(statusList.at(i), SAI_STATUS_SUCCESS);
    }

    // Counter operations based on the created port
    // 1. Enable counter polling for port stat counter group
    sai_redis_flex_counter_group_parameter_t flexCounterGroupParam;

    std::string group("PORT_STAT_COUNTER");
    std::string poll_interval = "10000";
    std::string stats_mode = "STATS_MODE_READ";
    std::string operation = "enable";

    flexCounterGroupParam.counter_group_name.list = (int8_t*)const_cast<char *>(group.c_str());
    flexCounterGroupParam.counter_group_name.count = (uint32_t)group.length();
    flexCounterGroupParam.poll_interval.list = (int8_t*)const_cast<char *>(poll_interval.c_str());
    flexCounterGroupParam.poll_interval.count = (uint32_t)poll_interval.length();
    flexCounterGroupParam.plugin_name.list = nullptr;
    flexCounterGroupParam.plugin_name.count = 0;
    flexCounterGroupParam.plugins.list = nullptr;
    flexCounterGroupParam.plugins.count = 0;
    flexCounterGroupParam.stats_mode.list = (int8_t*)const_cast<char *>(stats_mode.c_str());
    flexCounterGroupParam.stats_mode.count = (uint32_t)stats_mode.length();
    flexCounterGroupParam.operation.list = (int8_t*)const_cast<char *>(operation.c_str());;
    flexCounterGroupParam.operation.count = (uint32_t)operation.length();
    flexCounterGroupParam.bulk_chunk_size.list = nullptr;
    flexCounterGroupParam.bulk_chunk_size.count = 0;
    flexCounterGroupParam.bulk_chunk_size_per_prefix.list = nullptr;
    flexCounterGroupParam.bulk_chunk_size_per_prefix.count = 0;

    attr.id = SAI_REDIS_SWITCH_ATTR_FLEX_COUNTER_GROUP;
    attr.value.ptr = (void*)&flexCounterGroupParam;

    // Failed to create if the switchId is invalid for the context
    status = m_sairedis->set(SAI_OBJECT_TYPE_SWITCH, m_switchId + 1, &attr);
    std::vector<swss::FieldValueTuple> fvVector, fvVectorExpected;
    ASSERT_FALSE(m_flexCounterGroupTable->get("PORT_STAT_COUNTER", fvVector));

    status = m_sairedis->set(SAI_OBJECT_TYPE_SWITCH, m_switchId, &attr);
    ASSERT_EQ(status, SAI_STATUS_SUCCESS);

    ASSERT_TRUE(m_flexCounterGroupTable->get("PORT_STAT_COUNTER", fvVector));
    fvVectorExpected.emplace_back(POLL_INTERVAL_FIELD, poll_interval);
    fvVectorExpected.emplace_back(STATS_MODE_FIELD, stats_mode);
    fvVectorExpected.emplace_back(FLEX_COUNTER_STATUS_FIELD, operation);
    ASSERT_EQ(fvVectorExpected, fvVector);
    fvVectorExpected.clear();
    fvVector.clear();

    // 2. Start counter polling for the port just created
    // Try with a bad key first
    sai_redis_flex_counter_parameter_t flexCounterParam;
    std::string key = "PORT_STAT_COUNTER";
    std::string counters = "SAI_PORT_STAT_IF_IN_OCTETS";
    std::string counter_field_name = "PORT_COUNTER_ID_LIST";

    flexCounterParam.counter_key.list = (int8_t*)const_cast<char *>(key.c_str());
    flexCounterParam.counter_key.count = (uint32_t)key.length();
    flexCounterParam.counter_ids.list = (int8_t*)const_cast<char *>(counters.c_str());
    flexCounterParam.counter_ids.count = (uint32_t)counters.length();
    flexCounterParam.counter_field_name.list = (int8_t*)const_cast<char *>(counter_field_name.c_str());
    flexCounterParam.counter_field_name.count = (uint32_t)counter_field_name.length();
    flexCounterParam.stats_mode.list = nullptr;
    flexCounterParam.stats_mode.count = 0;

    attr.id = SAI_REDIS_SWITCH_ATTR_FLEX_COUNTER;
    attr.value.ptr = (void*)&flexCounterParam;

    status = m_sairedis->set(SAI_OBJECT_TYPE_SWITCH, m_switchId, &attr);
    ASSERT_EQ(status, SAI_STATUS_FAILURE);
    ASSERT_FALSE(m_flexCounterTable->get(key, fvVector));

    // Try with a good key
    key = "PORT_STAT_COUNTER:" + sai_serialize_object_id(oidList[0]);
    flexCounterParam.counter_key.list = (int8_t*)const_cast<char *>(key.c_str());
    flexCounterParam.counter_key.count = (uint32_t)key.length();
    status = m_sairedis->set(SAI_OBJECT_TYPE_SWITCH, m_switchId, &attr);
    ASSERT_EQ(status, SAI_STATUS_SUCCESS);

    ASSERT_TRUE(m_flexCounterTable->get(key, fvVector));
    fvVectorExpected.emplace_back(counter_field_name, counters);
    ASSERT_EQ(fvVectorExpected, fvVector);

    // Try with bulk initialization
    key = "PORT_STAT_COUNTER:";
    for (auto i = 1u; i < portCount; i++)
    {
        key += sai_serialize_object_id(oidList[i]) + ",";
    }
    key.pop_back();

    flexCounterParam.counter_key.list = (int8_t*)const_cast<char *>(key.c_str());
    flexCounterParam.counter_key.count = (uint32_t)key.length();
    status = m_sairedis->set(SAI_OBJECT_TYPE_SWITCH, m_switchId, &attr);
    ASSERT_EQ(status, SAI_STATUS_SUCCESS);

    for (auto i = 1u; i < portCount; i++)
    {
        key = "PORT_STAT_COUNTER:" + sai_serialize_object_id(oidList[i]);
        ASSERT_TRUE(m_flexCounterTable->get(key, fvVector));
        ASSERT_EQ(fvVectorExpected, fvVector);
    }

    flexCounterParam.counter_ids.list = nullptr;
    flexCounterParam.counter_ids.count = 0;
    flexCounterParam.counter_field_name.list = nullptr;
    flexCounterParam.counter_field_name.count = 0;

    // 3. Stop counter polling for the port
    for (auto i = 0u; i < portCount; i++)
    {
        key = "PORT_STAT_COUNTER:" + sai_serialize_object_id(oidList[i]);
        flexCounterParam.counter_key.list = (int8_t*)const_cast<char *>(key.c_str());
        flexCounterParam.counter_key.count = (uint32_t)key.length();
        status = m_sairedis->set(SAI_OBJECT_TYPE_SWITCH, m_switchId, &attr);
        ASSERT_EQ(status, SAI_STATUS_SUCCESS);
        ASSERT_FALSE(m_flexCounterTable->get(key, fvVector));
    }

    // 4. Disable counter polling for the group
    flexCounterGroupParam.poll_interval.list = nullptr;
    flexCounterGroupParam.poll_interval.count = 0;
    flexCounterGroupParam.stats_mode.list = nullptr;
    flexCounterGroupParam.stats_mode.count = 0;
    flexCounterGroupParam.operation.list = nullptr;
    flexCounterGroupParam.operation.count = 0;

    attr.id = SAI_REDIS_SWITCH_ATTR_FLEX_COUNTER_GROUP;
    attr.value.ptr = (void*)&flexCounterGroupParam;

    status = m_sairedis->set(SAI_OBJECT_TYPE_SWITCH, m_switchId, &attr);
    ASSERT_EQ(status, SAI_STATUS_SUCCESS);
    ASSERT_FALSE(m_flexCounterTable->get(key, fvVector));

    // Validate port bulk remove
    status = m_sairedis->bulkRemove(
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
