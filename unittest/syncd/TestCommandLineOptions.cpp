#include "CommandLineOptions.h"

void testCtr()
{
    syncd::CommandLineOptions opt;

    opt.getCommandLineString();

    syncd::CommandLineOptions::startTypeStringToStartType("foo");
}
