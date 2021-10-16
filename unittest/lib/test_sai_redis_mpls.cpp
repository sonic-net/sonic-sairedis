#include <gtest/gtest.h>

extern "C" {
#include "sai.h"
}

#include "swss/logger.h"

TEST(libsairedis, mpls)
{
    sai_mpls_api_t *api = nullptr;

    sai_api_query(SAI_API_MPLS, (void**)&api);

    EXPECT_NE(api, nullptr);

    sai_inseg_entry_t id ;

    EXPECT_NE(SAI_STATUS_SUCCESS, api->create_inseg_entry(&id,0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->remove_inseg_entry(0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->set_inseg_entry_attribute(0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->get_inseg_entry_attribute(0,0,0));

    EXPECT_NE(SAI_STATUS_SUCCESS, api->create_inseg_entries(0,0,0,0,SAI_BULK_OP_ERROR_MODE_IGNORE_ERROR,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->remove_inseg_entries(0,0,SAI_BULK_OP_ERROR_MODE_IGNORE_ERROR,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->set_inseg_entries_attribute(0,0,0,SAI_BULK_OP_ERROR_MODE_IGNORE_ERROR,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->get_inseg_entries_attribute(0,0,0,0,SAI_BULK_OP_ERROR_MODE_IGNORE_ERROR,0));
}
