#include <gtest/gtest.h>

extern "C" {
#include "sai.h"
}

#include "swss/logger.h"

TEST(libsaivs, wred)
{
    sai_wred_api_t *vs_wred_api = nullptr;

    sai_api_query(SAI_API_WRED, (void**)&vs_wred_api);

    EXPECT_NE(vs_wred_api, nullptr);

    sai_object_id_t id;

    EXPECT_NE(SAI_STATUS_SUCCESS, vs_wred_api->create_wred(&id,0,0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, vs_wred_api->remove_wred(0));
    EXPECT_NE(SAI_STATUS_SUCCESS, vs_wred_api->set_wred_attribute(0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, vs_wred_api->get_wred_attribute(0,0,0));
}
