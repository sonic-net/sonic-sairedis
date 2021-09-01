#include <gtest/gtest.h>

extern "C" {
#include "sai.h"
}

#include "swss/logger.h"

TEST(libsaivs, samplepacket)
{
    sai_samplepacket_api_t *api = nullptr;

    sai_api_query(SAI_API_SAMPLEPACKET, (void**)&api);

    EXPECT_NE(api, nullptr);

    sai_object_id_t id;

    EXPECT_NE(SAI_STATUS_SUCCESS, api->create_samplepacket(&id,0,0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->remove_samplepacket(0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->set_samplepacket_attribute(0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->get_samplepacket_attribute(0,0,0));
}
