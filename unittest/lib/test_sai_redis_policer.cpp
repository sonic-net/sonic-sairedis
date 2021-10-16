#include <gtest/gtest.h>

extern "C" {
#include "sai.h"
}

#include "swss/logger.h"

TEST(libsairedis, policer)
{
    sai_policer_api_t *api = nullptr;

    sai_api_query(SAI_API_POLICER, (void**)&api);

    EXPECT_NE(api, nullptr);

    sai_object_id_t id;

    EXPECT_NE(SAI_STATUS_SUCCESS, api->create_policer(&id,0,0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->remove_policer(0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->set_policer_attribute(0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->get_policer_attribute(0,0,0));

    EXPECT_NE(SAI_STATUS_SUCCESS, api->get_policer_stats(0,0,0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->get_policer_stats_ext(0,0,0,SAI_STATS_MODE_READ,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->clear_policer_stats(0,0,0));
}
