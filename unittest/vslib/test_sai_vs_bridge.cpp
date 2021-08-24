#include <gtest/gtest.h>

extern "C" {
#include "sai.h"
}

#include "swss/logger.h"

TEST(libsaivs, bridge)
{
    sai_bridge_api_t *api = nullptr;

    sai_api_query(SAI_API_BRIDGE, (void**)&api);

    EXPECT_NE(api, nullptr);

    sai_object_id_t id;

    EXPECT_NE(SAI_STATUS_SUCCESS, api->create_bridge(&id,0,0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->remove_bridge(0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->set_bridge_attribute(0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->get_bridge_attribute(0,0,0));

    EXPECT_NE(SAI_STATUS_SUCCESS, api->get_bridge_stats(0,0,0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->get_bridge_stats_ext(0,0,0,SAI_STATS_MODE_READ,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->clear_bridge_stats(0,0,0));

    EXPECT_NE(SAI_STATUS_SUCCESS, api->create_bridge_port(&id,0,0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->remove_bridge_port(0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->set_bridge_port_attribute(0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->get_bridge_port_attribute(0,0,0));

    EXPECT_NE(SAI_STATUS_SUCCESS, api->get_bridge_port_stats(0,0,0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->get_bridge_port_stats_ext(0,0,0,SAI_STATS_MODE_READ,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->clear_bridge_port_stats(0,0,0));
}
