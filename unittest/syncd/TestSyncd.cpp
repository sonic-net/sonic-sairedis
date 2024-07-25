#include "Syncd.h"
#include "MockableSaiInterface.h"
#include "CommandLineOptions.h"
#include "sairediscommon.h"
#include "SelectableChannel.h"
#include "swss/dbconnector.h"
#include "swss/redisreply.h"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

using namespace syncd;

class MockSelectableChannel : public sairedis::SelectableChannel {
public:
    MOCK_METHOD(bool, empty, (), (override));
    MOCK_METHOD(void, pop, (swss::KeyOpFieldsValuesTuple& kco, bool initViewMode), (override));
    MOCK_METHOD(void, set, (const std::string& key, const std::vector<swss::FieldValueTuple>& values, const std::string& op), (override));
    MOCK_METHOD(int, getFd, (), (override));
    MOCK_METHOD(uint64_t, readData, (), (override));
};

void clearDB()
{
    SWSS_LOG_ENTER();

    swss::DBConnector db("ASIC_DB", 0, true);
    swss::RedisReply r(&db, "FLUSHALL", REDIS_REPLY_STATUS);

    r.checkStatusOK();
}

class SyncdTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        clearDB();
    }
    void TearDown() override
    {
        clearDB();
    }
};

TEST_F(SyncdTest, processNotifySyncd)
{
    auto sai = std::make_shared<MockableSaiInterface>();
    auto opt = std::make_shared<syncd::CommandLineOptions>();
    opt->m_enableTempView = true;
    opt->m_startType = SAI_START_TYPE_FASTFAST_BOOT;
    syncd::Syncd syncd_object(sai, opt, false);

    MockSelectableChannel consumer;
    EXPECT_CALL(consumer, empty()).WillOnce(testing::Return(true));
    EXPECT_CALL(consumer, pop(testing::_, testing::_)).WillOnce(testing::Invoke([](swss::KeyOpFieldsValuesTuple& kco, bool initViewMode) {
        kfvKey(kco) = SYNCD_APPLY_VIEW;
        kfvOp(kco) = REDIS_ASIC_STATE_COMMAND_NOTIFY;
    }));
    syncd_object.processEvent(consumer);
}
