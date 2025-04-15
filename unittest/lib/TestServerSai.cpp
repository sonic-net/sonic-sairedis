#include "ServerSai.h"

#include "sai_serialize.h"
#include "vslib/ContextConfigContainer.h"
#include "vslib/VirtualSwitchSaiInterface.h"
#include "vslib/Sai.h"
#include "lib/Sai.h"

#include "swss/dbconnector.h"

#include "sairediscommon.h"
#include "meta/SelectableChannel.h"

#include "MockSaiInterface.h"
#include "SelectableChannel.h"
#include "swss/dbconnector.h"
#include "swss/redisreply.h"
#include "swss/logger.h"

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <memory>


using namespace sairedis;
using namespace std;
using namespace swss;

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
    return -1;
}

static sai_service_method_table_t test_services = {
    profile_get_value,
    profile_get_next_value
};

class TestServerSaiMockChannel : public SelectableChannel
{
public:
    MOCK_METHOD(bool, empty, (), (override));
    MOCK_METHOD(void, pop, (swss::KeyOpFieldsValuesTuple & kco, bool initViewMode), (override));
    MOCK_METHOD(void, set, (const std::string &key, const std::vector<swss::FieldValueTuple> &values, const std::string &op), (override));
    MOCK_METHOD(int, getFd, (), (override));
    MOCK_METHOD(uint64_t, readData, (), (override));

};


TEST(ServerSai, bulkGet)
{
    ServerSai sai;

    sai.apiInitialize(0,&test_services);

    sai_object_id_t oids[1] = {0};
    uint32_t attrcount[1] = {0};
    sai_attribute_t* attrs[1] = {0};
    sai_status_t statuses[1] = {0};

    EXPECT_NE(SAI_STATUS_SUCCESS,
            sai.bulkGet(
                SAI_OBJECT_TYPE_PORT,
                1,
                oids,
                attrcount,
                attrs,
                SAI_BULK_OP_ERROR_MODE_STOP_ON_ERROR,
                statuses));
}

TEST(ServerSai, stats_st_capability_query)
{
    SWSS_LOG_ENTER();

    ServerSai sai;

    sai.apiInitialize(0, &test_services);
    sai.m_selectableChannel = make_shared<TestServerSaiMockChannel>();
    sai.m_sai = make_shared<MockSaiInterface>();

    KeyOpFieldsValuesTuple kco;
    kfvKey(kco) = "oid:0x21000000000000";
    kfvFieldsValues(kco).push_back(make_pair("OBJECT_TYPE", "SAI_OBJECT_TYPE_PORT"));
    kfvFieldsValues(kco).push_back(make_pair("LIST_SIZE", "96"));

    EXPECT_EQ(SAI_STATUS_SUCCESS,
              sai.processStatsStCapabilityQuery(kco));
}
