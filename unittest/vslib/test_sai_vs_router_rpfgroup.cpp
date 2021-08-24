#include <gtest/gtest.h>

extern "C" {
#include "sai.h"
}

#include "swss/logger.h"

TEST(libsaivs, rpf_group)
{
    sai_rpf_group_api_t *api = nullptr;

    sai_api_query(SAI_API_RPF_GROUP, (void**)&api);

    EXPECT_NE(api, nullptr);

    sai_object_id_t id;

    EXPECT_NE(SAI_STATUS_SUCCESS, api->create_rpf_group(&id,0,0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->remove_rpf_group(0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->set_rpf_group_attribute(0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->get_rpf_group_attribute(0,0,0));

    EXPECT_NE(SAI_STATUS_SUCCESS, api->create_rpf_group_member(&id,0,0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->remove_rpf_group_member(0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->set_rpf_group_member_attribute(0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->get_rpf_group_member_attribute(0,0,0));
}
