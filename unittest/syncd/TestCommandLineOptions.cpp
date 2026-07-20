#include "CommandLineOptions.h"
#include "CommandLineOptionsParser.h"

#include <gtest/gtest.h>
#include <algorithm>

using namespace syncd;

const std::string expected_usage =
R"(Usage: syncd [-d] [-p profile] [-t type] [-u] [-S] [-U] [-C] [-s] [-z mode] [-l] [-R] [-g idx] [-x contextConfig] [-b breakConfig] [-B supportingBulkCounters] [-G] [-h]
    -d --diag
        Enable diagnostic shell
    -p --profile profile
        Provide profile map file
    -t --startType type
        Specify start type (cold|warm|fast|fastfast|express)
    -u --useTempView
        Use temporary view between init and apply
    -S --disableExitSleep
        Disable sleep when syncd crashes
    -U --enableUnittests
        Metadata enable unittests
    -C --enableConsistencyCheck
        Enable consisteny check DB vs ASIC after comparison logic
    -s --syncMode
        Enable synchronous mode (depreacated, use -z)
    -z --redisCommunicationMode
        Redis communication mode (redis_async|redis_sync|zmq_sync), default: redis_async
    -l --enableBulk
        Enable SAI Bulk support
    -R --asyncRec
        Enable asynchronous ASIC_DB writes (only effective with ZMQ southbound)
    -g --globalContext
        Global context index to load from context config file
    -x --contextConfig
        Context configuration file
    -b --breakConfig
        Comparison logic 'break before make' configuration file
    -w --watchdogWarnTimeSpan
        Watchdog time span (in microseconds) for normal operations
    -W --watchdogInitTimeSpan
        Watchdog time span (in microseconds) for init phase (default: same as -w)
    -B --supportingBulkCounters
        Counter groups those support bulk polling
    -G --enablePerPortCounterDiscovery
        Enable counter-group discovery during counter add operations
    -a --enableAttrVersionCheck
        Enable attribute SAI version check when performing SAI discovery
    -h --help
        Print out this message
)";

TEST(CommandLineOptions, getCommandLineString)
{
    syncd::CommandLineOptions opt;

    auto str = opt.getCommandLineString();

    EXPECT_EQ(str, " EnableDiagShell=NO EnableTempView=NO DisableExitSleep=NO EnableUnittests=NO"
            " EnableConsistencyCheck=NO EnableSyncMode=NO EnableAsyncRec=NO RedisCommunicationMode=redis_async"
            " EnableSaiBulkSuport=NO StartType=cold ProfileMapFile= GlobalContext=0 ContextConfig= BreakConfig="
            " WatchdogWarnTimeSpan=30000000 WatchdogInitTimeSpan=30000000 SupportingBulkCounters="
            " EnablePerPortCounterDiscovery=NO EnableAttrVersionCheck=NO");
}

TEST(CommandLineOptions, startTypeStringToStartType)
{
    auto st = syncd::CommandLineOptions::startTypeStringToStartType("foo");

    EXPECT_EQ(st, SAI_START_TYPE_UNKNOWN);
}

TEST(CommandLineOptionsParser, printUsage)
{
    std::stringstream buffer;
    std::streambuf *sbuf = std::cout.rdbuf();
    std::cout.rdbuf(buffer.rdbuf());
    syncd::CommandLineOptionsParser::printUsage();
    std::cout.rdbuf(sbuf);

    EXPECT_EQ(expected_usage, buffer.str());
}

TEST(CommandLineOptionsParser, parseCommandLine)
{
    char arg1[] = "test";
    char arg2[] = "-w";
    char arg3[] = "1000";
    char arg4[] = "-B";
    char arg5[] = "WATERMARK";
    char arg6[] = "-G";
    std::vector<char *> args = {arg1, arg2, arg3, arg4, arg5, arg6};

    auto opt = syncd::CommandLineOptionsParser::parseCommandLine((int)args.size(), args.data());
    EXPECT_EQ(opt->m_watchdogWarnTimeSpan, 1000);
    EXPECT_EQ(opt->m_watchdogInitTimeSpan, 1000);
    EXPECT_EQ(opt->supportingBulkCounterGroups, "WATERMARK");
    EXPECT_TRUE(opt->m_enablePerPortCounterDiscovery);
}

