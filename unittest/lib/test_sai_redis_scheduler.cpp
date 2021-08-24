#include <gtest/gtest.h>

extern "C" {
#include "sai.h"
}

#include "swss/logger.h"

TEST(libsairedis, scheduler)
{
    sai_scheduler_api_t *api = nullptr;

    sai_api_query(SAI_API_SCHEDULER, (void**)&api);

    EXPECT_NE(api, nullptr);

    sai_object_id_t id;

    EXPECT_NE(SAI_STATUS_SUCCESS, api->create_scheduler(&id,0,0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->remove_scheduler(0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->set_scheduler_attribute(0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->get_scheduler_attribute(0,0,0));
}
