#include <gtest/gtest.h>

extern "C" {
#include "sai.h"
}

#include "swss/logger.h"

TEST(libsairedis, system_port)
{
    sai_system_port_api_t *api = nullptr;

    sai_api_query(SAI_API_SYSTEM_PORT, (void**)&api);

    EXPECT_NE(api, nullptr);

    sai_object_id_t id;

    EXPECT_NE(SAI_STATUS_SUCCESS, api->create_system_port(&id,0,0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->remove_system_port(0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->set_system_port_attribute(0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->get_system_port_attribute(0,0,0));
}
