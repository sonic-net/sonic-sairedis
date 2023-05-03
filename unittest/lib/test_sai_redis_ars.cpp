#include <gtest/gtest.h>

extern "C" {
#include "sai.h"
}

#include "swss/logger.h"

TEST(libsairedis, ars)
{
    sai_ars_api_t *api = nullptr;

    sai_api_query(SAI_API_ARS, (void**)&api);

    EXPECT_NE(api, nullptr);

    sai_object_id_t id;

    EXPECT_NE(SAI_STATUS_SUCCESS, api->create_ars(&id,0,0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->remove_ars(0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->set_ars_attribute(0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->get_ars_attribute(0,0,0));
}
