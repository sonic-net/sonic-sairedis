#include "RedisRemoteSaiInterface.h"
#include "ContextConfigContainer.h"
#include "ContextConfig.h"
#include "RedisChannel.h"
#include "sairediscommon.h"

#include <gtest/gtest.h>
#include <functional>

using namespace sairedis;
using namespace std;
using namespace swss;

class TestRedisRemoteSaiInterfaceMockChannel : public RedisChannel
{
public:
  TestRedisRemoteSaiInterfaceMockChannel(_In_ const string &dbAsic,
              _In_ Channel::Callback callback) : RedisChannel(dbAsic, callback) {
      SWSS_LOG_ENTER();
    }

  sai_status_t wait(
      _In_ const string &command,
      _Out_ KeyOpFieldsValuesTuple &kco)
    {
      SWSS_LOG_ENTER();

      if (m_wait_mock)
      {
          return m_wait_mock(command, kco);
      }
      return SAI_STATUS_SUCCESS;
    }

  function<sai_status_t(const string &command, KeyOpFieldsValuesTuple &kco)> m_wait_mock;
};


TEST(RedisRemoteSaiInterface, queryStatsCapabilityNegative)
{
    auto ctx = ContextConfigContainer::loadFromFile("foo");
    auto rec = std::make_shared<Recorder>();

    RedisRemoteSaiInterface sai(ctx->get(0), nullptr, rec);

    EXPECT_EQ(SAI_STATUS_INVALID_PARAMETER,
              sai.queryStatsCapability(0,
                SAI_OBJECT_TYPE_NULL,
                0));
}

TEST(RedisRemoteSaiInterface, queryStatsStCapability)
{
    SWSS_LOG_ENTER();

    auto ctx = ContextConfigContainer::loadFromFile("foo");
    auto rec = make_shared<Recorder>();

    RedisRemoteSaiInterface sai(ctx->get(0), nullptr, rec);

    sai.m_communicationChannel = std::make_shared<TestRedisRemoteSaiInterfaceMockChannel>(
        sai.m_contextConfig->m_dbAsic,
        std::bind(&RedisRemoteSaiInterface::handleNotification, &sai, placeholders::_1, placeholders::_2, placeholders::_3));

    EXPECT_EQ(SAI_STATUS_INVALID_PARAMETER,
              sai.queryStatsStCapability(0,
                                         SAI_OBJECT_TYPE_NULL,
                                         0));

    dynamic_cast<TestRedisRemoteSaiInterfaceMockChannel& >(*sai.m_communicationChannel).m_wait_mock =
        [](const string &command, KeyOpFieldsValuesTuple &kco) -> sai_status_t
    {
        SWSS_LOG_ENTER();

        kfvFieldsValues(kco).push_back(make_pair("", "0,1"));
        kfvFieldsValues(kco).push_back(make_pair("", "0,1"));
        kfvFieldsValues(kco).push_back(make_pair("", "100,100"));
        kfvFieldsValues(kco).push_back(make_pair("", "2"));

        return SAI_STATUS_SUCCESS;
    };

    vector<sai_stat_st_capability_t> buffer;
    buffer.resize(96);
    sai_stat_st_capability_list_t stats_capability;
    stats_capability.count = static_cast<uint32_t>(buffer.size());
    stats_capability.list = buffer.data();
    EXPECT_EQ(SAI_STATUS_SUCCESS,
              sai.queryStatsStCapability(0,
                                         SAI_OBJECT_TYPE_PORT,
                                         &stats_capability));
}

TEST(RedisRemoteSaiInterface, zmqSyncFallsBackToRedisSyncWhenJsonDisablesZmq)
{
    SWSS_LOG_ENTER();

    auto cc = std::make_shared<ContextConfig>(0, "syncd", "ASIC_DB", "COUNTERS_DB", "FLEX_COUNTER_DB", "STATE_DB");
    cc->m_loadedFromJson = true;
    cc->m_zmqEnable = false;

    auto rec = make_shared<Recorder>();
    RedisRemoteSaiInterface sai(cc, nullptr, rec);

    sai_attribute_t attr;
    attr.id = SAI_REDIS_SWITCH_ATTR_REDIS_COMMUNICATION_MODE;
    attr.value.s32 = SAI_REDIS_COMMUNICATION_MODE_ZMQ_SYNC;

    EXPECT_EQ(SAI_STATUS_SUCCESS, sai.set(SAI_OBJECT_TYPE_SWITCH, SAI_NULL_OBJECT_ID, &attr));

    EXPECT_NE(dynamic_cast<RedisChannel*>(sai.m_communicationChannel.get()), nullptr);
    EXPECT_TRUE(sai.m_syncMode);
}
