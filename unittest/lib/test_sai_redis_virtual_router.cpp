#include <gtest/gtest.h>

extern "C" {
#include "sai.h"
}

#include "swss/logger.h"

TEST(libsairedis, virtual_router)
{
    sai_virtual_router_api_t *api = nullptr;

    sai_api_query(SAI_API_VIRTUAL_ROUTER, (void**)&api);

    EXPECT_NE(api, nullptr);

    sai_object_id_t id;

    EXPECT_NE(SAI_STATUS_SUCCESS, api->create_virtual_router(&id,0,0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->remove_virtual_router(0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->set_virtual_router_attribute(0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->get_virtual_router_attribute(0,0,0));
}
