#include "RedisRemoteSaiInterface.h"
#include "ContextConfigContainer.h"
#include "ContextConfig.h"
#include "RedisChannel.h"
#include "ZeroMQChannel.h"
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

// Helper: build a ContextConfig with a given zmq state and unique IPC
// endpoints, so each test stands up its own ZMQ socket files without
// colliding with siblings under /tmp.
static std::shared_ptr<ContextConfig> makeCtx(
        context_config_zmq_state_t state,
        const std::string& tag)
{
    SWSS_LOG_ENTER();

    auto cc = std::make_shared<ContextConfig>(
            0, "syncd", "ASIC_DB", "COUNTERS_DB", "FLEX_COUNTER_DB", "STATE_DB");
    cc->m_zmqEnable = state;
    cc->m_zmqEndpoint    = "ipc:///tmp/test_rrsi_" + tag + "_ep";
    cc->m_zmqNtfEndpoint = "ipc:///tmp/test_rrsi_" + tag + "_ntf_ep";
    return cc;
}

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

// -------- Init scenarios: file opinion drives the initial channel choice --------

// File explicitly enables ZMQ: init creates a ZeroMQChannel and the
// resolved communication mode locks to ZMQ_SYNC.
TEST(RedisRemoteSaiInterface, initSetsUpZmqChannelWhenJsonEnablesZmq)
{
    auto cc = makeCtx(CONTEXT_CONFIG_ZMQ_ENABLED, "init_enabled");
    auto rec = make_shared<Recorder>();
    RedisRemoteSaiInterface sai(cc, nullptr, rec);

    EXPECT_NE(dynamic_cast<ZeroMQChannel*>(sai.m_communicationChannel.get()), nullptr);
    EXPECT_EQ(sai.m_redisCommunicationMode, SAI_REDIS_COMMUNICATION_MODE_ZMQ_SYNC);
    EXPECT_TRUE(sai.m_syncMode);
}

// File is silent on zmq_enable (EMPTY): init falls to the default Redis
// channel; the resolved mode stays at the constructor default REDIS_ASYNC.
TEST(RedisRemoteSaiInterface, initSetsUpRedisChannelWhenJsonZmqIsEmpty)
{
    auto cc = makeCtx(CONTEXT_CONFIG_ZMQ_EMPTY, "init_empty");
    auto rec = make_shared<Recorder>();
    RedisRemoteSaiInterface sai(cc, nullptr, rec);

    EXPECT_NE(dynamic_cast<RedisChannel*>(sai.m_communicationChannel.get()), nullptr);
    EXPECT_EQ(sai.m_redisCommunicationMode, SAI_REDIS_COMMUNICATION_MODE_REDIS_ASYNC);
}

// File explicitly disables ZMQ: init falls to Redis (file authoritative).
TEST(RedisRemoteSaiInterface, initSetsUpRedisChannelWhenJsonDisablesZmq)
{
    auto cc = makeCtx(CONTEXT_CONFIG_ZMQ_DISABLED, "init_disabled");
    auto rec = make_shared<Recorder>();
    RedisRemoteSaiInterface sai(cc, nullptr, rec);

    EXPECT_NE(dynamic_cast<RedisChannel*>(sai.m_communicationChannel.get()), nullptr);
    EXPECT_EQ(sai.m_redisCommunicationMode, SAI_REDIS_COMMUNICATION_MODE_REDIS_ASYNC);
}

// -------- COMM_MODE attr: file ENABLED is authoritative, attr is ignored --------

