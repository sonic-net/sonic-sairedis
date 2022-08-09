#include <gtest/gtest.h>

extern "C" {
#include "sai.h"
}

#include "swss/logger.h"

TEST(libsairedis, vlan)
{
    sai_vlan_api_t *api = nullptr;

    sai_api_query(SAI_API_VLAN, (void**)&api);

    EXPECT_NE(api, nullptr);

    sai_object_id_t id;

    EXPECT_NE(SAI_STATUS_SUCCESS, api->create_vlan(&id,0,0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->remove_vlan(0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->set_vlan_attribute(0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->get_vlan_attribute(0,0,0));

    EXPECT_NE(SAI_STATUS_SUCCESS, api->create_vlan_member(&id,0,0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->remove_vlan_member(0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->set_vlan_member_attribute(0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->get_vlan_member_attribute(0,0,0));

    EXPECT_NE(SAI_STATUS_SUCCESS, api->create_vlan_members(0,0,0,0,SAI_BULK_OP_ERROR_MODE_IGNORE_ERROR,0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->remove_vlan_members(0,0,SAI_BULK_OP_ERROR_MODE_IGNORE_ERROR,0));

    EXPECT_NE(SAI_STATUS_SUCCESS, api->get_vlan_stats(0,0,0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->get_vlan_stats_ext(0,0,0,SAI_STATS_MODE_READ,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->clear_vlan_stats(0,0,0));
}
