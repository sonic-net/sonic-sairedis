#include <gtest/gtest.h>

extern "C" {
#include "sai.h"
}

#include "swss/logger.h"

TEST(libsaivs, wred)
{
    sai_wred_api_t *api = nullptr;

    sai_api_query(SAI_API_WRED, (void**)&api);

    EXPECT_NE(api, nullptr);

    sai_object_id_t id;

    EXPECT_NE(SAI_STATUS_SUCCESS, api->create_wred(&id,0,0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->remove_wred(0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->set_wred_attribute(0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->get_wred_attribute(0,0,0));
}
