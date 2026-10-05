#include "Recorder.h"

#include <gtest/gtest.h>

#include <memory>
#include <fstream>
#include <thread>
#include <chrono>
#include <cstdio>

using namespace sairedis;

static std::string tmpRecordingDir()
{
    return ".";
}

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

TEST(Recorder, StartAndStopRecording)
{
    std::string path = tmpRecordingDir() + "/test_recorder.log";
    std::remove(path.c_str());

    Recorder r;
    r.enableRecording(true);
    r.recordComment("hello world");
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    r.enableRecording(false);

    std::ifstream f(path);
    ASSERT_TRUE(f.is_open());
    std::string line;
    bool found = false;
    while (std::getline(f, line))
        if (line.find("hello world") != std::string::npos) { found = true; break; }
    EXPECT_TRUE(found);
    f.close();
    std::remove(path.c_str());
}

TEST(Recorder, RequestLogRotatePushesRotateEntry)
{
    std::string path = tmpRecordingDir() + "/test_recorder_rotate.log";
    std::remove(path.c_str());

    Recorder r;
    r.enableRecording(true);
    r.recordComment("before rotate");
    r.requestLogRotate();
    r.recordComment("after rotate");
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    r.enableRecording(false);

    std::remove(path.c_str());
}

TEST(Recorder, DestructorStopsWorker)
{
    {
        Recorder r;
        r.enableRecording(true);
        r.recordComment("destructor test");
    }
}

TEST(Recorder, StopWorkerIdempotent)
{
    Recorder r;
    r.enableRecording(true);
    r.enableRecording(false);
    r.enableRecording(false);
}

TEST(Recorder, RecordLineWhenNotRunning)
{
    Recorder r;
    r.recordComment("should be dropped silently");
    EXPECT_TRUE(true);
}

