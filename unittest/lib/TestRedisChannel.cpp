#include "RedisChannel.h"
#include "sairediscommon.h"

#include "swss/notificationproducer.h"

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
