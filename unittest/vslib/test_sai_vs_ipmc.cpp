#include <gtest/gtest.h>

extern "C" {
#include "sai.h"
}

#include "swss/logger.h"

TEST(libsaivs, ipmc)
{
    sai_ipmc_api_t *api = nullptr;

    sai_api_query(SAI_API_IPMC, (void**)&api);

    EXPECT_NE(api, nullptr);

    sai_ipmc_entry_t id;

    EXPECT_NE(SAI_STATUS_SUCCESS, api->create_ipmc_entry(&id,0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->remove_ipmc_entry(0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->set_ipmc_entry_attribute(0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->get_ipmc_entry_attribute(0,0,0));
}
