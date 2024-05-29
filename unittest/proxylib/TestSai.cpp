#include <cstdint>

#include <memory>
#include <vector>
#include <array>

#include <gtest/gtest.h>

#include "meta/DummySaiInterface.h"

#include "Sai.h"
#include "Proxy.h"

// TODO fix join when tests will fail

using namespace saiproxy;

class SaiTest : public ::testing::Test
{
public:
    SaiTest() = default;
    virtual ~SaiTest() = default;

public:
    virtual void SetUp() override
    {
        m_sai = std::make_shared<Sai>();

        //sai_attribute_t attr;
        //attr.id = SAI_SWITCH_ATTR_INIT_SWITCH;
        //attr.value.booldata = true;

        //auto status = m_sai->create(SAI_OBJECT_TYPE_SWITCH, &m_swid, SAI_NULL_OBJECT_ID, 1, &attr);
        //ASSERT_EQ(status, SAI_STATUS_SUCCESS);
    }

    virtual void TearDown() override
    {
        //auto status = m_sai->remove(SAI_OBJECT_TYPE_SWITCH, m_swid);
        //ASSERT_EQ(status, SAI_STATUS_SUCCESS);
    }

protected:
    std::shared_ptr<Sai> m_sai;

    sai_object_id_t m_swid = SAI_NULL_OBJECT_ID;

    const std::uint32_t m_guid = 0; // default context config id
    const std::uint32_t m_scid = 0; // default switch config id
};

static const char* profile_get_value(
        _In_ sai_switch_profile_id_t profile_id,
        _In_ const char* variable)
{
    SWSS_LOG_ENTER();

    if (variable == NULL)
        return NULL;

    return nullptr;
}

static int profile_get_next_value(
        _In_ sai_switch_profile_id_t profile_id,
        _Out_ const char** variable,
        _Out_ const char** value)
{
    SWSS_LOG_ENTER();

    return 0;
}

static sai_service_method_table_t test_services = {
    profile_get_value,
    profile_get_next_value
};

TEST_F(SaiTest, Ctr)
{
    auto s = std::make_shared<Sai>();
}

TEST(Sai, bulkGet)
{
    Sai sai;

    sai_object_id_t oids[1] = {0};
    uint32_t attrcount[1] = {0};
    sai_attribute_t* attrs[1] = {0};
    sai_status_t statuses[1] = {0};


    EXPECT_EQ(SAI_STATUS_NOT_IMPLEMENTED,
            sai.bulkGet(
                SAI_OBJECT_TYPE_PORT,
                1,
                oids,
                attrcount,
                attrs,
                SAI_BULK_OP_ERROR_MODE_STOP_ON_ERROR,
                statuses));
}

TEST(Sai, apiInitialize)
{
    Sai sai;

    // non zero flags
    EXPECT_EQ(sai.apiInitialize(1, &test_services), SAI_STATUS_INVALID_PARAMETER);

    // table null
    EXPECT_EQ(sai.apiInitialize(0, NULL), SAI_STATUS_INVALID_PARAMETER);

    // correct one
    EXPECT_EQ(sai.apiInitialize(0, &test_services), SAI_STATUS_SUCCESS);

    // second initialize
    EXPECT_EQ(sai.apiInitialize(0, &test_services), SAI_STATUS_FAILURE);
}

TEST(Sai, apiUninitialize)
{
    Sai sai;

    EXPECT_EQ(sai.apiInitialize(0, &test_services), SAI_STATUS_SUCCESS);

    EXPECT_EQ(sai.apiUninitialize(), SAI_STATUS_SUCCESS);
}

static void fun(std::shared_ptr<Proxy> proxy)
{
    SWSS_LOG_ENTER();

    proxy->run();
}

TEST(Sai, create)
{
    Sai sai;

    EXPECT_EQ(sai.apiInitialize(0, &test_services), SAI_STATUS_SUCCESS);

    std::shared_ptr<sairedis::SaiInterface> dummy = std::make_shared<saimeta::DummySaiInterface>();

    auto proxy = std::make_shared<Proxy>(dummy);

    auto thread = std::make_shared<std::thread>(fun,proxy);

    sai_object_id_t switch_id = SAI_NULL_OBJECT_ID;

    sai_attribute_t attr;

    attr.id = SAI_SWITCH_ATTR_INIT_SWITCH;
    attr.value.booldata = true;

    // create oid
    auto status = sai.create(
            SAI_OBJECT_TYPE_SWITCH,
            &switch_id,
            SAI_NULL_OBJECT_ID, // creating switch
            1,
            &attr);

    EXPECT_EQ(status, SAI_STATUS_SUCCESS);
    EXPECT_NE(switch_id, SAI_NULL_OBJECT_ID);

    sai_fdb_entry_t fdb = {};

    attr.id = SAI_FDB_ENTRY_ATTR_META_DATA;
    attr.value.u32 = 0;

    // create entry
    status = sai.create(&fdb, 1, &attr);

    EXPECT_EQ(status, SAI_STATUS_SUCCESS);

    proxy->stop();

    thread->join();
}

