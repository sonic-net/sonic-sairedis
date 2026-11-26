#include "ZeroMQSelectableChannel.h"
#include "ZeroMQChannel.h"

#include "swss/select.h"

#include <gtest/gtest.h>
#include <poll.h>
#include <unistd.h>

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

TEST(ZeroMQSelectableChannel, shutdownWithPendingRequest)
{
    // Linux abstract IPC endpoints are private to this test and leave no file.
    const std::string endpoint = "ipc://@sairedis-shutdown-" + std::to_string(getpid());
    const std::string notifications = endpoint + "-notifications";
    {
        ZeroMQChannel peer(endpoint, notifications, cb);
        ZeroMQSelectableChannel channel(endpoint);
        peer.set("first", {}, "request");
        pollfd ready = {channel.getFd(), POLLIN, 0};
        ASSERT_EQ(poll(&ready, 1, 10000), 1);
        ASSERT_TRUE(ready.revents & POLLIN);
        // Leave the request unread while the poll thread waits for the consumer.
    }

    ZeroMQChannel peer(endpoint, notifications, cb);
    peer.setResponseTimeout(10000);
    ZeroMQSelectableChannel channel(endpoint);
    peer.set("second", {}, "request");
    swss::Select select;
    select.addSelectable(&channel);
    swss::Selectable *selected = nullptr;
    ASSERT_EQ(select.select(&selected, 10000), swss::Select::OBJECT);
    ASSERT_EQ(selected, &channel);
    swss::KeyOpFieldsValuesTuple message;
    channel.pop(message, false);
    EXPECT_EQ(kfvKey(message), "second");
    channel.set("SAI_STATUS_SUCCESS", {}, "reply");
    EXPECT_EQ(peer.wait("reply", message), SAI_STATUS_SUCCESS);
}

