#include <gtest/gtest.h>

extern "C" {
#include "sai.h"
}

#include "swss/logger.h"

TEST(libsairedis, buffer)
{
    sai_buffer_api_t *api = nullptr;

    sai_api_query(SAI_API_BUFFER, (void**)&api);

    EXPECT_NE(api, nullptr);

    sai_object_id_t id;

    EXPECT_NE(SAI_STATUS_SUCCESS, api->create_buffer_pool(&id,0,0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->remove_buffer_pool(0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->set_buffer_pool_attribute(0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->get_buffer_pool_attribute(0,0,0));

    EXPECT_NE(SAI_STATUS_SUCCESS, api->get_buffer_pool_stats(0,0,0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->get_buffer_pool_stats_ext(0,0,0,SAI_STATS_MODE_READ,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->clear_buffer_pool_stats(0,0,0));

    EXPECT_NE(SAI_STATUS_SUCCESS, api->create_ingress_priority_group(&id,0,0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->remove_ingress_priority_group(0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->set_ingress_priority_group_attribute(0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->get_ingress_priority_group_attribute(0,0,0));

    EXPECT_NE(SAI_STATUS_SUCCESS, api->get_ingress_priority_group_stats(0,0,0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->get_ingress_priority_group_stats_ext(0,0,0,SAI_STATS_MODE_READ,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->clear_ingress_priority_group_stats(0,0,0));

    EXPECT_NE(SAI_STATUS_SUCCESS, api->create_buffer_profile(&id,0,0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->remove_buffer_profile(0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->set_buffer_profile_attribute(0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->get_buffer_profile_attribute(0,0,0));
}
