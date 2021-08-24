#include <gtest/gtest.h>

extern "C" {
#include "sai.h"
}

#include "swss/logger.h"

TEST(libsairedis, nat)
{
    sai_nat_api_t *api = nullptr;

    sai_api_query(SAI_API_NAT, (void**)&api);

    EXPECT_NE(api, nullptr);

    sai_nat_entry_t id ;

    EXPECT_NE(SAI_STATUS_SUCCESS, api->create_nat_entry(&id,0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->remove_nat_entry(0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->set_nat_entry_attribute(0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->get_nat_entry_attribute(0,0,0));

    EXPECT_NE(SAI_STATUS_SUCCESS, api->create_nat_entries(0,0,0,0,SAI_BULK_OP_ERROR_MODE_IGNORE_ERROR,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->remove_nat_entries(0,0,SAI_BULK_OP_ERROR_MODE_IGNORE_ERROR,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->set_nat_entries_attribute(0,0,0,SAI_BULK_OP_ERROR_MODE_IGNORE_ERROR,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->get_nat_entries_attribute(0,0,0,0,SAI_BULK_OP_ERROR_MODE_IGNORE_ERROR,0));

    sai_object_id_t id1;

    EXPECT_NE(SAI_STATUS_SUCCESS, api->create_nat_zone_counter(&id1,0,0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->remove_nat_zone_counter(0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->set_nat_zone_counter_attribute(0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->get_nat_zone_counter_attribute(0,0,0));
}
