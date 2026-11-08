#include "ZeroMQChannel.h"

#include "swss/logger.h"

#include <gtest/gtest.h>

#include <memory>

using namespace sairedis;

TEST(ZeroMQChannel, ctr)
{
    EXPECT_THROW(std::make_shared<ZeroMQChannel>("/invalid_ep", "/invalid_ntf_ep", nullptr), std::runtime_error);

    EXPECT_THROW(std::make_shared<ZeroMQChannel>("ipc:///tmp/valid", "/invalid_ntf_ep", nullptr), std::runtime_error);
}

TEST(ZeroMQChannel, flush)
{
    auto c = std::make_shared<ZeroMQChannel>("ipc:///tmp/valid_ep", "ipc:///tmp/valid_ntf_ep", nullptr);

    c->flush();
}

TEST(ZeroMQChannel, wait)
{
    auto c = std::make_shared<ZeroMQChannel>("ipc:///tmp/valid_ep", "ipc:///tmp/valid_ntf_ep", nullptr);

    c->setResponseTimeout(60);

    swss::KeyOpFieldsValuesTuple kco;

    EXPECT_NE(c->wait("foo", kco), SAI_STATUS_SUCCESS);
}
