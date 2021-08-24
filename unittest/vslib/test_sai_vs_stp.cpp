#include <gtest/gtest.h>

extern "C" {
#include "sai.h"
}

#include "swss/logger.h"

TEST(libsaivs, stp)
{
    sai_stp_api_t *api = nullptr;

    sai_api_query(SAI_API_STP, (void**)&api);

    EXPECT_NE(api, nullptr);

    sai_object_id_t id;

    EXPECT_NE(SAI_STATUS_SUCCESS, api->create_stp(&id,0,0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->remove_stp(0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->set_stp_attribute(0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->get_stp_attribute(0,0,0));

    EXPECT_NE(SAI_STATUS_SUCCESS, api->create_stp_port(&id,0,0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->remove_stp_port(0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->set_stp_port_attribute(0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->get_stp_port_attribute(0,0,0));

    EXPECT_NE(SAI_STATUS_SUCCESS, api->create_stp_ports(0,0,0,0,SAI_BULK_OP_ERROR_MODE_IGNORE_ERROR,0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->remove_stp_ports(0,0,SAI_BULK_OP_ERROR_MODE_IGNORE_ERROR,0));
}
