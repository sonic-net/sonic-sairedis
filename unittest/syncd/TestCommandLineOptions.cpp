#include "CommandLineOptions.h"
#include "CommandLineOptionsParser.h"

#include <gtest/gtest.h>
#include <algorithm>

using namespace syncd;

TEST(CommandLineOptions, getCommandLineString)
{
    syncd::CommandLineOptions opt;

    auto str = opt.getCommandLineString();

    EXPECT_EQ(str, " EnableDiagShell=NO EnableTempView=NO DisableExitSleep=NO EnableUnittests=NO"
            " EnableConsistencyCheck=NO EnableSyncMode=NO RedisCommunicationMode=redis_async"
            " EnableSaiBulkSuport=NO StartType=cold ProfileMapFile= GlobalContext=0 ContextConfig= BreakConfig="
            " WatchdogWarnTimeSpan=30000000");
}

TEST(CommandLineOptions, startTypeStringToStartType)
{
    auto st = syncd::CommandLineOptions::startTypeStringToStartType("foo");

    EXPECT_EQ(st, SAI_START_TYPE_UNKNOWN);
}

TEST(CommandLineOptionsParser, parseCommandLine)
{
    char arg1[] = "test";
    char arg2[] = "-w";
    char arg3[] = "1000";
    std::vector<char *> args = {arg1, arg2, arg3};

    auto opt = syncd::CommandLineOptionsParser::parseCommandLine((int)args.size(), args.data());
    EXPECT_EQ(opt->m_watchdogWarnTimeSpan, 1000);
}
