#include <gtest/gtest.h>

extern "C" {
#include "sai.h"
}

#include "swss/logger.h"

TEST(libsairedis, acl)
{
    sai_acl_api_t *api = nullptr;

    sai_api_query(SAI_API_ACL, (void**)&api);

    EXPECT_NE(api, nullptr);

    sai_object_id_t id;

    EXPECT_NE(SAI_STATUS_SUCCESS, api->create_acl_table(&id,0,0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->remove_acl_table(0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->set_acl_table_attribute(0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->get_acl_table_attribute(0,0,0));

    EXPECT_NE(SAI_STATUS_SUCCESS, api->create_acl_entry(&id,0,0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->remove_acl_entry(0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->set_acl_entry_attribute(0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->get_acl_entry_attribute(0,0,0));

    EXPECT_NE(SAI_STATUS_SUCCESS, api->create_acl_counter(&id,0,0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->remove_acl_counter(0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->set_acl_counter_attribute(0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->get_acl_counter_attribute(0,0,0));

    EXPECT_NE(SAI_STATUS_SUCCESS, api->create_acl_range(&id,0,0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->remove_acl_range(0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->set_acl_range_attribute(0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->get_acl_range_attribute(0,0,0));

    EXPECT_NE(SAI_STATUS_SUCCESS, api->create_acl_table_group(&id,0,0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->remove_acl_table_group(0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->set_acl_table_group_attribute(0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->get_acl_table_group_attribute(0,0,0));

    EXPECT_NE(SAI_STATUS_SUCCESS, api->create_acl_table_group_member(&id,0,0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->remove_acl_table_group_member(0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->set_acl_table_group_member_attribute(0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->get_acl_table_group_member_attribute(0,0,0));
}
