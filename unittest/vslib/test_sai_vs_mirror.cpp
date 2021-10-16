#include <gtest/gtest.h>

extern "C" {
#include "sai.h"
}

#include "swss/logger.h"

TEST(libsaivs, mirror)
{
    sai_mirror_api_t *api = nullptr;

    sai_api_query(SAI_API_MIRROR, (void**)&api);

    EXPECT_NE(api, nullptr);

    sai_object_id_t id;

    EXPECT_NE(SAI_STATUS_SUCCESS, api->create_mirror_session(&id,0,0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->remove_mirror_session(0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->set_mirror_session_attribute(0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->get_mirror_session_attribute(0,0,0));
}
