#include <gtest/gtest.h>

extern "C" {
#include "sai.h"
}

#include "swss/logger.h"

TEST(libsaivs, router_interface)
{
    sai_router_interface_api_t *api = nullptr;

    sai_api_query(SAI_API_ROUTER_INTERFACE, (void**)&api);

    EXPECT_NE(api, nullptr);

    sai_object_id_t id;

    EXPECT_NE(SAI_STATUS_SUCCESS, api->create_router_interface(&id,0,0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->remove_router_interface(0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->set_router_interface_attribute(0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->get_router_interface_attribute(0,0,0));

    EXPECT_NE(SAI_STATUS_SUCCESS, api->get_router_interface_stats(0,0,0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->get_router_interface_stats_ext(0,0,0,SAI_STATS_MODE_READ,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->clear_router_interface_stats(0,0,0));
}
