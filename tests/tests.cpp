#include <zmq.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>

#include "syncd/ZeroMQNotificationProducer.h"

#include "lib/ZeroMQChannel.h"

#include "meta/sai_serialize.h"

#include <unistd.h>
#include <signal.h>
#include <thread>
#include <memory>

using namespace syncd;
using namespace sairedis;

#define ASSERT_EQ(a,b) if ((a) != (b)) { SWSS_LOG_THROW("ASSERT EQ FAILED: " #a " != " #b); }

#define ASSERT_THROW(a,b)                      \
    try {                                      \
        a;                                     \
        SWSS_LOG_ERROR("ASSERT_THROW FAILED"); \
        exit(1);                               \
    }                                          \
    catch(const b &e) {                        \
    }                                          \
    catch(...) {                               \
        SWSS_LOG_THROW("ASSERT_THROW FAILED"); \
    }

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

void send_signals()
{
    SWSS_LOG_ENTER();
    pid_t pid = getpid();
    for (int i = 0; i < 11; ++i)
    {
        sleep(1);
        kill(pid, SIGHUP);
    }
};

/*
 * Test if runtime_error will be thrown if zmq wait reaches max retry due to
 * signal interrupt.
 */
static void test_zeromqchannel_eintr_errno_on_wait()
{
    SWSS_LOG_ENTER();

    std::cout << " * " << __FUNCTION__ << std::endl;

    ZeroMQChannel z("ipc:///tmp/feeds1", "ipc:///tmp/feeds2", nullptr);
    z.setResponseTimeout(60000);

    std::thread signal_thread(send_signals);

    swss::KeyOpFieldsValuesTuple kco;
    ASSERT_THROW(z.wait("foo", kco), std::runtime_error);

    signal_thread.join();
}

void sighup_handler(int signo)
{
    SWSS_LOG_ENTER();
}

int main()
{
    SWSS_LOG_ENTER();

    signal(SIGHUP, sighup_handler);

    test_zeromqchannel_destructor();

    test_zeromqchannel_first_notification();

    test_zeromqchannel_eintr_errno_on_wait();

    return 0;
}
