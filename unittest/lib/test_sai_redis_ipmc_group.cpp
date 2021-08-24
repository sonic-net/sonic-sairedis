#include <gtest/gtest.h>

extern "C" {
#include "sai.h"
}

#include "swss/logger.h"

TEST(libsairedis, ipmc_group)
{
    sai_ipmc_group_api_t *api = nullptr;

    sai_api_query(SAI_API_IPMC_GROUP, (void**)&api);

    EXPECT_NE(api, nullptr);

    sai_object_id_t id;

    EXPECT_NE(SAI_STATUS_SUCCESS, api->create_ipmc_group(&id,0,0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->remove_ipmc_group(0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->set_ipmc_group_attribute(0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->get_ipmc_group_attribute(0,0,0));

    EXPECT_NE(SAI_STATUS_SUCCESS, api->create_ipmc_group_member(&id,0,0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->remove_ipmc_group_member(0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->set_ipmc_group_member_attribute(0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->get_ipmc_group_member_attribute(0,0,0));
}
