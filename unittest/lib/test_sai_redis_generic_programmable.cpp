#include <gtest/gtest.h>

extern "C" {
#include "sai.h"
}

#include "swss/logger.h"

TEST(libsairedis, generic_programmable)
{
    sai_generic_programmable_api_t *api = nullptr;

    sai_api_query(SAI_API_GENERIC_PROGRAMMABLE, (void**)&api);

    EXPECT_NE(api, nullptr);

    sai_object_id_t obj_id;

    EXPECT_NE(SAI_STATUS_SUCCESS, api->create_generic_programmable(&obj_id,0,0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->remove_generic_programmable(0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->set_generic_programmable_attribute(0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->get_generic_programmable_attribute(0,0,0));
}
