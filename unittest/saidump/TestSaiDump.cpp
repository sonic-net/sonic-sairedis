#include <gtest/gtest.h>
#include "meta/sai_serialize.h"
#include "SaiDump.h"
using namespace swss;

#define ARRAYLEN(arr) (int)(sizeof(arr) / sizeof((arr)[0]))

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
    static constexpr int64_t RDB_JSON_MAX_SIZE = 1024 * 1024 * 100;
    syncd::SaiDump m_saiDump;
    const char *cmd1[] = {"saidump", "-r", "./dump.json", "-m", "100"};
    m_saiDump.handleCmdLine(ARRAYLEN(cmd1), const_cast<char **>(cmd1));
    EXPECT_EQ(m_saiDump.getRdbJsonFile(), "./dump.json");
    EXPECT_EQ(m_saiDump.getRdbJSonSizeLimit(), RDB_JSON_MAX_SIZE);

    // reset, otherwise getopt_long could not be called correctly again
    optind = 0;
    const char *cmd2[] = {"saidump", "-g"};
    m_saiDump.handleCmdLine(ARRAYLEN(cmd2), const_cast<char **>(cmd2));
    EXPECT_EQ(m_saiDump.getDumpGraph(), true);

    optind = 0;
    const char *cmd3[] = {"saidump", "-t"};
    m_saiDump.handleCmdLine(ARRAYLEN(cmd3), const_cast<char **>(cmd3));
    EXPECT_EQ(m_saiDump.getDumpTempView(), true);
}

TEST(SaiDump, dumpFromRedisRdbJson)
{
    SWSS_LOG_ENTER();
    syncd::SaiDump m_saiDump;
    const char *cmd1[] = {"saidump", "-r", "", "-m", "100"};
    optind = 0;
    m_saiDump.handleCmdLine(ARRAYLEN(cmd1), const_cast<char **>(cmd1));
    EXPECT_EQ(SAI_STATUS_FAILURE, m_saiDump.dumpFromRedisRdbJson());
    const char *cmd2[] = {"saidump", "-r", "./dump.json", "-m", "100"};
    optind = 0;
    m_saiDump.handleCmdLine(ARRAYLEN(cmd2), const_cast<char **>(cmd2));
    EXPECT_EQ(SAI_STATUS_SUCCESS, m_saiDump.dumpFromRedisRdbJson());
    const char *cmd3[] = {"saidump", "-r", "./err.json", "-m", "100"};
    optind = 0;
    m_saiDump.handleCmdLine(ARRAYLEN(cmd3), const_cast<char **>(cmd3));
    EXPECT_EQ(SAI_STATUS_FAILURE, m_saiDump.dumpFromRedisRdbJson());
}

TEST(SaiDump, dumpFromRedisDb1)
{
    SWSS_LOG_ENTER();
    optind = 0;
    syncd::SaiDump m_saiDump;
    const char *cmd1[] = {"saidump", "-r", "./dump.json", "-m", "100"};
    m_saiDump.dumpFromRedisDb(ARRAYLEN(cmd1), const_cast<char **>(cmd1));
}

TEST(SaiDump, dumpFromRedisDb2)
{
    SWSS_LOG_ENTER();
    optind = 0;
    syncd::SaiDump m_saiDump;
    const char *cmd1[] = {"saidump", "-g", "-t", "-r", "./dump.json", "-m", "100"};
    m_saiDump.dumpFromRedisDb(ARRAYLEN(cmd1), const_cast<char **>(cmd1));
}

TEST(SaiDump, dumpGraphFun)
{
    SWSS_LOG_ENTER();
    TableDump dump;
    syncd::SaiDump m_saiDump;
    optind = 0;
    const char *cmd1[] = {"saidump", "-g", "-t"};
    m_saiDump.handleCmdLine(ARRAYLEN(cmd1), const_cast<char **>(cmd1));
    m_saiDump.dumpGraphTable(dump);
    optind = 0;
    const char *cmd2[] = {"saidump"};
    m_saiDump.handleCmdLine(ARRAYLEN(cmd2), const_cast<char **>(cmd2));
    m_saiDump.dumpGraphTable(dump);

    size_t indent = 4;
    TableMap map;
    m_saiDump.printAttributes(indent, map);
}
