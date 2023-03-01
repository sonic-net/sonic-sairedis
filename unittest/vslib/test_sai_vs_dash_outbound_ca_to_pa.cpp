#include <gtest/gtest.h>

extern "C" {
#include "sai.h"
#include "saiextensions.h"
}

#include "swss/logger.h"

TEST(libsaivs, dash_ca_to_pa)
{
    sai_dash_outbound_ca_to_pa_api_t *api = nullptr;

    sai_api_query((sai_api_t)SAI_API_DASH_OUTBOUND_CA_TO_PA, (void**)&api);

    EXPECT_NE(api, nullptr);

    EXPECT_NE(SAI_STATUS_SUCCESS, api->create_outbound_ca_to_pa_entry(0,0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->remove_outbound_ca_to_pa_entry(0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->set_outbound_ca_to_pa_entry_attribute(0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->get_outbound_ca_to_pa_entry_attribute(0,0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->create_outbound_ca_to_pa_entries(0,0,0,0,SAI_BULK_OP_ERROR_MODE_IGNORE_ERROR,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->remove_outbound_ca_to_pa_entries(0,0,SAI_BULK_OP_ERROR_MODE_IGNORE_ERROR,0));
}