// File ENABLED + attr requesting ZMQ_SYNC: redundant, the early-return
// guard fires; nothing changes.
TEST(RedisRemoteSaiInterface, commModeIgnoredWhenJsonEnabledAndAttrIsZmqSync)
{
    auto cc = makeCtx(CONTEXT_CONFIG_ZMQ_ENABLED, "ignored_zmq");
    auto rec = make_shared<Recorder>();
    RedisRemoteSaiInterface sai(cc, nullptr, rec);

    ASSERT_NE(dynamic_cast<ZeroMQChannel*>(sai.m_communicationChannel.get()), nullptr);

    sai_attribute_t attr;
    attr.id = SAI_REDIS_SWITCH_ATTR_REDIS_COMMUNICATION_MODE;
    attr.value.s32 = SAI_REDIS_COMMUNICATION_MODE_ZMQ_SYNC;

    EXPECT_EQ(SAI_STATUS_SUCCESS, sai.set(SAI_OBJECT_TYPE_SWITCH, SAI_NULL_OBJECT_ID, &attr));

    EXPECT_NE(dynamic_cast<ZeroMQChannel*>(sai.m_communicationChannel.get()), nullptr);
    EXPECT_EQ(sai.m_redisCommunicationMode, SAI_REDIS_COMMUNICATION_MODE_ZMQ_SYNC);
}

// File ENABLED + attr requesting REDIS_ASYNC: caller's attempt to downgrade
// the transport is rejected; the file's intent wins.
TEST(RedisRemoteSaiInterface, commModeIgnoredWhenJsonEnabledAndAttrIsRedisAsync)
{
    auto cc = makeCtx(CONTEXT_CONFIG_ZMQ_ENABLED, "ignored_async");
    auto rec = make_shared<Recorder>();
    RedisRemoteSaiInterface sai(cc, nullptr, rec);

    sai_attribute_t attr;
    attr.id = SAI_REDIS_SWITCH_ATTR_REDIS_COMMUNICATION_MODE;
    attr.value.s32 = SAI_REDIS_COMMUNICATION_MODE_REDIS_ASYNC;

    EXPECT_EQ(SAI_STATUS_SUCCESS, sai.set(SAI_OBJECT_TYPE_SWITCH, SAI_NULL_OBJECT_ID, &attr));

    EXPECT_NE(dynamic_cast<ZeroMQChannel*>(sai.m_communicationChannel.get()), nullptr);
    EXPECT_EQ(sai.m_redisCommunicationMode, SAI_REDIS_COMMUNICATION_MODE_ZMQ_SYNC);
}

// File ENABLED + attr requesting REDIS_SYNC: same. File authoritative.
TEST(RedisRemoteSaiInterface, commModeIgnoredWhenJsonEnabledAndAttrIsRedisSync)
{
    auto cc = makeCtx(CONTEXT_CONFIG_ZMQ_ENABLED, "ignored_sync");
    auto rec = make_shared<Recorder>();
    RedisRemoteSaiInterface sai(cc, nullptr, rec);

    sai_attribute_t attr;
    attr.id = SAI_REDIS_SWITCH_ATTR_REDIS_COMMUNICATION_MODE;
    attr.value.s32 = SAI_REDIS_COMMUNICATION_MODE_REDIS_SYNC;

    EXPECT_EQ(SAI_STATUS_SUCCESS, sai.set(SAI_OBJECT_TYPE_SWITCH, SAI_NULL_OBJECT_ID, &attr));

    EXPECT_NE(dynamic_cast<ZeroMQChannel*>(sai.m_communicationChannel.get()), nullptr);
    EXPECT_EQ(sai.m_redisCommunicationMode, SAI_REDIS_COMMUNICATION_MODE_ZMQ_SYNC);
}

// -------- COMM_MODE attr: file EMPTY, caller drives --------

