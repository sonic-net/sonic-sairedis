#include <gtest/gtest.h>
#include "meta/sai_serialize.h"
#include "saidump.h"
#define ARGVN 5

extern CmdOptions g_cmdOptions;

TEST(SaiDump, printUsage)
{
    SWSS_LOG_ENTER();
    testing::internal::CaptureStdout();
    printUsage();
    std::string output = testing::internal::GetCapturedStdout();
    EXPECT_EQ(true, output.find("Usage: saidump [-t] [-g] [-r] [-m] [-h]") != std::string::npos);
}

TEST(SaiDump, handleCmdLine)
{
    SWSS_LOG_ENTER();
    const char *arr[] = {"saidump", "-r", "./dump.json", "-m", "100"};
    char **argv = new char *[ARGVN];

    for (int i = 0; i < ARGVN; ++i)
    {
        argv[i] = const_cast<char *>(arr[i]);
    }

    g_cmdOptions = handleCmdLine(ARGVN, argv);
    delete[] argv;
    EXPECT_EQ(g_cmdOptions.rdbJsonFile, "./dump.json");
    EXPECT_EQ(g_cmdOptions.rdbJSonSizeLimit, RDB_JSON_MAX_SIZE);
}

TEST(SaiDump, dumpFromRedisRdbJson)
{
    SWSS_LOG_ENTER();
    EXPECT_EQ(SAI_STATUS_FAILURE, dumpFromRedisRdbJson(""));
    EXPECT_EQ(SAI_STATUS_FAILURE, dumpFromRedisRdbJson("./dump.json"));
}

TEST(SaiDump, preProcessFile)
{
    SWSS_LOG_ENTER();
    EXPECT_EQ(SAI_STATUS_FAILURE, preProcessFile(""));
    g_cmdOptions.rdbJSonSizeLimit = 0;
    EXPECT_EQ(SAI_STATUS_FAILURE, preProcessFile("./dump.json"));
    g_cmdOptions.rdbJSonSizeLimit = RDB_JSON_MAX_SIZE;
    EXPECT_EQ(SAI_STATUS_SUCCESS, preProcessFile("./dump.json"));
    EXPECT_EQ(SAI_STATUS_SUCCESS, dumpFromRedisRdbJson("./dump.json"));
}

int main(int argc, char *argv[])
{
    SWSS_LOG_ENTER();
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}