TEST(Sai, remove)
{
    Sai sai;

    EXPECT_EQ(sai.apiInitialize(0, &test_services), SAI_STATUS_SUCCESS);

    std::shared_ptr<sairedis::SaiInterface> dummy = std::make_shared<saimeta::DummySaiInterface>();

    auto proxy = std::make_shared<Proxy>(dummy);

    auto thread = std::make_shared<std::thread>(fun,proxy);

    // remove oid
    auto status = sai.remove(
            SAI_OBJECT_TYPE_SWITCH,
            (sai_object_id_t)1);

    EXPECT_EQ(status, SAI_STATUS_SUCCESS);

    sai_fdb_entry_t fdb = {};

    // remove entry
    status = sai.remove(&fdb);

    EXPECT_EQ(status, SAI_STATUS_SUCCESS);

    proxy->stop();

    thread->join();
}

TEST(Sai, set)
{
    Sai sai;

    EXPECT_EQ(sai.apiInitialize(0, &test_services), SAI_STATUS_SUCCESS);

    std::shared_ptr<sairedis::SaiInterface> dummy = std::make_shared<saimeta::DummySaiInterface>();

    auto proxy = std::make_shared<Proxy>(dummy);

    auto thread = std::make_shared<std::thread>(fun,proxy);

    sai_attribute_t attr;

    attr.id = SAI_SWITCH_ATTR_INIT_SWITCH;
    attr.value.booldata = true;

    // set oid
    auto status = sai.set(
            SAI_OBJECT_TYPE_SWITCH,
            (sai_object_id_t)1,
            &attr);

    EXPECT_EQ(status, SAI_STATUS_SUCCESS);

    sai_fdb_entry_t fdb = {};

    attr.id = SAI_FDB_ENTRY_ATTR_META_DATA;
    attr.value.u32 = 0;

    // set entry
    status = sai.set(&fdb, &attr);

    EXPECT_EQ(status, SAI_STATUS_SUCCESS);

    proxy->stop();

    thread->join();
}

TEST(Sai, get)
{
    Sai sai;

    EXPECT_EQ(sai.apiInitialize(0, &test_services), SAI_STATUS_SUCCESS);

    std::shared_ptr<sairedis::SaiInterface> dummy = std::make_shared<saimeta::DummySaiInterface>();

    auto proxy = std::make_shared<Proxy>(dummy);

    auto thread = std::make_shared<std::thread>(fun,proxy);

    sai_attribute_t attr;

    attr.id = SAI_SWITCH_ATTR_INIT_SWITCH;

    // get oid
    auto status = sai.get(
            SAI_OBJECT_TYPE_SWITCH,
            (sai_object_id_t)1,
            1,
            &attr);

    EXPECT_EQ(status, SAI_STATUS_SUCCESS);

    sai_fdb_entry_t fdb = {};

    attr.id = SAI_FDB_ENTRY_ATTR_META_DATA;

    // get entry
    status = sai.get(&fdb, 1, &attr);

    EXPECT_EQ(status, SAI_STATUS_SUCCESS);

    proxy->stop();

    thread->join();
}

TEST(Sai, flushFdbEntries)
{
    Sai sai;

    EXPECT_EQ(sai.apiInitialize(0, &test_services), SAI_STATUS_SUCCESS);

    std::shared_ptr<sairedis::SaiInterface> dummy = std::make_shared<saimeta::DummySaiInterface>();

    auto proxy = std::make_shared<Proxy>(dummy);

    auto thread = std::make_shared<std::thread>(fun,proxy);

    auto status = sai.flushFdbEntries((sai_object_id_t)1, 0, 0);

    EXPECT_EQ(status, SAI_STATUS_SUCCESS);

    proxy->stop();

    thread->join();
}


