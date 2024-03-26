#include <gtest/gtest.h>

extern "C" {
#include "sai.h"
#include "saiextensions.h"
}

#include "swss/logger.h"

TEST(libsaiproxy, dash_vnet)
{
    sai_dash_vnet_api_t *api = nullptr;

    sai_api_query((sai_api_t)SAI_API_DASH_VNET, (void**)&api);

    EXPECT_NE(api, nullptr);

    EXPECT_NE(SAI_STATUS_SUCCESS, api->create_vnet(0,0,0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->remove_vnet(0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->set_vnet_attribute(0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->get_vnet_attribute(0,0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->create_vnets(0,0,0,0,SAI_BULK_OP_ERROR_MODE_IGNORE_ERROR,0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->remove_vnets(0,0,SAI_BULK_OP_ERROR_MODE_IGNORE_ERROR,0));
}
