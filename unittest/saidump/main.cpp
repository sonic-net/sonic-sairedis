#include <gtest/gtest.h>
#include "meta/sai_serialize.h"
#include "saidump.h"
#define ARGVN 5

TEST(SaiDump, printUsage)
{
    SWSS_LOG_ENTER();
    testing::internal::CaptureStdout();
    syncd::SaiDump m_saiDump;
    m_saiDump.printUsage();
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

    syncd::SaiDump m_saiDump;
    m_saiDump.handleCmdLine(ARGVN, argv);
    delete[] argv;
    EXPECT_EQ(m_saiDump.rdbJsonFile, "./dump.json");
    EXPECT_EQ(m_saiDump.rdbJSonSizeLimit, RDB_JSON_MAX_SIZE);
}


TEST(SaiDump, dumpFromRedisRdbJson)
{
    SWSS_LOG_ENTER();
    syncd::SaiDump m_saiDump;
    m_saiDump.rdbJsonFile = "";
    EXPECT_EQ(SAI_STATUS_FAILURE, m_saiDump.dumpFromRedisRdbJson());
    m_saiDump.rdbJsonFile = "./dump.json";
    EXPECT_EQ(SAI_STATUS_SUCCESS, m_saiDump.dumpFromRedisRdbJson());
}

TEST(SaiDump, preProcessFile)
{
    SWSS_LOG_ENTER();
    syncd::SaiDump m_saiDump;    
    EXPECT_EQ(SAI_STATUS_FAILURE, m_saiDump.preProcessFile(""));
    m_saiDump.rdbJSonSizeLimit = 0;
    EXPECT_EQ(SAI_STATUS_FAILURE, m_saiDump.preProcessFile("./dump.json"));
    m_saiDump.rdbJSonSizeLimit = RDB_JSON_MAX_SIZE;
    EXPECT_EQ(SAI_STATUS_SUCCESS, m_saiDump.preProcessFile("./dump.json"));
    m_saiDump.rdbJsonFile = "./dump.json";    
    EXPECT_EQ(SAI_STATUS_SUCCESS, m_saiDump.dumpFromRedisRdbJson());
}

int main(int argc, char *argv[])
{
    SWSS_LOG_ENTER();
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}