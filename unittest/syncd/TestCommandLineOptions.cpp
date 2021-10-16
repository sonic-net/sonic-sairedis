#include "CommandLineOptions.h"

#include <gtest/gtest.h>

using namespace syncd;

TEST(CommandLineOptions, getCommandLineString)
{
    syncd::CommandLineOptions opt;

    auto str = opt.getCommandLineString();

    EXPECT_EQ(str, " EnableDiagShell=NO EnableTempView=NO DisableExitSleep=NO EnableUnittests=NO"
            " EnableConsistencyCheck=NO EnableSyncMode=NO RedisCommunicationMode=redis_async"
            " EnableSaiBulkSuport=NO StartType=cold ProfileMapFile= GlobalContext=0 ContextConfig= BreakConfig=");
}

TEST(CommandLineOptions, startTypeStringToStartType)
{
    auto st = syncd::CommandLineOptions::startTypeStringToStartType("foo");

    EXPECT_EQ(st, SAI_START_TYPE_UNKNOWN);
}