// File EMPTY + attr ZMQ_SYNC: caller promotes the transport to ZMQ.
// m_zmqEnable stays EMPTY: the file did not enable ZMQ, the caller did.
TEST(RedisRemoteSaiInterface, commModePromotesToZmqWhenJsonEmpty)
{
    auto cc = makeCtx(CONTEXT_CONFIG_ZMQ_EMPTY, "empty_to_zmq");
    auto rec = make_shared<Recorder>();
    RedisRemoteSaiInterface sai(cc, nullptr, rec);

    ASSERT_NE(dynamic_cast<RedisChannel*>(sai.m_communicationChannel.get()), nullptr);

    sai_attribute_t attr;
    attr.id = SAI_REDIS_SWITCH_ATTR_REDIS_COMMUNICATION_MODE;
    attr.value.s32 = SAI_REDIS_COMMUNICATION_MODE_ZMQ_SYNC;

    EXPECT_EQ(SAI_STATUS_SUCCESS, sai.set(SAI_OBJECT_TYPE_SWITCH, SAI_NULL_OBJECT_ID, &attr));

    EXPECT_NE(dynamic_cast<ZeroMQChannel*>(sai.m_communicationChannel.get()), nullptr);
    EXPECT_EQ(sai.m_redisCommunicationMode, SAI_REDIS_COMMUNICATION_MODE_ZMQ_SYNC);

    // The file's opinion is not mutated by a runtime caller. That
    // invariant is what lets a subsequent attr toggle work; see
    // commModeCanSwitchBackWhenJsonEmpty below.
    EXPECT_EQ(sai.m_contextConfig->m_zmqEnable, CONTEXT_CONFIG_ZMQ_EMPTY);
}

// File EMPTY + attr REDIS_ASYNC: just takes the attr value.
TEST(RedisRemoteSaiInterface, commModeRedisAsyncWhenJsonEmpty)
{
    auto cc = makeCtx(CONTEXT_CONFIG_ZMQ_EMPTY, "empty_async");
    auto rec = make_shared<Recorder>();
    RedisRemoteSaiInterface sai(cc, nullptr, rec);

    sai_attribute_t attr;
    attr.id = SAI_REDIS_SWITCH_ATTR_REDIS_COMMUNICATION_MODE;
    attr.value.s32 = SAI_REDIS_COMMUNICATION_MODE_REDIS_ASYNC;

    EXPECT_EQ(SAI_STATUS_SUCCESS, sai.set(SAI_OBJECT_TYPE_SWITCH, SAI_NULL_OBJECT_ID, &attr));

    EXPECT_NE(dynamic_cast<RedisChannel*>(sai.m_communicationChannel.get()), nullptr);
    EXPECT_EQ(sai.m_redisCommunicationMode, SAI_REDIS_COMMUNICATION_MODE_REDIS_ASYNC);
    EXPECT_FALSE(sai.m_syncMode);
}

// File EMPTY + attr REDIS_SYNC: just takes the attr value, sync mode on.
TEST(RedisRemoteSaiInterface, commModeRedisSyncWhenJsonEmpty)
{
    auto cc = makeCtx(CONTEXT_CONFIG_ZMQ_EMPTY, "empty_sync");
    auto rec = make_shared<Recorder>();
    RedisRemoteSaiInterface sai(cc, nullptr, rec);

    sai_attribute_t attr;
    attr.id = SAI_REDIS_SWITCH_ATTR_REDIS_COMMUNICATION_MODE;
    attr.value.s32 = SAI_REDIS_COMMUNICATION_MODE_REDIS_SYNC;

    EXPECT_EQ(SAI_STATUS_SUCCESS, sai.set(SAI_OBJECT_TYPE_SWITCH, SAI_NULL_OBJECT_ID, &attr));

    EXPECT_NE(dynamic_cast<RedisChannel*>(sai.m_communicationChannel.get()), nullptr);
    EXPECT_EQ(sai.m_redisCommunicationMode, SAI_REDIS_COMMUNICATION_MODE_REDIS_SYNC);
    EXPECT_TRUE(sai.m_syncMode);
}

