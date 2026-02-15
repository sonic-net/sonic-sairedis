#include "Recorder.h"

#include <gtest/gtest.h>

#include <memory>

using namespace sairedis;

TEST(Recorder, requestLogRotate)
{
    Recorder rec;

    rec.enableRecording(true);

    rec.recordComment("foo");

    int code = rename("sairedis.rec", "sairedis.rec.1");

    EXPECT_EQ(code, 0);

    EXPECT_NE(access("sairedis.rec", F_OK),0);

    rec.requestLogRotate();

    EXPECT_EQ(access("sairedis.rec", F_OK),0);

    rec.recordComment("bar");
}
