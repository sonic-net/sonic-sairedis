#include "ServerSai.h"

#include "sai_serialize.h"
#include "vslib/ContextConfigContainer.h"
#include "vslib/VirtualSwitchSaiInterface.h"
#include "vslib/Sai.h"
#include "lib/Sai.h"

#include "swss/dbconnector.h"

#include "sairediscommon.h"

#include "MockSaiInterface.h"
#include "SelectableChannel.h"
#include "swss/dbconnector.h"
#include "swss/redisreply.h"

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <memory>


using namespace sairedis;

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

TEST(ServerSai, bulkGet)
{
    ServerSai sai;

    sai.apiInitialize(0,&test_services);

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

using namespace ::testing;

#ifdef MOCK_METHOD
class MockSelectableChannel : public sairedis::SelectableChannel {
public:
    MOCK_METHOD(bool, empty, (), (override));
    MOCK_METHOD(void, pop, (swss::KeyOpFieldsValuesTuple& kco, bool initViewMode), (override));
    MOCK_METHOD(void, set, (const std::string& key, const std::vector<swss::FieldValueTuple>& values, const std::string& op), (override));
    MOCK_METHOD(int, getFd, (), (override));
    MOCK_METHOD(uint64_t, readData, (), (override));
};

void clearDB()

class ServerSaiTest : public ::testing::Test
{
protected:
    ServerSai serverSai;
    MockSaiInterface* mockSai;
    MockSelectableChannel* mockSelectableChannel;

    void SetUp() override
    {
        mockSai = new MockSaiInterface();
        mockSelectableChannel = new MockSelectableChannel();

        serverSai.m_sai = mockSai;
        serverSai.m_selectableChannel = mockSelectableChannel;
    }

    void TearDown() override
    {
        delete mockSai;
        delete mockSelectableChannel;
    }
};

TEST_F(ServerSaiTest, ProcessStatsCapabilityQuery_InvalidInput)
{
    ServerSai serverSai;

    serverSai.apiInitialize(0,&test_services);

    swss::KeyOpFieldsValuesTuple kco("oid:0x21000000000000", "SET", {
        {"SAI_OBJECT_TYPE_PORT", "1"}
    });

    EXPECT_CALL(*mockSelectableChannel, set(
        StrEq("SAI_STATUS_INVALID_PARAMETER"),
        IsEmpty(),
        StrEq(REDIS_ASIC_STATE_COMMAND_STATS_CAPABILITY_RESPONSE)
    )).Times(1);

    sai_status_t status = serverSai.processStatsCapabilityQuery(kco);

    EXPECT_EQ(status, SAI_STATUS_INVALID_PARAMETER);
}

TEST_F(ServerSaiTest, ProcessStatsCapabilityQuery_ValidInputSuccess)
{
    ServerSai serverSai;

    serverSai.apiInitialize(0,&test_services);

    swss::KeyOpFieldsValuesTuple kco("oid:0x21000000000000", "SET", {
        {"SAI_OBJECT_TYPE_PORT", "1"},
        {"5"}
    });

    sai_stat_capability_t statList[5] = {
        {1, 2}, {3, 4}, {5, 6}, {7, 8}, {9, 10}
    };

    sai_stat_capability_list_t statCapList;
    statCapList.count = 5;
    statCapList.list = statList;

    EXPECT_CALL(*mockSai, queryStatsCapability(_, _, _))
        .WillOnce(DoAll(SetArgPointee<2>(statCapList), Return(SAI_STATUS_SUCCESS)));

    EXPECT_CALL(*mockSelectableChannel, set(
        StrEq("SAI_STATUS_SUCCESS"),
        AllOf(
            Contains(FieldValueTuple("STAT_ENUM", "1,3,5,7,9")),
            Contains(FieldValueTuple("STAT_MODES", "2,4,6,8,10")),
            Contains(FieldValueTuple("STAT_COUNT", "5"))
        ),
        StrEq(REDIS_ASIC_STATE_COMMAND_STATS_CAPABILITY_RESPONSE)
    )).Times(1);

    sai_status_t status = serverSai.processStatsCapabilityQuery(kco);

    EXPECT_EQ(status, SAI_STATUS_SUCCESS);
}

TEST_F(ServerSaiTest, ProcessStatsCapabilityQuery_BufferOverflow)
{
    ServerSai serverSai;

    serverSai.apiInitialize(0,&test_services);

    swss::KeyOpFieldsValuesTuple kco("oid:0x21000000000000", "SET", {
        {"SAI_OBJECT_TYPE_PORT", "1"},
        {"0"}
    });

    sai_stat_capability_list_t statCapList;
    statCapList.count = 31;

    EXPECT_CALL(*mockSai, queryStatsCapability(_, _, _))
        .WillOnce(DoAll(SetArgPointee<2>(statCapList), Return(SAI_STATUS_BUFFER_OVERFLOW)));

    EXPECT_CALL(*mockSelectableChannel, set(
        StrEq("SAI_STATUS_BUFFER_OVERFLOW"),
        Contains(FieldValueTuple("STAT_COUNT", "31")),
        StrEq(REDIS_ASIC_STATE_COMMAND_STATS_CAPABILITY_RESPONSE)
    )).Times(1);

    sai_status_t status = serverSai.processStatsCapabilityQuery(kco);

    EXPECT_EQ(status, SAI_STATUS_BUFFER_OVERFLOW);
}
#endif
