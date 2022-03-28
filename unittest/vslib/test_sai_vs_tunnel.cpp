#include <gtest/gtest.h>

extern "C" {
#include "sai.h"
}

#include "swss/logger.h"

TEST(libsaivs, tunnel)
{
    sai_tunnel_api_t *api = nullptr;

    sai_api_query(SAI_API_TUNNEL, (void**)&api);

    EXPECT_NE(api, nullptr);

    sai_object_id_t id;

    EXPECT_NE(SAI_STATUS_SUCCESS, api->create_tunnel_map(&id,0,0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->remove_tunnel_map(0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->set_tunnel_map_attribute(0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->get_tunnel_map_attribute(0,0,0));

    EXPECT_NE(SAI_STATUS_SUCCESS, api->create_tunnel(&id,0,0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->remove_tunnel(0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->set_tunnel_attribute(0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->get_tunnel_attribute(0,0,0));

    EXPECT_NE(SAI_STATUS_SUCCESS, api->get_tunnel_stats(0,0,0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->get_tunnel_stats_ext(0,0,0,SAI_STATS_MODE_READ,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->clear_tunnel_stats(0,0,0));

    EXPECT_NE(SAI_STATUS_SUCCESS, api->create_tunnel_term_table_entry(&id,0,0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->remove_tunnel_term_table_entry(0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->set_tunnel_term_table_entry_attribute(0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->get_tunnel_term_table_entry_attribute(0,0,0));

    EXPECT_NE(SAI_STATUS_SUCCESS, api->create_tunnel_map_entry(&id,0,0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->remove_tunnel_map_entry(0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->set_tunnel_map_entry_attribute(0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->get_tunnel_map_entry_attribute(0,0,0));
}
