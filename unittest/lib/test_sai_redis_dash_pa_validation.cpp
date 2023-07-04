#include <gtest/gtest.h>

extern "C" {
#include "sai.h"
#include "saiextensions.h"
}

#include "swss/logger.h"

TEST(libsairedis, dash_pa_validation)
{
    sai_dash_pa_validation_api_t *api = nullptr;

    sai_api_query((sai_api_t)SAI_API_DASH_PA_VALIDATION, (void**)&api);

    EXPECT_NE(api, nullptr);

    EXPECT_NE(SAI_STATUS_SUCCESS, api->create_pa_validation_entry(0,0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->remove_pa_validation_entry(0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->set_pa_validation_entry_attribute(0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->get_pa_validation_entry_attribute(0,0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->create_pa_validation_entries(0,0,0,0,SAI_BULK_OP_ERROR_MODE_IGNORE_ERROR,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->remove_pa_validation_entries(0,0,SAI_BULK_OP_ERROR_MODE_IGNORE_ERROR,0));
}
