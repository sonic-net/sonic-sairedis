#include <gtest/gtest.h>

extern "C" {
#include "sai.h"
}

#include "swss/logger.h"

TEST(libsaivs, port)
{
    sai_port_api_t *api = nullptr;

    sai_api_query(SAI_API_PORT, (void**)&api);

    EXPECT_NE(api, nullptr);

    sai_object_id_t id;

    EXPECT_NE(SAI_STATUS_SUCCESS, api->create_port(&id,0,0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->remove_port(0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->set_port_attribute(0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->get_port_attribute(0,0,0));

    EXPECT_NE(SAI_STATUS_SUCCESS, api->get_port_stats(0,0,0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->get_port_stats_ext(0,0,0,SAI_STATS_MODE_READ,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->clear_port_stats(0,0,0));

    EXPECT_EQ(SAI_STATUS_NOT_IMPLEMENTED, api->clear_port_all_stats(0));

    EXPECT_NE(SAI_STATUS_SUCCESS, api->create_port_pool(&id,0,0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->remove_port_pool(0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->set_port_pool_attribute(0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->get_port_pool_attribute(0,0,0));

    EXPECT_NE(SAI_STATUS_SUCCESS, api->get_port_pool_stats(0,0,0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->get_port_pool_stats_ext(0,0,0,SAI_STATS_MODE_READ,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->clear_port_pool_stats(0,0,0));

    EXPECT_NE(SAI_STATUS_SUCCESS, api->create_port_connector(&id,0,0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->remove_port_connector(0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->set_port_connector_attribute(0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->get_port_connector_attribute(0,0,0));

    EXPECT_NE(SAI_STATUS_SUCCESS, api->create_port_serdes(&id,0,0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->remove_port_serdes(0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->set_port_serdes_attribute(0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->get_port_serdes_attribute(0,0,0));
}
