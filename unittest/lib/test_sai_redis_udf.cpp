#include <gtest/gtest.h>

extern "C" {
#include "sai.h"
}

#include "swss/logger.h"

TEST(libsairedis, udf)
{
    sai_udf_api_t *api = nullptr;

    sai_api_query(SAI_API_UDF, (void**)&api);

    EXPECT_NE(api, nullptr);

    sai_object_id_t id;

    EXPECT_NE(SAI_STATUS_SUCCESS, api->create_udf(&id,0,0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->remove_udf(0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->set_udf_attribute(0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->get_udf_attribute(0,0,0));

    EXPECT_NE(SAI_STATUS_SUCCESS, api->create_udf_match(&id,0,0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->remove_udf_match(0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->set_udf_match_attribute(0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->get_udf_match_attribute(0,0,0));

    EXPECT_NE(SAI_STATUS_SUCCESS, api->create_udf_group(&id,0,0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->remove_udf_group(0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->set_udf_group_attribute(0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->get_udf_group_attribute(0,0,0));
}
