#include <gtest/gtest.h>

extern "C" {
#include "sai.h"
}

#include "swss/logger.h"

TEST(libsairedis, hash)
{
    sai_hash_api_t *api= nullptr;

    sai_api_query(SAI_API_HASH, (void**)&api);

    EXPECT_NE(api, nullptr);

    sai_object_id_t id;

    EXPECT_NE(SAI_STATUS_SUCCESS, api->create_hash(&id,0,0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->remove_hash(0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->set_hash_attribute(0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->get_hash_attribute(0,0,0));

    EXPECT_NE(SAI_STATUS_SUCCESS, api->create_fine_grained_hash_field(&id,0,0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->remove_fine_grained_hash_field(0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->set_fine_grained_hash_field_attribute(0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->get_fine_grained_hash_field_attribute(0,0,0));
}
