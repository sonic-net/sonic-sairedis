#include "ShareMemorySelectableChannel.h"
#include "ShareMemoryChannel.h"

#include "swss/select.h"

#include <gtest/gtest.h>

using namespace sairedis;

TEST(ShmSelectableChannel, empty)
{
    ShareMemorySelectableChannel c("shm_test");

    EXPECT_EQ(c.empty(), true);
}

TEST(ShmSelectableChannel, pop)
{
    ShareMemorySelectableChannel c("shm_test");

    swss::KeyOpFieldsValuesTuple kco;

    EXPECT_THROW(c.pop(kco, false), std::runtime_error);
}

TEST(ShmSelectableChannel, hasData)
{
    ShareMemorySelectableChannel c("shm_test");

    EXPECT_EQ(c.hasData(), false);
}

TEST(ShmSelectableChannel, hasCachedData)
{
    ShareMemorySelectableChannel c("shm_test");

    EXPECT_EQ(c.hasCachedData(), false);
}

TEST(ShmSelectableChannel, set)
{
    ShareMemorySelectableChannel c("shm_test");

    std::vector<swss::FieldValueTuple> values;

    // The behavior of ShmSelectableChannel is diffeent with ZeroMQSelectableChannel, set will success even there is no one listening.
    c.set("key", values, "op");
}

static void cb(
        _In_ const std::string&,
        _In_ const std::string&,
        _In_ const std::vector<swss::FieldValueTuple>&)
{
    SWSS_LOG_ENTER();

    // notification callback
}

TEST(ShmSelectableChannel, readData)
{
    ShareMemoryChannel main("shm_test", "shm_test_ntf", cb);

    ShareMemorySelectableChannel c("shm_test");

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

