#include <gtest/gtest.h>

extern "C" {
#include "sai.h"
#include "saiextensions.h"
}

#include "swss/logger.h"

TEST(libsaivs, dash_acl)
{
    sai_dash_acl_api_t *api = nullptr;

    sai_api_query((sai_api_t)SAI_API_DASH_ACL, (void**)&api);

    EXPECT_NE(api, nullptr);

    sai_object_id_t id;

    EXPECT_NE(SAI_STATUS_SUCCESS, api->create_dash_acl_group(&id,0,0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->remove_dash_acl_group(0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->set_dash_acl_group_attribute(0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->get_dash_acl_group_attribute(0,0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->create_dash_acl_groups(0,0,0,0,SAI_BULK_OP_ERROR_MODE_IGNORE_ERROR,0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->remove_dash_acl_groups(0,0,SAI_BULK_OP_ERROR_MODE_IGNORE_ERROR,0));

    EXPECT_NE(SAI_STATUS_SUCCESS, api->create_dash_acl_rule(&id,0,0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->remove_dash_acl_rule(0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->set_dash_acl_rule_attribute(0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->get_dash_acl_rule_attribute(0,0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->create_dash_acl_rules(0,0,0,0,SAI_BULK_OP_ERROR_MODE_IGNORE_ERROR,0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->remove_dash_acl_rules(0,0,SAI_BULK_OP_ERROR_MODE_IGNORE_ERROR,0));
}
