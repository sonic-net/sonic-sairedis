#include <gtest/gtest.h>

extern "C" {
#include "sai.h"
}

#include "swss/logger.h"

TEST(libsaivs, l2mc_group)
{
    sai_l2mc_group_api_t *api = nullptr;

    sai_api_query(SAI_API_L2MC_GROUP, (void**)&api);

    EXPECT_NE(api, nullptr);

    sai_object_id_t id;

    EXPECT_NE(SAI_STATUS_SUCCESS, api->create_l2mc_group(&id,0,0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->remove_l2mc_group(0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->set_l2mc_group_attribute(0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->get_l2mc_group_attribute(0,0,0));

    EXPECT_NE(SAI_STATUS_SUCCESS, api->create_l2mc_group_member(&id,0,0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->remove_l2mc_group_member(0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->set_l2mc_group_member_attribute(0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->get_l2mc_group_member_attribute(0,0,0));
}
