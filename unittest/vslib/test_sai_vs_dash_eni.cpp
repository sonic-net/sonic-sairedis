#include <gtest/gtest.h>

extern "C" {
#include "sai.h"
#include "saiextensions.h"
}

#include "swss/logger.h"

TEST(libsaivs, dash_eni)
{
    sai_dash_eni_api_t *api = nullptr;

    sai_api_query((sai_api_t)SAI_API_DASH_ENI, (void**)&api);

    EXPECT_NE(api, nullptr);

    sai_object_id_t id;

    EXPECT_NE(SAI_STATUS_SUCCESS, api->create_eni_ether_address_map_entry(0,0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->remove_eni_ether_address_map_entry(0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->set_eni_ether_address_map_entry_attribute(0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->get_eni_ether_address_map_entry_attribute(0,0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->create_eni_ether_address_map_entries(0,0,0,0,SAI_BULK_OP_ERROR_MODE_IGNORE_ERROR,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->remove_eni_ether_address_map_entries(0,0,SAI_BULK_OP_ERROR_MODE_IGNORE_ERROR,0));

    EXPECT_NE(SAI_STATUS_SUCCESS, api->create_eni(&id,0,0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->remove_eni(0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->set_eni_attribute(0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->get_eni_attribute(0,0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->create_enis(0,0,0,0,SAI_BULK_OP_ERROR_MODE_IGNORE_ERROR,0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->remove_enis(0,0,SAI_BULK_OP_ERROR_MODE_IGNORE_ERROR,0));
}
