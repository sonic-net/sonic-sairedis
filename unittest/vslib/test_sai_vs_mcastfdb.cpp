#include <gtest/gtest.h>

extern "C" {
#include "sai.h"
}

#include "swss/logger.h"

TEST(libsaivs, mcast_fdb)
{
    sai_mcast_fdb_api_t *api = nullptr;

    sai_api_query(SAI_API_MCAST_FDB, (void**)&api);

    EXPECT_NE(api, nullptr);

    sai_mcast_fdb_entry_t id;

    EXPECT_NE(SAI_STATUS_SUCCESS, api->create_mcast_fdb_entry(&id,0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->remove_mcast_fdb_entry(0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->set_mcast_fdb_entry_attribute(0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->get_mcast_fdb_entry_attribute(0,0,0));
}