TEST(CommandLineOptionsParser, parseCommandLineAsyncRec)
{
    char arg1[] = "test";
    char arg2[] = "-R";
    std::vector<char *> args = {arg1, arg2};

    auto opt = syncd::CommandLineOptionsParser::parseCommandLine((int)args.size(), args.data());
    EXPECT_TRUE(opt->m_enableAsyncRec);
}

TEST(CommandLineOptionsParser, parseCommandLineAsyncRecDefaultsOff)
{
    char arg1[] = "test";
    std::vector<char *> args = {arg1};

    auto opt = syncd::CommandLineOptionsParser::parseCommandLine((int)args.size(), args.data());
    EXPECT_FALSE(opt->m_enableAsyncRec);
}

TEST(CommandLineOptionsParser, parseCommandLineInitTimeout)
{
    char arg1[] = "test";
    char arg2[] = "-w";
    char arg3[] = "30000000";
    char arg4[] = "-W";
    char arg5[] = "150000000";
    std::vector<char *> args = {arg1, arg2, arg3, arg4, arg5};

    auto opt = syncd::CommandLineOptionsParser::parseCommandLine((int)args.size(), args.data());
    EXPECT_EQ(opt->m_watchdogWarnTimeSpan, 30000000);
    EXPECT_EQ(opt->m_watchdogInitTimeSpan, 150000000);
}

TEST(CommandLineOptionsParser, parseCommandLineNeitherTimeout)
{
    char arg1[] = "test";
    std::vector<char *> args = {arg1};

    auto opt = syncd::CommandLineOptionsParser::parseCommandLine((int)args.size(), args.data());
    EXPECT_EQ(opt->m_watchdogWarnTimeSpan, 30000000);
    EXPECT_EQ(opt->m_watchdogInitTimeSpan, 30000000);
}

TEST(CommandLineOptionsParser, parseCommandLineOnlyInitTimeout)
{
    char arg1[] = "test";
    char arg2[] = "-W";
    char arg3[] = "150000000";
    std::vector<char *> args = {arg1, arg2, arg3};

    auto opt = syncd::CommandLineOptionsParser::parseCommandLine((int)args.size(), args.data());
    EXPECT_EQ(opt->m_watchdogWarnTimeSpan, 30000000);
    EXPECT_EQ(opt->m_watchdogInitTimeSpan, 150000000);
}

TEST(CommandLineOptionsParser, parseCommandLineIsRepeatable)
{
    // Each parseCommandLine call must be independent of any earlier call in the same
    // process: parsing one argument vector must fully reset the parser so a later parse
    // sees only its own arguments. Exercise several parses back to back and check that
    // each yields exactly its own result.

    {
        char arg1[] = "test";
        char arg2[] = "-R";
        std::vector<char *> args = {arg1, arg2};

        auto opt = syncd::CommandLineOptionsParser::parseCommandLine((int)args.size(), args.data());
        EXPECT_TRUE(opt->m_enableAsyncRec);
    }

    {
        char arg1[] = "test";
        char arg2[] = "-w";
        char arg3[] = "30000000";
        char arg4[] = "-W";
        char arg5[] = "150000000";
        std::vector<char *> args = {arg1, arg2, arg3, arg4, arg5};

        auto opt = syncd::CommandLineOptionsParser::parseCommandLine((int)args.size(), args.data());
        EXPECT_EQ(opt->m_watchdogWarnTimeSpan, 30000000);
        EXPECT_EQ(opt->m_watchdogInitTimeSpan, 150000000);
        EXPECT_FALSE(opt->m_enableAsyncRec);
    }

    {
        char arg1[] = "test";
        std::vector<char *> args = {arg1};

        auto opt = syncd::CommandLineOptionsParser::parseCommandLine((int)args.size(), args.data());
        EXPECT_EQ(opt->m_watchdogWarnTimeSpan, 30000000);
        EXPECT_EQ(opt->m_watchdogInitTimeSpan, 30000000);
        EXPECT_FALSE(opt->m_enableAsyncRec);
    }
}
