#include <gtest/gtest.h>

extern "C" {
#include "sai.h"
}

#include "swss/logger.h"

TEST(libsaivs, ars_profile)
{
    sai_ars_profile_api_t *api = nullptr;

    sai_api_query(SAI_API_ARS_PROFILE, (void**)&api);

    EXPECT_NE(api, nullptr);

    sai_object_id_t id;

    EXPECT_NE(SAI_STATUS_SUCCESS, api->create_ars_profile(&id,0,0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->remove_ars_profile(0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->set_ars_profile_attribute(0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->get_ars_profile_attribute(0,0,0));
}
