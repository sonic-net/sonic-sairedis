#include "TestClient.h"

int main()
{
    swss::Logger::getInstance().setMinPrio(swss::Logger::SWSS_DEBUG);

    SWSS_LOG_ENTER();

    swss::Logger::getInstance().setMinPrio(swss::Logger::SWSS_NOTICE);

    TestClient tc;

    tc.test_create_switch();

    tc.test_create_vlan();

    tc.test_bulk_create_vlan();

    tc.test_query_api();

    tc.test_fdb_flush();

    tc.test_stats();

    return EXIT_SUCCESS;
}
