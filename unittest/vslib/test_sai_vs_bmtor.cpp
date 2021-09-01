#include <gtest/gtest.h>

extern "C" {
#include "sai.h"
#include "saiextensions.h"
}

#include "swss/logger.h"

TEST(libsaivs, bmtor)
{
    sai_bmtor_api_t *api = nullptr;

    sai_api_query((sai_api_t)SAI_API_BMTOR, (void**)&api);

    EXPECT_NE(api, nullptr);

    sai_object_id_t id;

    EXPECT_NE(SAI_STATUS_SUCCESS, api->create_table_bitmap_classification_entry(&id,0,0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->remove_table_bitmap_classification_entry(0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->set_table_bitmap_classification_entry_attribute(0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->get_table_bitmap_classification_entry_attribute(0,0,0));

    EXPECT_NE(SAI_STATUS_SUCCESS, api->get_table_bitmap_classification_entry_stats(0,0,0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->get_table_bitmap_classification_entry_stats_ext(0,0,0,SAI_STATS_MODE_READ,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->clear_table_bitmap_classification_entry_stats(0,0,0));

    EXPECT_NE(SAI_STATUS_SUCCESS, api->create_table_bitmap_router_entry(&id,0,0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->remove_table_bitmap_router_entry(0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->set_table_bitmap_router_entry_attribute(0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->get_table_bitmap_router_entry_attribute(0,0,0));

    EXPECT_NE(SAI_STATUS_SUCCESS, api->get_table_bitmap_router_entry_stats(0,0,0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->get_table_bitmap_router_entry_stats_ext(0,0,0,SAI_STATS_MODE_READ,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->clear_table_bitmap_router_entry_stats(0,0,0));

    EXPECT_NE(SAI_STATUS_SUCCESS, api->create_table_meta_tunnel_entry(&id,0,0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->remove_table_meta_tunnel_entry(0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->set_table_meta_tunnel_entry_attribute(0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->get_table_meta_tunnel_entry_attribute(0,0,0));

    EXPECT_NE(SAI_STATUS_SUCCESS, api->get_table_meta_tunnel_entry_stats(0,0,0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->get_table_meta_tunnel_entry_stats_ext(0,0,0,SAI_STATS_MODE_READ,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->clear_table_meta_tunnel_entry_stats(0,0,0));
}
