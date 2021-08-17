#include <gtest/gtest.h>

extern "C" {
#include "sai.h"
}

#include "swss/logger.h"

TEST(libsaivs, tam)
{
    sai_tam_api_t *api = nullptr;

    sai_api_query(SAI_API_TAM, (void**)&api);

    EXPECT_NE(api, nullptr);

    sai_object_id_t id;

    EXPECT_NE(SAI_STATUS_SUCCESS, api->create_tam(&id,0,0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->remove_tam(0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->set_tam_attribute(0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->get_tam_attribute(0,0,0));

    EXPECT_NE(SAI_STATUS_SUCCESS, api->create_tam_math_func(&id,0,0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->remove_tam_math_func(0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->set_tam_math_func_attribute(0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->get_tam_math_func_attribute(0,0,0));

    EXPECT_NE(SAI_STATUS_SUCCESS, api->create_tam_report(&id,0,0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->remove_tam_report(0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->set_tam_report_attribute(0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->get_tam_report_attribute(0,0,0));

    EXPECT_NE(SAI_STATUS_SUCCESS, api->create_tam_event_threshold(&id,0,0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->remove_tam_event_threshold(0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->set_tam_event_threshold_attribute(0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->get_tam_event_threshold_attribute(0,0,0));

    EXPECT_NE(SAI_STATUS_SUCCESS, api->create_tam_int(&id,0,0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->remove_tam_int(0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->set_tam_int_attribute(0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->get_tam_int_attribute(0,0,0));

    EXPECT_NE(SAI_STATUS_SUCCESS, api->create_tam_tel_type(&id,0,0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->remove_tam_tel_type(0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->set_tam_tel_type_attribute(0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->get_tam_tel_type_attribute(0,0,0));

    EXPECT_NE(SAI_STATUS_SUCCESS, api->create_tam_transport(&id,0,0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->remove_tam_transport(0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->set_tam_transport_attribute(0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->get_tam_transport_attribute(0,0,0));

    EXPECT_NE(SAI_STATUS_SUCCESS, api->create_tam_telemetry(&id,0,0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->remove_tam_telemetry(0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->set_tam_telemetry_attribute(0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->get_tam_telemetry_attribute(0,0,0));

    EXPECT_NE(SAI_STATUS_SUCCESS, api->create_tam_collector(&id,0,0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->remove_tam_collector(0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->set_tam_collector_attribute(0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->get_tam_collector_attribute(0,0,0));

    EXPECT_NE(SAI_STATUS_SUCCESS, api->create_tam_event_action(&id,0,0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->remove_tam_event_action(0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->set_tam_event_action_attribute(0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->get_tam_event_action_attribute(0,0,0));

    EXPECT_NE(SAI_STATUS_SUCCESS, api->create_tam_event(&id,0,0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->remove_tam_event(0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->set_tam_event_attribute(0,0));
    EXPECT_NE(SAI_STATUS_SUCCESS, api->get_tam_event_attribute(0,0,0));
}
