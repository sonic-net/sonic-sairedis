#include <gtest/gtest.h>

extern "C" {
#include "sai.h"
}

#include "swss/logger.h"

TEST(libsaivs, segmentroute_sidlist)
{
    sai_segmentroute_api_t *api = nullptr;

    sai_api_query(SAI_API_SEGMENTROUTE, (void**)&api);

    EXPECT_NE(api, nullptr);

    sai_object_id_t id;

    EXPECT_NE(SAI_STATUS_SUCCESS, api->create_segmentroute_sidlist(&id,0,0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->remove_segmentroute_sidlist(0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->set_segmentroute_sidlist_attribute(0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->get_segmentroute_sidlist_attribute(0,0,0));

    EXPECT_NE(SAI_STATUS_SUCCESS, api->create_segmentroute_sidlists(0,0,0,0,SAI_BULK_OP_ERROR_MODE_IGNORE_ERROR,0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->remove_segmentroute_sidlists(0,0,SAI_BULK_OP_ERROR_MODE_IGNORE_ERROR,0));
}
