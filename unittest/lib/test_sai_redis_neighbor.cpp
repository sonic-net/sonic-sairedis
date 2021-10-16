#include <gtest/gtest.h>

extern "C" {
#include "sai.h"
}

#include "swss/logger.h"

TEST(libsairedis, neighbor)
{
    sai_neighbor_api_t *api = nullptr;

    sai_api_query(SAI_API_NEIGHBOR, (void**)&api);

    EXPECT_NE(api, nullptr);

    sai_neighbor_entry_t id ;

    EXPECT_NE(SAI_STATUS_SUCCESS, api->create_neighbor_entry(&id,0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->remove_neighbor_entry(0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->set_neighbor_entry_attribute(0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->get_neighbor_entry_attribute(0,0,0));

    EXPECT_EQ(SAI_STATUS_NOT_IMPLEMENTED, api->remove_all_neighbor_entries(0));
}
