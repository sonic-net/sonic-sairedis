#include <gtest/gtest.h>

extern "C" {
#include "sai.h"
}

#include "swss/logger.h"

TEST(libsairedis, srv6)
{
    sai_srv6_api_t *api = nullptr;

    sai_api_query(SAI_API_SRV6, (void**)&api);

    EXPECT_NE(api, nullptr);

    sai_object_id_t obj_id;
    sai_my_sid_entry_t id ;

    EXPECT_NE(SAI_STATUS_SUCCESS, api->create_srv6_sidlist(&obj_id,0,0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->remove_srv6_sidlist(0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->set_srv6_sidlist_attribute(0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->get_srv6_sidlist_attribute(0,0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->create_srv6_sidlists(0,0,0,0,SAI_BULK_OP_ERROR_MODE_IGNORE_ERROR,0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->remove_srv6_sidlists(0,0,SAI_BULK_OP_ERROR_MODE_IGNORE_ERROR,0));

    EXPECT_NE(SAI_STATUS_SUCCESS, api->create_my_sid_entry(&id,0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->remove_my_sid_entry(0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->set_my_sid_entry_attribute(0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->get_my_sid_entry_attribute(0,0,0));

    EXPECT_NE(SAI_STATUS_SUCCESS, api->create_my_sid_entries(0,0,0,0,SAI_BULK_OP_ERROR_MODE_IGNORE_ERROR,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->remove_my_sid_entries(0,0,SAI_BULK_OP_ERROR_MODE_IGNORE_ERROR,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->set_my_sid_entries_attribute(0,0,0,SAI_BULK_OP_ERROR_MODE_IGNORE_ERROR,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->get_my_sid_entries_attribute(0,0,0,0,SAI_BULK_OP_ERROR_MODE_IGNORE_ERROR,0));
}
