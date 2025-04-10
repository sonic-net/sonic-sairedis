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

class TestableServerSai : public ServerSai
{
public:

    using ServerSai::processStatsCapabilityQuery;

    void setSaiInterface(std::shared_ptr<SaiInterface> saiInterface)
    {
        SWSS_LOG_ENTER();
        m_sai = std::move(saiInterface);
    }

    void setSelectableChannel(std::shared_ptr<SelectableChannel> selectableChannel)
    {
        SWSS_LOG_ENTER();
        m_selectableChannel = std::move(selectableChannel);
    }
};

class ServerSaiTest : public ::testing::Test
{
protected:
    TestableServerSai serverSai;
    std::shared_ptr<MockSaiInterface> mockSai;
    std::shared_ptr<MockSelectableChannel> mockSelectableChannel;

    void SetUp() override
    {
        mockSai = std::make_shared<MockSaiInterface>();
        mockSelectableChannel = std::make_shared<MockSelectableChannel>();

        serverSai.setSaiInterface(mockSai);
        serverSai.setSelectableChannel(mockSelectableChannel);

    }

    void TearDown() override
    {
        mockSai.reset();
        mockSelectableChannel.reset();
    }
};

TEST_F(ServerSaiTest, ProcessStatsCapabilityQuery_ValidInputSuccess)
{
    swss::KeyOpFieldsValuesTuple kco(
        std::string("oid:0x21000000000000"),
        std::string("SET"),
        std::vector<std::pair<std::string, std::string>>{
            {std::string("type"), std::string("SAI_OBJECT_TYPE_PORT")},
            {std::string("list_size"), std::string("1")}
        }
    );

    sai_stat_capability_t statList[2] = {
        {SAI_PORT_STAT_IF_IN_UCAST_PKTS, SAI_STATS_MODE_READ}
    };

    sai_stat_capability_list_t statCapList;
    statCapList.count = 1;
    statCapList.list = statList;

    EXPECT_CALL(*mockSai, queryStatsCapability(_, _, _))
        .WillRepeatedly(DoAll(SetArgPointee<2>(statCapList), Return(SAI_STATUS_SUCCESS)));

    EXPECT_CALL(*mockSelectableChannel, set(
        StrEq("SAI_STATUS_SUCCESS"),
        AllOf(
            Contains(swss::FieldValueTuple("STAT_ENUM", "1,")),
            Contains(swss::FieldValueTuple("STAT_MODES", "1,")),
            Contains(swss::FieldValueTuple("STAT_COUNT", "1"))
        ),
        StrEq("stats_capability_response")
    )).Times(1);

    sai_status_t status = serverSai.processStatsCapabilityQuery(kco);
    EXPECT_EQ(status, SAI_STATUS_SUCCESS);
}
#endif