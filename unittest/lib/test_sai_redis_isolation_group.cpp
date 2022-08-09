#include <gtest/gtest.h>

extern "C" {
#include "sai.h"
}

#include "swss/logger.h"

TEST(libsairedis, isolation_group)
{
    sai_isolation_group_api_t *api = nullptr;

    sai_api_query(SAI_API_ISOLATION_GROUP, (void**)&api);

    EXPECT_NE(api, nullptr);

    sai_object_id_t id;

    EXPECT_NE(SAI_STATUS_SUCCESS, api->create_isolation_group(&id,0,0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->remove_isolation_group(0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->set_isolation_group_attribute(0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->get_isolation_group_attribute(0,0,0));

    EXPECT_NE(SAI_STATUS_SUCCESS, api->create_isolation_group_member(&id,0,0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->remove_isolation_group_member(0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->set_isolation_group_member_attribute(0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->get_isolation_group_member_attribute(0,0,0));
}
