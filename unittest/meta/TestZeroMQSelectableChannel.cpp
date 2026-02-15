#include "ZeroMQSelectableChannel.h"
#include "ZeroMQChannel.h"

#include "swss/select.h"

#include <gtest/gtest.h>

using namespace sairedis;

TEST(ZeroMQSelectableChannel, ctr)
{
    EXPECT_THROW(std::make_shared<ZeroMQSelectableChannel>("/dev_not/foo"), std::runtime_error);
}

TEST(ZeroMQSelectableChannel, empty)
{
    ZeroMQSelectableChannel c("ipc:///tmp/zmq_test");

    EXPECT_EQ(c.empty(), true);
}

TEST(ZeroMQSelectableChannel, pop)
{
    ZeroMQSelectableChannel c("ipc:///tmp/zmq_test");

    swss::KeyOpFieldsValuesTuple kco;

    EXPECT_THROW(c.pop(kco, false), std::runtime_error);
}

TEST(ZeroMQSelectableChannel, hasData)
{
    ZeroMQSelectableChannel c("ipc:///tmp/zmq_test");

    EXPECT_EQ(c.hasData(), false);
}

TEST(ZeroMQSelectableChannel, hasCachedData)
{
    ZeroMQSelectableChannel c("ipc:///tmp/zmq_test");

    EXPECT_EQ(c.hasCachedData(), false);
}

TEST(ZeroMQSelectableChannel, set)
{
    ZeroMQSelectableChannel c("ipc:///tmp/zmq_test");

    std::vector<swss::FieldValueTuple> values;

    // wrong state
    EXPECT_THROW(c.set("key", values, "op"), std::runtime_error);
}

static void cb(
        _In_ const std::string&,
        _In_ const std::string&,
        _In_ const std::vector<swss::FieldValueTuple>&)
{
    SWSS_LOG_ENTER();

    // notification callback
}

TEST(ZeroMQSelectableChannel, readData)
{
    ZeroMQChannel main("ipc:///tmp/zmq_test", "ipc:///tmp/zmq_test_ntf", cb);

    ZeroMQSelectableChannel c("ipc:///tmp/zmq_test");

    swss::Select ss;

    ss.addSelectable(&c);

    swss::Selectable *sel = NULL;

    std::vector<swss::FieldValueTuple> values;

    main.set("key", values, "command");

    int result = ss.select(&sel);

    EXPECT_EQ(result, swss::Select::OBJECT);

    swss::KeyOpFieldsValuesTuple kco;

    c.pop(kco, false);
}

