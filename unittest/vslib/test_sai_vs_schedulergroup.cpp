#include <gtest/gtest.h>

extern "C" {
#include "sai.h"
}

#include "swss/logger.h"

TEST(libsaivs, scheduler_group)
{
    sai_scheduler_group_api_t *api = nullptr;

    sai_api_query(SAI_API_SCHEDULER_GROUP, (void**)&api);

    EXPECT_NE(api, nullptr);

    sai_object_id_t id;

    EXPECT_NE(SAI_STATUS_SUCCESS, api->create_scheduler_group(&id,0,0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->remove_scheduler_group(0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->set_scheduler_group_attribute(0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->get_scheduler_group_attribute(0,0,0));
}
