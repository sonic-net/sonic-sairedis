#include <gtest/gtest.h>

extern "C" {
#include "sai.h"
}

#include "swss/logger.h"

TEST(libsaivs, dtel)
{
    sai_dtel_api_t *api = nullptr;

    sai_api_query(SAI_API_DTEL, (void**)&api);

    EXPECT_NE(api, nullptr);

    sai_object_id_t id;

    EXPECT_NE(SAI_STATUS_SUCCESS, api->create_dtel(&id,0,0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->remove_dtel(0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->set_dtel_attribute(0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->get_dtel_attribute(0,0,0));

    EXPECT_NE(SAI_STATUS_SUCCESS, api->create_dtel_queue_report(&id,0,0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->remove_dtel_queue_report(0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->set_dtel_queue_report_attribute(0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->get_dtel_queue_report_attribute(0,0,0));

    EXPECT_NE(SAI_STATUS_SUCCESS, api->create_dtel_int_session(&id,0,0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->remove_dtel_int_session(0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->set_dtel_int_session_attribute(0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->get_dtel_int_session_attribute(0,0,0));

    EXPECT_NE(SAI_STATUS_SUCCESS, api->create_dtel_report_session(&id,0,0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->remove_dtel_report_session(0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->set_dtel_report_session_attribute(0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->get_dtel_report_session_attribute(0,0,0));

    EXPECT_NE(SAI_STATUS_SUCCESS, api->create_dtel_event(&id,0,0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->remove_dtel_event(0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->set_dtel_event_attribute(0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->get_dtel_event_attribute(0,0,0));
}
