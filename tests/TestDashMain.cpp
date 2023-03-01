#include "TestDashEnv.h"

#include "swss/logger.h"

int main(int argc, char **argv)
{
    SWSS_LOG_ENTER();

    swss::Logger::getInstance().setMinPrio(swss::Logger::SWSS_NOTICE);

    testing::InitGoogleTest(&argc, argv);

    testing::AddGlobalTestEnvironment(TestDashEnv::instance());

    return RUN_ALL_TESTS();
}
