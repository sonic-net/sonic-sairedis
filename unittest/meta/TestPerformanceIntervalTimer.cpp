#include "PerformanceIntervalTimer.h"

#include <gtest/gtest.h>

#include <memory>

using namespace sairediscommon;

TEST(PerformanceIntervalTimer, inc)
{
    PerformanceIntervalTimer p("foo", 2);

    p.inc();
    p.inc(10);
}
