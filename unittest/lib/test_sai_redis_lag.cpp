#include <gtest/gtest.h>

extern "C" {
#include "sai.h"
}

#include "swss/logger.h"

TEST(libsairedis, lag)
{
    sai_lag_api_t *api = nullptr;

    sai_api_query(SAI_API_LAG, (void**)&api);

    EXPECT_NE(api, nullptr);

    sai_object_id_t id;

    EXPECT_NE(SAI_STATUS_SUCCESS, api->create_lag(&id,0,0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->remove_lag(0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->set_lag_attribute(0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->get_lag_attribute(0,0,0));

    EXPECT_NE(SAI_STATUS_SUCCESS, api->create_lag_member(&id,0,0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->remove_lag_member(0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->set_lag_member_attribute(0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->get_lag_member_attribute(0,0,0));

    EXPECT_NE(SAI_STATUS_SUCCESS, api->create_lag_members(0,0,0,0,SAI_BULK_OP_ERROR_MODE_IGNORE_ERROR,0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->remove_lag_members(0,0,SAI_BULK_OP_ERROR_MODE_IGNORE_ERROR,0));
}
