#include <gtest/gtest.h>

extern "C" {
#include "sai.h"
}

#include "swss/logger.h"

TEST(libsaivs, switch)
{
    sai_switch_api_t *api = nullptr;

    sai_api_query(SAI_API_SWITCH, (void**)&api);

    EXPECT_NE(api, nullptr);

    sai_object_id_t id;

    EXPECT_NE(SAI_STATUS_SUCCESS, api->create_switch(&id,0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->remove_switch(0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->set_switch_attribute(0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->get_switch_attribute(0,0,0));

    EXPECT_NE(SAI_STATUS_SUCCESS, api->get_switch_stats(0,0,0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->get_switch_stats_ext(0,0,0,SAI_STATS_MODE_READ,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->clear_switch_stats(0,0,0));

    EXPECT_EQ(SAI_STATUS_NOT_IMPLEMENTED, api->switch_mdio_read(0,0,0,0,0));
    EXPECT_EQ(SAI_STATUS_NOT_IMPLEMENTED, api->switch_mdio_write(0,0,0,0,0));

    EXPECT_NE(SAI_STATUS_SUCCESS, api->create_switch_tunnel(&id,0,0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->remove_switch_tunnel(0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->set_switch_tunnel_attribute(0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->get_switch_tunnel_attribute(0,0,0));
}
