#include "NetMsgRegistrar.h"

#include "swss/logger.h"

#include <gtest/gtest.h>

using namespace saivs;

static void callback(int, struct nl_object*)
{
    SWSS_LOG_ENTER();

    // empty
}

TEST(NetMsgRegistrar, registerCallback)
{
    auto& reg = NetMsgRegistrar::getInstance();

    auto index = reg.registerCallback(callback);

    reg.unregisterCallback(index);
}

TEST(NetMsgRegistrar, unregisterAll)
{
    auto& reg = NetMsgRegistrar::getInstance();

    reg.registerCallback(callback);

    usleep(100*1000);

    reg.unregisterAll();
}

TEST(NetMsgRegistrar, resetIndex)
{
    auto& reg = NetMsgRegistrar::getInstance();

    reg.resetIndex();

    auto index = reg.registerCallback(callback);

    reg.unregisterAll();

    auto index2 = reg.registerCallback(callback);

    EXPECT_NE(index, index2);

    reg.unregisterAll();

    reg.resetIndex();

    auto index3 = reg.registerCallback(callback);

    EXPECT_EQ(index, index3);
}