TEST(Sai, processObjectTypeGetAvailability)
{
    Sai sai;

    EXPECT_EQ(sai.apiInitialize(0, &test_services), SAI_STATUS_SUCCESS);

    std::shared_ptr<sairedis::SaiInterface> dummy = std::make_shared<saimeta::DummySaiInterface>();

    auto proxy = std::make_shared<Proxy>(dummy);

    auto thread = std::make_shared<std::thread>(fun,proxy);

    uint64_t count;

    auto status = sai.objectTypeGetAvailability(
            (sai_object_id_t)1,
            SAI_OBJECT_TYPE_SWITCH,
            0,
            0,
            &count);


    EXPECT_EQ(status, SAI_STATUS_SUCCESS);

    proxy->stop();

    thread->join();
}

TEST(Sai, processQueryAttributeCapability)
{
    Sai sai;

    EXPECT_EQ(sai.apiInitialize(0, &test_services), SAI_STATUS_SUCCESS);

    std::shared_ptr<sairedis::SaiInterface> dummy = std::make_shared<saimeta::DummySaiInterface>();

    auto proxy = std::make_shared<Proxy>(dummy);

    auto thread = std::make_shared<std::thread>(fun,proxy);

    sai_attr_capability_t cap;

    auto status = sai.queryAttributeCapability(
            (sai_object_id_t)1,
            SAI_OBJECT_TYPE_SWITCH,
            SAI_SWITCH_ATTR_INIT_SWITCH,
            &cap);

    EXPECT_EQ(status, SAI_STATUS_SUCCESS);

    proxy->stop();

    thread->join();
}

TEST(Sai, queryAttributeEnumValuesCapability)
{
    Sai sai;

    EXPECT_EQ(sai.apiInitialize(0, &test_services), SAI_STATUS_SUCCESS);

    std::shared_ptr<sairedis::SaiInterface> dummy = std::make_shared<saimeta::DummySaiInterface>();

    auto proxy = std::make_shared<Proxy>(dummy);

    auto thread = std::make_shared<std::thread>(fun,proxy);

    sai_s32_list_t list;

    int32_t arr[10];

    list.count = 10;
    list.list = arr;

    auto status = sai.queryAttributeEnumValuesCapability(
            (sai_object_id_t)1,
            SAI_OBJECT_TYPE_SWITCH,
            SAI_SWITCH_ATTR_INIT_SWITCH,
            &list);

    EXPECT_EQ(status, SAI_STATUS_SUCCESS);

    proxy->stop();

    thread->join();
}

TEST(Sai, objectTypeQuery)
{
    Sai sai;

    EXPECT_EQ(sai.apiInitialize(0, &test_services), SAI_STATUS_SUCCESS);

    std::shared_ptr<sairedis::SaiInterface> dummy = std::make_shared<saimeta::DummySaiInterface>();

    auto proxy = std::make_shared<Proxy>(dummy);

    auto thread = std::make_shared<std::thread>(fun,proxy);

    auto objectType = sai.objectTypeQuery(SAI_NULL_OBJECT_ID);

    EXPECT_EQ(objectType, SAI_OBJECT_TYPE_NULL);

    proxy->stop();

    thread->join();
}

TEST(Sai, switchIdQuery)
{
    Sai sai;

    EXPECT_EQ(sai.apiInitialize(0, &test_services), SAI_STATUS_SUCCESS);

    std::shared_ptr<sairedis::SaiInterface> dummy = std::make_shared<saimeta::DummySaiInterface>();

    auto proxy = std::make_shared<Proxy>(dummy);

    auto thread = std::make_shared<std::thread>(fun,proxy);

    auto objectId = sai.switchIdQuery(SAI_NULL_OBJECT_ID);

    EXPECT_EQ(objectId, SAI_NULL_OBJECT_ID);

    proxy->stop();

    thread->join();
}

TEST(Sai, queryApiVersion)
{
    Sai sai;

    sai_api_version_t version;

    // api not initialized
    EXPECT_EQ(sai.queryApiVersion(&version), SAI_STATUS_FAILURE);

    EXPECT_EQ(sai.apiInitialize(0, &test_services), SAI_STATUS_SUCCESS);

    std::shared_ptr<sairedis::SaiInterface> dummy = std::make_shared<saimeta::DummySaiInterface>();

    auto proxy = std::make_shared<Proxy>(dummy);

    auto thread = std::make_shared<std::thread>(fun,proxy);

    EXPECT_EQ(sai.queryApiVersion(NULL), SAI_STATUS_INVALID_PARAMETER);

    auto status = sai.queryApiVersion(&version);

    EXPECT_EQ(status, SAI_STATUS_SUCCESS);
    EXPECT_EQ(version, SAI_API_VERSION);

    proxy->stop();

    thread->join();
}

