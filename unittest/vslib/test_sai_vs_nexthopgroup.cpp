#include <gtest/gtest.h>

extern "C" {
#include "sai.h"
}

#include "swss/logger.h"

TEST(libsaivs, next_hop_group)
{
    sai_next_hop_group_api_t *api = nullptr;

    sai_api_query(SAI_API_NEXT_HOP_GROUP, (void**)&api);

    EXPECT_NE(api, nullptr);

    sai_object_id_t id;

    EXPECT_NE(SAI_STATUS_SUCCESS, api->create_next_hop_group(&id,0,0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->remove_next_hop_group(0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->set_next_hop_group_attribute(0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->get_next_hop_group_attribute(0,0,0));

    EXPECT_NE(SAI_STATUS_SUCCESS, api->create_next_hop_group_member(&id,0,0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->remove_next_hop_group_member(0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->set_next_hop_group_member_attribute(0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->get_next_hop_group_member_attribute(0,0,0));

    EXPECT_NE(SAI_STATUS_SUCCESS, api->create_next_hop_group_members(0,0,0,0,SAI_BULK_OP_ERROR_MODE_IGNORE_ERROR,0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->remove_next_hop_group_members(0,0,SAI_BULK_OP_ERROR_MODE_IGNORE_ERROR,0));

    EXPECT_NE(SAI_STATUS_SUCCESS, api->create_next_hop_group_map(&id,0,0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->remove_next_hop_group_map(0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->set_next_hop_group_map_attribute(0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->get_next_hop_group_map_attribute(0,0,0));

}
