#include <gtest/gtest.h>

extern "C" {
#include "sai.h"
}

#include "swss/logger.h"

TEST(libsaivs, qos_map)
{
    sai_qos_map_api_t *api = nullptr;

    sai_api_query(SAI_API_QOS_MAP, (void**)&api);

    EXPECT_NE(api, nullptr);

    sai_object_id_t id;

    EXPECT_NE(SAI_STATUS_SUCCESS, api->create_qos_map(&id,0,0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->remove_qos_map(0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->set_qos_map_attribute(0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->get_qos_map_attribute(0,0,0));
}