// With a file-silent context, the caller can toggle the communication
// mode back and forth via the COMM_MODE attr. The COMM_MODE handler keys
// its file-authoritative early return off m_zmqEnable; as long as that
// field reflects only what the file said (EMPTY here) and is not mutated
// by runtime callers, the early return never fires for a runtime-driven
// transition, and every toggle is honored.
TEST(RedisRemoteSaiInterface, commModeCanSwitchBackWhenJsonEmpty)
{
    auto cc = makeCtx(CONTEXT_CONFIG_ZMQ_EMPTY, "toggle");
    auto rec = make_shared<Recorder>();
    RedisRemoteSaiInterface sai(cc, nullptr, rec);

    EXPECT_NE(dynamic_cast<RedisChannel*>(sai.m_communicationChannel.get()), nullptr);
    EXPECT_EQ(sai.m_redisCommunicationMode, SAI_REDIS_COMMUNICATION_MODE_REDIS_ASYNC);

    sai_attribute_t attr;
    attr.id = SAI_REDIS_SWITCH_ATTR_REDIS_COMMUNICATION_MODE;

    // Step 1: upgrade to ZMQ_SYNC.
    attr.value.s32 = SAI_REDIS_COMMUNICATION_MODE_ZMQ_SYNC;
    EXPECT_EQ(SAI_STATUS_SUCCESS, sai.set(SAI_OBJECT_TYPE_SWITCH, SAI_NULL_OBJECT_ID, &attr));
    EXPECT_NE(dynamic_cast<ZeroMQChannel*>(sai.m_communicationChannel.get()), nullptr);
    EXPECT_EQ(sai.m_redisCommunicationMode, SAI_REDIS_COMMUNICATION_MODE_ZMQ_SYNC);

    // Step 2: downgrade back to REDIS_ASYNC. The COMM_MODE handler must
    // honor this even after a prior runtime promotion to ZMQ on a
    // file-silent context.
    attr.value.s32 = SAI_REDIS_COMMUNICATION_MODE_REDIS_ASYNC;
    EXPECT_EQ(SAI_STATUS_SUCCESS, sai.set(SAI_OBJECT_TYPE_SWITCH, SAI_NULL_OBJECT_ID, &attr));
    EXPECT_NE(dynamic_cast<RedisChannel*>(sai.m_communicationChannel.get()), nullptr);
    EXPECT_EQ(sai.m_redisCommunicationMode, SAI_REDIS_COMMUNICATION_MODE_REDIS_ASYNC);

    // Step 3: do it again, REDIS_SYNC this time, to make sure m_zmqEnable
    // hasn't been quietly mutated.
    attr.value.s32 = SAI_REDIS_COMMUNICATION_MODE_REDIS_SYNC;
    EXPECT_EQ(SAI_STATUS_SUCCESS, sai.set(SAI_OBJECT_TYPE_SWITCH, SAI_NULL_OBJECT_ID, &attr));
    EXPECT_NE(dynamic_cast<RedisChannel*>(sai.m_communicationChannel.get()), nullptr);
    EXPECT_EQ(sai.m_redisCommunicationMode, SAI_REDIS_COMMUNICATION_MODE_REDIS_SYNC);

    // Step 4: back to ZMQ once more, confirming the runtime path is
    // bidirectional through repeated toggles.
    attr.value.s32 = SAI_REDIS_COMMUNICATION_MODE_ZMQ_SYNC;
    EXPECT_EQ(SAI_STATUS_SUCCESS, sai.set(SAI_OBJECT_TYPE_SWITCH, SAI_NULL_OBJECT_ID, &attr));
    EXPECT_NE(dynamic_cast<ZeroMQChannel*>(sai.m_communicationChannel.get()), nullptr);
    EXPECT_EQ(sai.m_redisCommunicationMode, SAI_REDIS_COMMUNICATION_MODE_ZMQ_SYNC);

    // Through every toggle, the file's opinion is preserved.
    EXPECT_EQ(sai.m_contextConfig->m_zmqEnable, CONTEXT_CONFIG_ZMQ_EMPTY);
}

// -------- COMM_MODE attr: file DISABLED, file demotes ZMQ requests --------

