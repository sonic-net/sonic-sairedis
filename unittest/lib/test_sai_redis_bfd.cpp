#include <gtest/gtest.h>

extern "C" {
#include "sai.h"
}

#include "swss/logger.h"

TEST(libsairedis, bfd)
{
    sai_bfd_api_t *api = nullptr;

    sai_api_query(SAI_API_BFD, (void**)&api);

    EXPECT_NE(api, nullptr);

    sai_object_id_t id;

    EXPECT_NE(SAI_STATUS_SUCCESS, api->create_bfd_session(&id,0,0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->remove_bfd_session(0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->set_bfd_session_attribute(0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->get_bfd_session_attribute(0,0,0));

    EXPECT_NE(SAI_STATUS_SUCCESS, api->get_bfd_session_stats(0,0,0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->get_bfd_session_stats_ext(0,0,0,SAI_STATS_MODE_READ,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->clear_bfd_session_stats(0,0,0));
}
