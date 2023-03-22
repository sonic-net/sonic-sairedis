#include <gtest/gtest.h>

extern "C" {
#include "sai.h"
#include "saiextensions.h"
}

#include "swss/logger.h"

TEST(libsairedis, dash_outbound_routing)
{
    sai_dash_outbound_routing_api_t *api = nullptr;

    sai_api_query((sai_api_t)SAI_API_DASH_OUTBOUND_ROUTING, (void**)&api);

    EXPECT_NE(api, nullptr);

    EXPECT_NE(SAI_STATUS_SUCCESS, api->create_outbound_routing_entry(0,0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->remove_outbound_routing_entry(0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->set_outbound_routing_entry_attribute(0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->get_outbound_routing_entry_attribute(0,0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->create_outbound_routing_entries(0,0,0,0,SAI_BULK_OP_ERROR_MODE_IGNORE_ERROR,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->remove_outbound_routing_entries(0,0,SAI_BULK_OP_ERROR_MODE_IGNORE_ERROR,0));
}