// File DISABLED + attr ZMQ_SYNC: demote to REDIS_SYNC. This is the
// pre-existing test, updated to use the new enum.
TEST(RedisRemoteSaiInterface, zmqSyncFallsBackToRedisSyncWhenJsonDisablesZmq)
{
    SWSS_LOG_ENTER();

    auto cc = makeCtx(CONTEXT_CONFIG_ZMQ_DISABLED, "disabled_demote");
    auto rec = make_shared<Recorder>();
    RedisRemoteSaiInterface sai(cc, nullptr, rec);

    sai_attribute_t attr;
    attr.id = SAI_REDIS_SWITCH_ATTR_REDIS_COMMUNICATION_MODE;
    attr.value.s32 = SAI_REDIS_COMMUNICATION_MODE_ZMQ_SYNC;

    EXPECT_EQ(SAI_STATUS_SUCCESS, sai.set(SAI_OBJECT_TYPE_SWITCH, SAI_NULL_OBJECT_ID, &attr));

    EXPECT_NE(dynamic_cast<RedisChannel*>(sai.m_communicationChannel.get()), nullptr);
    EXPECT_TRUE(sai.m_syncMode);
    EXPECT_EQ(sai.m_redisCommunicationMode, SAI_REDIS_COMMUNICATION_MODE_REDIS_SYNC);

    // The file's opinion is unchanged.
    EXPECT_EQ(sai.m_contextConfig->m_zmqEnable, CONTEXT_CONFIG_ZMQ_DISABLED);
}

// File DISABLED + attr REDIS_ASYNC: caller's request matches the file's
// "no ZMQ" intent, so it goes through normally.
TEST(RedisRemoteSaiInterface, commModeRedisAsyncHonoredWhenJsonDisablesZmq)
{
    auto cc = makeCtx(CONTEXT_CONFIG_ZMQ_DISABLED, "disabled_async");
    auto rec = make_shared<Recorder>();
    RedisRemoteSaiInterface sai(cc, nullptr, rec);

    sai_attribute_t attr;
    attr.id = SAI_REDIS_SWITCH_ATTR_REDIS_COMMUNICATION_MODE;
    attr.value.s32 = SAI_REDIS_COMMUNICATION_MODE_REDIS_ASYNC;

    EXPECT_EQ(SAI_STATUS_SUCCESS, sai.set(SAI_OBJECT_TYPE_SWITCH, SAI_NULL_OBJECT_ID, &attr));

    EXPECT_NE(dynamic_cast<RedisChannel*>(sai.m_communicationChannel.get()), nullptr);
    EXPECT_EQ(sai.m_redisCommunicationMode, SAI_REDIS_COMMUNICATION_MODE_REDIS_ASYNC);
}

// File DISABLED + attr REDIS_SYNC: similarly honored.
TEST(RedisRemoteSaiInterface, commModeRedisSyncHonoredWhenJsonDisablesZmq)
{
    auto cc = makeCtx(CONTEXT_CONFIG_ZMQ_DISABLED, "disabled_sync");
    auto rec = make_shared<Recorder>();
    RedisRemoteSaiInterface sai(cc, nullptr, rec);

    sai_attribute_t attr;
    attr.id = SAI_REDIS_SWITCH_ATTR_REDIS_COMMUNICATION_MODE;
    attr.value.s32 = SAI_REDIS_COMMUNICATION_MODE_REDIS_SYNC;

    EXPECT_EQ(SAI_STATUS_SUCCESS, sai.set(SAI_OBJECT_TYPE_SWITCH, SAI_NULL_OBJECT_ID, &attr));

    EXPECT_NE(dynamic_cast<RedisChannel*>(sai.m_communicationChannel.get()), nullptr);
    EXPECT_EQ(sai.m_redisCommunicationMode, SAI_REDIS_COMMUNICATION_MODE_REDIS_SYNC);
    EXPECT_TRUE(sai.m_syncMode);
}

// -------- SYNC_MODE attr: forced sync depends on the resolved channel --------

