#include "Signal.h"

#include <gtest/gtest.h>

using namespace saivs;

TEST(Signal, notifyOne)
{
    Signal s;

    s.notifyOne();
}
