#include "RedisChannel.h"
#include "sairediscommon.h"

#include "sai_serialize.h"

#include "swss/notificationproducer.h"
#include "swss/producertable.h"
#include "swss/consumertable.h"
#include "swss/select.h"

#include <gtest/gtest.h>

#include <memory>

using namespace sairedis;

static std::string g_op;
static std::string g_data;

static void callback(
        _In_ const std::string& op,
        _In_ const std::string& data,
        _In_ const std::vector<swss::FieldValueTuple>& values)
{
    SWSS_LOG_ENTER();

    g_op = op;
    g_data = data;
}

TEST(RedisChannel, reset)
{
    RedisChannel rc("ASIC_DB", callback);

    EXPECT_NE(nullptr, rc.getDbConnector());
}

TEST(RedisChannel, notificationThreadFunction)
{
    RedisChannel rc("ASIC_DB", callback);

    rc.setResponseTimeout(10);

    auto db = std::make_shared<swss::DBConnector>("ASIC_DB", 0);

    swss::NotificationProducer p(db.get(), REDIS_TABLE_NOTIFICATIONS);

    std::vector<swss::FieldValueTuple> vals;
    p.send("foo", "bar", vals);

    usleep(200*1000);

    EXPECT_EQ(g_op, "foo");
}

TEST(RedisChannel, flush)
{
    RedisChannel rc("ASIC_DB", callback);

    rc.flush();
}

class RedisChannelWait: public ::testing::Test
{
    protected:

        void SetUp() override
        {
            SWSS_LOG_ENTER();

            m_db = std::make_shared<swss::DBConnector>("ASIC_DB", 0);
            m_syncd = std::make_shared<swss::ProducerTable>(m_db.get(), REDIS_TABLE_GETRESPONSE);

            drain();

            m_channel = std::make_shared<RedisChannel>("ASIC_DB", callback);
            m_channel->setResponseTimeout(100);
        }

        // responses left by other tests

        void drain()
        {
            SWSS_LOG_ENTER();

            swss::ConsumerTable consumer(m_db.get(), REDIS_TABLE_GETRESPONSE);

            swss::Select s;
            s.addSelectable(&consumer);

            swss::Selectable *sel;

            while (s.select(&sel, 10) == swss::Select::OBJECT)
            {
                swss::KeyOpFieldsValuesTuple kco;

                consumer.pop(kco);
            }
        }

        // syncd answers a request

        void respond(
                _In_ const std::string& command,
                _In_ const std::vector<swss::FieldValueTuple>& values = {})
        {
            SWSS_LOG_ENTER();

            m_syncd->set(sai_serialize_status(SAI_STATUS_SUCCESS), values, command);
        }

        sai_status_t wait(
                _In_ const std::string& command)
        {
            SWSS_LOG_ENTER();

            return m_channel->wait(command, m_kco);
        }

        std::string field()
        {
            SWSS_LOG_ENTER();

            return kfvFieldsValues(m_kco).size() == 1 ? fvField(kfvFieldsValues(m_kco)[0]) : "";
        }

        std::shared_ptr<swss::DBConnector> m_db;
        std::shared_ptr<swss::ProducerTable> m_syncd;
        std::shared_ptr<RedisChannel> m_channel;
        swss::KeyOpFieldsValuesTuple m_kco;
};

TEST_F(RedisChannelWait, dropsLateResponsesOfFailedWaits)
{
    // two requests time out while syncd is busy

    EXPECT_EQ(SAI_STATUS_FAILURE, wait(REDIS_ASIC_STATE_COMMAND_GETRESPONSE));
    EXPECT_EQ(SAI_STATUS_FAILURE, wait(REDIS_ASIC_STATE_COMMAND_GETRESPONSE));

    // syncd catches up: the empty response of a SET, a GET's response, then the current GET's response

    respond(REDIS_ASIC_STATE_COMMAND_GETRESPONSE);
    respond(REDIS_ASIC_STATE_COMMAND_GETRESPONSE, { { "late", "2" } });
    respond(REDIS_ASIC_STATE_COMMAND_GETRESPONSE, { { "current", "3" } });

    EXPECT_EQ(SAI_STATUS_SUCCESS, wait(REDIS_ASIC_STATE_COMMAND_GETRESPONSE));
    EXPECT_EQ("current", field());

    // nothing is pending any more

    respond(REDIS_ASIC_STATE_COMMAND_GETRESPONSE, { { "next", "4" } });

    EXPECT_EQ(SAI_STATUS_SUCCESS, wait(REDIS_ASIC_STATE_COMMAND_GETRESPONSE));
    EXPECT_EQ("next", field());
}

TEST_F(RedisChannelWait, failsWithoutReturningDroppedResponse)
{
    EXPECT_EQ(SAI_STATUS_FAILURE, wait(REDIS_ASIC_STATE_COMMAND_GETRESPONSE));

    // syncd answers request 1 late and is still busy with request 2

    respond(REDIS_ASIC_STATE_COMMAND_GETRESPONSE, { { "late", "1" } });

    EXPECT_EQ(SAI_STATUS_FAILURE, wait(REDIS_ASIC_STATE_COMMAND_GETRESPONSE));

    EXPECT_EQ("", kfvKey(m_kco));
    EXPECT_EQ("", kfvOp(m_kco));
    EXPECT_EQ(0u, kfvFieldsValues(m_kco).size());

    // both late responses are dropped before the response of request 3

    respond(REDIS_ASIC_STATE_COMMAND_GETRESPONSE, { { "late", "2" } });
    respond(REDIS_ASIC_STATE_COMMAND_GETRESPONSE, { { "current", "3" } });

    EXPECT_EQ(SAI_STATUS_SUCCESS, wait(REDIS_ASIC_STATE_COMMAND_GETRESPONSE));
    EXPECT_EQ("current", field());
}

TEST_F(RedisChannelWait, countsLateResponsesPerType)
{
    // a GET and then an FDB flush time out

    EXPECT_EQ(SAI_STATUS_FAILURE, wait(REDIS_ASIC_STATE_COMMAND_GETRESPONSE));
    EXPECT_EQ(SAI_STATUS_FAILURE, wait(REDIS_ASIC_STATE_COMMAND_FLUSHRESPONSE));

    // syncd answers both late, then the next GET

    respond(REDIS_ASIC_STATE_COMMAND_GETRESPONSE, { { "late", "1" } });
    respond(REDIS_ASIC_STATE_COMMAND_FLUSHRESPONSE);
    respond(REDIS_ASIC_STATE_COMMAND_GETRESPONSE, { { "current", "2" } });

    // the GET drops both late responses, the flush one while waiting for its own type

    EXPECT_EQ(SAI_STATUS_SUCCESS, wait(REDIS_ASIC_STATE_COMMAND_GETRESPONSE));
    EXPECT_EQ("current", field());

    // the dropped flush response was accounted: the next one is returned

    respond(REDIS_ASIC_STATE_COMMAND_FLUSHRESPONSE);

    EXPECT_EQ(SAI_STATUS_SUCCESS, wait(REDIS_ASIC_STATE_COMMAND_FLUSHRESPONSE));
}
