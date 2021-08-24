#include <gtest/gtest.h>

extern "C" {
#include "sai.h"
}

#include "swss/logger.h"

TEST(libsaivs, debug_counter)
{
    sai_debug_counter_api_t *api = nullptr;

    sai_api_query(SAI_API_DEBUG_COUNTER, (void**)&api);

    EXPECT_NE(api, nullptr);

    sai_object_id_t id;

    EXPECT_NE(SAI_STATUS_SUCCESS, api->create_debug_counter(&id,0,0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->remove_debug_counter(0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->set_debug_counter_attribute(0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->get_debug_counter_attribute(0,0,0));
}
