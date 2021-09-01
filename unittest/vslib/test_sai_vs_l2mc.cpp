#include <gtest/gtest.h>

extern "C" {
#include "sai.h"
}

#include "swss/logger.h"

TEST(libsaivs, l2mc)
{
    sai_l2mc_api_t *api = nullptr;

    sai_api_query(SAI_API_L2MC, (void**)&api);

    EXPECT_NE(api, nullptr);

    sai_l2mc_entry_t id ;

    EXPECT_NE(SAI_STATUS_SUCCESS, api->create_l2mc_entry(&id,0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->remove_l2mc_entry(0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->set_l2mc_entry_attribute(0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->get_l2mc_entry_attribute(0,0,0));
}
