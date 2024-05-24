#include <gtest/gtest.h>

extern "C" {
#include "sai.h"
}

#include "swss/logger.h"

TEST(libsaiproxy, poe)
{
    sai_poe_api_t *api = nullptr;

    sai_api_query(SAI_API_POE, (void**)&api);

    EXPECT_NE(api, nullptr);

    sai_object_id_t id;

    EXPECT_NE(SAI_STATUS_SUCCESS, api->create_poe_device(&id,0,0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->remove_poe_device(0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->set_poe_device_attribute(0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->get_poe_device_attribute(0,0,0));

    EXPECT_NE(SAI_STATUS_SUCCESS, api->create_poe_pse(&id,0,0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->remove_poe_pse(0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->set_poe_pse_attribute(0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->get_poe_pse_attribute(0,0,0));

    EXPECT_NE(SAI_STATUS_SUCCESS, api->create_poe_port(&id,0,0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->remove_poe_port(0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->set_poe_port_attribute(0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->get_poe_port_attribute(0,0,0));
}
