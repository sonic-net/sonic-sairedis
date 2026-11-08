#include "SaiDump.h"

using namespace syncd;

int main(int argc, char **argv)
{
    SWSS_LOG_ENTER();
    SaiDump saiDump;
    saiDump.dumpFromRedisDb(argc, argv);
    return EXIT_SUCCESS;
}