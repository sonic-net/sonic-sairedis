#include <gtest/gtest.h>

extern "C" {
#include "sai.h"
}

#include "swss/logger.h"

TEST(libsairedis, next_hop)
{
    sai_next_hop_api_t *api = nullptr;

    sai_api_query(SAI_API_NEXT_HOP, (void**)&api);

    EXPECT_NE(api, nullptr);

    sai_object_id_t id;

    EXPECT_NE(SAI_STATUS_SUCCESS, api->create_next_hop(&id,0,0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->remove_next_hop(0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->set_next_hop_attribute(0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->get_next_hop_attribute(0,0,0));
}
