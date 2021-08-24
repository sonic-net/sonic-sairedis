#include <gtest/gtest.h>

extern "C" {
#include "sai.h"
}

#include "swss/logger.h"

TEST(libsairedis, hostif)
{
    sai_hostif_api_t *api= nullptr;

    sai_api_query(SAI_API_HOSTIF, (void**)&api);

    EXPECT_NE(api, nullptr);

    sai_object_id_t id;

    EXPECT_NE(SAI_STATUS_SUCCESS, api->create_hostif(&id,0,0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->remove_hostif(0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->set_hostif_attribute(0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->get_hostif_attribute(0,0,0));

    EXPECT_NE(SAI_STATUS_SUCCESS, api->create_hostif_table_entry(&id,0,0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->remove_hostif_table_entry(0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->set_hostif_table_entry_attribute(0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->get_hostif_table_entry_attribute(0,0,0));

    EXPECT_NE(SAI_STATUS_SUCCESS, api->create_hostif_trap_group(&id,0,0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->remove_hostif_trap_group(0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->set_hostif_trap_group_attribute(0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->get_hostif_trap_group_attribute(0,0,0));
    
    EXPECT_NE(SAI_STATUS_SUCCESS, api->create_hostif_trap(&id,0,0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->remove_hostif_trap(0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->set_hostif_trap_attribute(0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->get_hostif_trap_attribute(0,0,0));

    EXPECT_NE(SAI_STATUS_SUCCESS, api->create_hostif_user_defined_trap(&id,0,0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->remove_hostif_user_defined_trap(0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->set_hostif_user_defined_trap_attribute(0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->get_hostif_user_defined_trap_attribute(0,0,0));

    EXPECT_EQ(SAI_STATUS_NOT_IMPLEMENTED, api->recv_hostif_packet(0,0,0,0,0));
    EXPECT_EQ(SAI_STATUS_NOT_IMPLEMENTED, api->send_hostif_packet(0,0,0,0,0));
    EXPECT_EQ(SAI_STATUS_NOT_IMPLEMENTED, api->allocate_hostif_packet(0,0,0,0,0));
    EXPECT_EQ(SAI_STATUS_NOT_IMPLEMENTED, api->free_hostif_packet(0,0));
}
