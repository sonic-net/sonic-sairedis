#include <gtest/gtest.h>

extern "C" {
#include "sai.h"
}

#include "swss/logger.h"

TEST(libsairedis, fdb)
{
    sai_fdb_api_t *api = nullptr;

    sai_api_query(SAI_API_FDB, (void**)&api);

    EXPECT_NE(api, nullptr);

    sai_fdb_entry_t id;

    EXPECT_NE(SAI_STATUS_SUCCESS, api->create_fdb_entry(&id,0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->remove_fdb_entry(0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->set_fdb_entry_attribute(0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->get_fdb_entry_attribute(0,0,0));

    EXPECT_NE(SAI_STATUS_SUCCESS, api->flush_fdb_entries(0,0,0));

    EXPECT_NE(SAI_STATUS_SUCCESS, api->create_fdb_entries(0,0,0,0,SAI_BULK_OP_ERROR_MODE_IGNORE_ERROR,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->remove_fdb_entries(0,0,SAI_BULK_OP_ERROR_MODE_IGNORE_ERROR,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->set_fdb_entries_attribute(0,0,0,SAI_BULK_OP_ERROR_MODE_IGNORE_ERROR,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->get_fdb_entries_attribute(0,0,0,0,SAI_BULK_OP_ERROR_MODE_IGNORE_ERROR,0));
}