TEST(Sai, logSet)
{
    Sai sai;

    sai_api_version_t version;

    // api not initialized
    EXPECT_EQ(sai.queryApiVersion(&version), SAI_STATUS_FAILURE);

    EXPECT_EQ(sai.apiInitialize(0, &test_services), SAI_STATUS_SUCCESS);

    std::shared_ptr<sairedis::SaiInterface> dummy = std::make_shared<saimeta::DummySaiInterface>();

    auto proxy = std::make_shared<Proxy>(dummy);

    auto thread = std::make_shared<std::thread>(fun,proxy);

    auto status = sai.logSet(SAI_API_SWITCH, SAI_LOG_LEVEL_NOTICE);

    EXPECT_EQ(status, SAI_STATUS_SUCCESS);

    proxy->stop();

    thread->join();
}

TEST(Sai, getStats)
{
    Sai sai;

    sai_api_version_t version;

    // api not initialized
    EXPECT_EQ(sai.queryApiVersion(&version), SAI_STATUS_FAILURE);

    EXPECT_EQ(sai.apiInitialize(0, &test_services), SAI_STATUS_SUCCESS);

    std::shared_ptr<sairedis::SaiInterface> dummy = std::make_shared<saimeta::DummySaiInterface>();

    auto proxy = std::make_shared<Proxy>(dummy);

    auto thread = std::make_shared<std::thread>(fun,proxy);

    sai_stat_id_t counter_ids[2] = { SAI_PORT_STAT_IF_IN_OCTETS, SAI_PORT_STAT_IF_IN_UCAST_PKTS };

    uint64_t counters[2];

    auto status = sai.getStats(
            SAI_OBJECT_TYPE_PORT,
            (sai_object_id_t)1,
            2,
            counter_ids,
            counters);

    EXPECT_EQ(status, SAI_STATUS_SUCCESS);

    proxy->stop();

    thread->join();
}

TEST(Sai, getStatsExt)
{
    Sai sai;

    sai_api_version_t version;

    // api not initialized
    EXPECT_EQ(sai.queryApiVersion(&version), SAI_STATUS_FAILURE);

    EXPECT_EQ(sai.apiInitialize(0, &test_services), SAI_STATUS_SUCCESS);

    std::shared_ptr<sairedis::SaiInterface> dummy = std::make_shared<saimeta::DummySaiInterface>();

    auto proxy = std::make_shared<Proxy>(dummy);

    auto thread = std::make_shared<std::thread>(fun,proxy);

    sai_stat_id_t counter_ids[2] = { SAI_PORT_STAT_IF_IN_OCTETS, SAI_PORT_STAT_IF_IN_UCAST_PKTS };

    uint64_t counters[2];

    auto status = sai.getStatsExt(
            SAI_OBJECT_TYPE_PORT,
            (sai_object_id_t)1,
            2,
            counter_ids,
            SAI_STATS_MODE_READ,
            counters);

    EXPECT_EQ(status, SAI_STATUS_SUCCESS);

    proxy->stop();

    thread->join();
}

TEST(Sai, clearStats)
{
    Sai sai;

    sai_api_version_t version;

    // api not initialized
    EXPECT_EQ(sai.queryApiVersion(&version), SAI_STATUS_FAILURE);

    EXPECT_EQ(sai.apiInitialize(0, &test_services), SAI_STATUS_SUCCESS);

    std::shared_ptr<sairedis::SaiInterface> dummy = std::make_shared<saimeta::DummySaiInterface>();

    auto proxy = std::make_shared<Proxy>(dummy);

    auto thread = std::make_shared<std::thread>(fun,proxy);

    sai_stat_id_t counter_ids[2] = { SAI_PORT_STAT_IF_IN_OCTETS, SAI_PORT_STAT_IF_IN_UCAST_PKTS };

    auto status = sai.clearStats(
            SAI_OBJECT_TYPE_PORT,
            (sai_object_id_t)1,
            2,
            counter_ids);

    EXPECT_EQ(status, SAI_STATUS_SUCCESS);

    proxy->stop();

    thread->join();
}

