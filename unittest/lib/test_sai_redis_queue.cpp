#include <gtest/gtest.h>

extern "C" {
#include "sai.h"
}

#include "swss/logger.h"

TEST(libsairedis, queue)
{
    sai_queue_api_t *api = nullptr;

    sai_api_query(SAI_API_QUEUE, (void**)&api);

    EXPECT_NE(api, nullptr);

    sai_object_id_t id;

    EXPECT_NE(SAI_STATUS_SUCCESS, api->create_queue(&id,0,0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->remove_queue(0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->set_queue_attribute(0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->get_queue_attribute(0,0,0));

    EXPECT_NE(SAI_STATUS_SUCCESS, api->get_queue_stats(0,0,0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->get_queue_stats_ext(0,0,0,SAI_STATS_MODE_READ,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->clear_queue_stats(0,0,0));
}
