#include <gtest/gtest.h>

#include "TimerWatchdog.h"

using namespace syncd;

TEST(TimerWatchdog, getWarnTimespan)
{
    TimerWatchdog wd(5000000);
    EXPECT_EQ(wd.getWarnTimespan(), 5000000);
}

TEST(TimerWatchdog, setWarnTimespanValid)
{
    TimerWatchdog wd(1000000);
    EXPECT_EQ(wd.getWarnTimespan(), 1000000);

    wd.setWarnTimespan(2000000);
    EXPECT_EQ(wd.getWarnTimespan(), 2000000);
}

TEST(TimerWatchdog, setWarnTimespanInvalidZero)
{
    TimerWatchdog wd(1000000);
    wd.setWarnTimespan(0);
    EXPECT_EQ(wd.getWarnTimespan(), 1000000);
}

TEST(TimerWatchdog, setWarnTimespanInvalidNegative)
{
    TimerWatchdog wd(1000000);
    wd.setWarnTimespan(-1);
    EXPECT_EQ(wd.getWarnTimespan(), 1000000);
}
