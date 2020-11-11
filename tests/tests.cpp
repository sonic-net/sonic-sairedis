#include <zmq.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>

#include "lib/inc/ZeroMQChannel.h"
#include "syncd/ZeroMQNotificationProducer.h"

#include "meta/sai_serialize.h"

#include <thread>
#include <memory>

using namespace sairedis;
using namespace syncd;

#define ASSERT_EQ(a,b) if ((a) != (b)) { SWSS_LOG_THROW("ASSERT EQ FAILED: " #a " != " #b); }

/*
 * Test if destructor proper clean and join zeromq socket and context, and
 * break recv method.
 */
static void test_zeromqchannel_destructor()
{
    SWSS_LOG_ENTER();

    std::cout << " * " << __FUNCTION__ << std::endl;

    for (int i = 0; i < 10; i++)
    {
        ZeroMQChannel z("ipc:///tmp/feeds1", "ipc:///tmp/feeds2", nullptr);

        usleep(10*1000);
    }
}

/*
 * Test if first notification sent from notification producer will arrive at
 * zeromq channel notification thread. There is an issue with PUB/SUB inside
 * zeromq, and in our model this was changed to push/pull model.
 */
static void test_zeromqchannel_first_notification()
{
    SWSS_LOG_ENTER();

    std::cout << " * " << __FUNCTION__ << std::endl;

    for (int i = 0; i < 10; i++)
    {
        std::string rop;
        std::string rdata;

        bool got = false;

        auto callback = [&](
                const std::string& op,
                const std::string& data,
                const std::vector<swss::FieldValueTuple>& values)
        {
            SWSS_LOG_NOTICE("got: %s %s", op.c_str(), data.c_str());

            rop = op;
            rdata = data;

            got = true;
        };

        ZeroMQChannel z("ipc:///tmp/feeds1", "ipc:///tmp/feeds2", callback);

        ZeroMQNotificationProducer p("ipc:///tmp/feeds2");

        p.send("foo", "bar", {});

        int count = 0;

        while (!got && count++ < 200) // in total we will wait max 2 seconds
        {
            usleep(10*1000);
        }

        ASSERT_EQ(rop, "foo");
        ASSERT_EQ(rdata, "bar");
    }
}

int main()
{
    SWSS_LOG_ENTER();

    test_zeromqchannel_destructor();

    test_zeromqchannel_first_notification();

    return 0;
}
