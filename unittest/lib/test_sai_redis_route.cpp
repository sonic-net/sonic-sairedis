#include <gtest/gtest.h>

extern "C" {
#include "sai.h"
}

#include "swss/logger.h"

TEST(libsairedis, route)
{
    sai_route_api_t *api = nullptr;

    sai_api_query(SAI_API_ROUTE, (void**)&api);

    EXPECT_NE(api, nullptr);

    sai_route_entry_t id ;

    EXPECT_NE(SAI_STATUS_SUCCESS, api->create_route_entry(&id,0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->remove_route_entry(0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->set_route_entry_attribute(0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->get_route_entry_attribute(0,0,0));

    EXPECT_NE(SAI_STATUS_SUCCESS, api->create_route_entries(0,0,0,0,SAI_BULK_OP_ERROR_MODE_IGNORE_ERROR,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->remove_route_entries(0,0,SAI_BULK_OP_ERROR_MODE_IGNORE_ERROR,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->set_route_entries_attribute(0,0,0,SAI_BULK_OP_ERROR_MODE_IGNORE_ERROR,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->get_route_entries_attribute(0,0,0,0,SAI_BULK_OP_ERROR_MODE_IGNORE_ERROR,0));
}
