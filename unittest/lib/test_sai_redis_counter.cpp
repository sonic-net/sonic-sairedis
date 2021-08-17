#include <gtest/gtest.h>

extern "C" {
#include "sai.h"
}

#include "swss/logger.h"

TEST(libsairedis, counter)
{
    sai_counter_api_t *api = nullptr;

    sai_api_query(SAI_API_COUNTER, (void**)&api);

    EXPECT_NE(api, nullptr);

    sai_object_id_t id;

    EXPECT_NE(SAI_STATUS_SUCCESS, api->create_counter(&id,0,0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->remove_counter(0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->set_counter_attribute(0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->get_counter_attribute(0,0,0));

    EXPECT_NE(SAI_STATUS_SUCCESS, api->get_counter_stats(0,0,0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->get_counter_stats_ext(0,0,0,SAI_STATS_MODE_READ,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->clear_counter_stats(0,0,0));
}