// Channel is ZMQ (file ENABLED, init took the ZMQ path). The deprecated
// SYNC_MODE attr is overridden to force sync regardless of the value the
// caller passes. The guard reads m_redisCommunicationMode, not the file's
// opinion, so this works whether ZMQ came from the file or from a runtime
// attr promotion.
TEST(RedisRemoteSaiInterface, syncModeForcesSyncWhenChannelIsZmq)
{
    auto cc = makeCtx(CONTEXT_CONFIG_ZMQ_ENABLED, "syncmode_zmq");
    auto rec = make_shared<Recorder>();
    RedisRemoteSaiInterface sai(cc, nullptr, rec);

    ASSERT_EQ(sai.m_redisCommunicationMode, SAI_REDIS_COMMUNICATION_MODE_ZMQ_SYNC);

    sai_attribute_t attr;
    attr.id = SAI_REDIS_SWITCH_ATTR_SYNC_MODE;
    attr.value.booldata = false;

    EXPECT_EQ(SAI_STATUS_SUCCESS, sai.set(SAI_OBJECT_TYPE_SWITCH, SAI_NULL_OBJECT_ID, &attr));

    EXPECT_TRUE(sai.m_syncMode);
}

// Channel is Redis (file EMPTY, init took the Redis path). The deprecated
// SYNC_MODE attr is taken at face value: no override.
TEST(RedisRemoteSaiInterface, syncModeAcceptsValueWhenChannelIsRedis)
{
    auto cc = makeCtx(CONTEXT_CONFIG_ZMQ_EMPTY, "syncmode_redis");
    auto rec = make_shared<Recorder>();
    RedisRemoteSaiInterface sai(cc, nullptr, rec);

    ASSERT_EQ(sai.m_redisCommunicationMode, SAI_REDIS_COMMUNICATION_MODE_REDIS_ASYNC);

    sai_attribute_t attr;
    attr.id = SAI_REDIS_SWITCH_ATTR_SYNC_MODE;
    attr.value.booldata = false;

    EXPECT_EQ(SAI_STATUS_SUCCESS, sai.set(SAI_OBJECT_TYPE_SWITCH, SAI_NULL_OBJECT_ID, &attr));

    EXPECT_FALSE(sai.m_syncMode);
}

// A context promoted to ZMQ at runtime via the COMM_MODE attr must still
// have the SYNC_MODE override applied, the same as a context locked to
// ZMQ by the file. The override keys off the resolved channel state
// (m_redisCommunicationMode), not the file's opinion (m_zmqEnable),
// so both promotion paths reach the same SYNC_MODE behavior.
TEST(RedisRemoteSaiInterface, syncModeForcedAfterRuntimePromotionToZmq)
{
    auto cc = makeCtx(CONTEXT_CONFIG_ZMQ_EMPTY, "syncmode_promoted");
    auto rec = make_shared<Recorder>();
    RedisRemoteSaiInterface sai(cc, nullptr, rec);

    // Promote to ZMQ via attr.
    sai_attribute_t commAttr;
    commAttr.id = SAI_REDIS_SWITCH_ATTR_REDIS_COMMUNICATION_MODE;
    commAttr.value.s32 = SAI_REDIS_COMMUNICATION_MODE_ZMQ_SYNC;
    EXPECT_EQ(SAI_STATUS_SUCCESS, sai.set(SAI_OBJECT_TYPE_SWITCH, SAI_NULL_OBJECT_ID, &commAttr));

    ASSERT_EQ(sai.m_redisCommunicationMode, SAI_REDIS_COMMUNICATION_MODE_ZMQ_SYNC);

    // Try to clear sync mode via the deprecated attr.
    sai_attribute_t syncAttr;
    syncAttr.id = SAI_REDIS_SWITCH_ATTR_SYNC_MODE;
    syncAttr.value.booldata = false;
    EXPECT_EQ(SAI_STATUS_SUCCESS, sai.set(SAI_OBJECT_TYPE_SWITCH, SAI_NULL_OBJECT_ID, &syncAttr));

    // ZMQ implies sync: the override fires because the resolved channel is
    // ZMQ, regardless of how it became ZMQ.
    EXPECT_TRUE(sai.m_syncMode);
}
