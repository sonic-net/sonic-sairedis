#include <map>
#include <string>
#include <vector>

#include "swss/logger.h"

extern "C" {
#include <sai.h>
}

#include "../inc/sai_vs.h"

const char* profile_get_value(
        _In_ sai_switch_profile_id_t profile_id,
        _In_ const char* variable)
{
    SWSS_LOG_ENTER();

    SWSS_LOG_NOTICE("request: %s", variable);

    if (std::string(variable) == SAI_KEY_VS_SWITCH_TYPE)
    {
        return SAI_VALUE_VS_SWITCH_TYPE_MLNX2700;
    }

    return NULL;
}

int profile_get_next_value(
        _In_ sai_switch_profile_id_t profile_id,
        _Out_ const char** variable,
        _Out_ const char** value)
{
    SWSS_LOG_ENTER();

    if (value == NULL)
    {
        SWSS_LOG_INFO("resetting profile map iterator");

        return 0;
    }

    if (variable == NULL)
    {
        SWSS_LOG_WARN("variable is null");
        return -1;
    }

    SWSS_LOG_INFO("iterator reached end");
    return -1;
}

service_method_table_t test_services = {
    profile_get_value,
    profile_get_next_value
};

#define SUCCESS(x) \
    if ((x) != SAI_STATUS_SUCCESS) \
{\
    SWSS_LOG_THROW("expected success on: %s", #x);\
}

#define ASSERT_TRUE(x)\
{\
    if (!(x))\
    {\
        SWSS_LOG_THROW("assert true failed %s", #x);\
    }\
}

void on_switch_state_change(
        _In_ sai_switch_oper_status_t switch_oper_status)
{
    SWSS_LOG_ENTER();
}

void on_fdb_event(
        _In_ uint32_t count,
        _In_ sai_fdb_event_notification_data_t *data)
{
    SWSS_LOG_ENTER();
}

void on_port_state_change(
        _In_ uint32_t count,
        _In_ sai_port_oper_status_notification_t *data)
{
    SWSS_LOG_ENTER();
}

void on_switch_shutdown_request()
{
    SWSS_LOG_ENTER();
}

void on_packet_event(
        _In_ const void *buffer,
        _In_ sai_size_t buffer_size,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();
}

void test_ports()
{
    SWSS_LOG_ENTER();

    uint32_t expected_ports = 32;

    sai_attribute_t attr;

    sai_object_id_t switch_id;

    attr.id = SAI_SWITCH_ATTR_INIT_SWITCH;
    attr.value.booldata = true;

    SUCCESS(sai_metadata_sai_switch_api->create_switch(&switch_id, 1, &attr));

    attr.id = SAI_SWITCH_ATTR_PORT_NUMBER;

    SUCCESS(sai_metadata_sai_switch_api->get_switch_attribute(switch_id, 1, &attr));

    ASSERT_TRUE(attr.value.u32 == expected_ports);

    std::vector<sai_object_id_t> ports;

    ports.resize(expected_ports);

    attr.id = SAI_SWITCH_ATTR_PORT_LIST;
    attr.value.objlist.count = expected_ports;
    attr.value.objlist.list = ports.data();

    SUCCESS(sai_metadata_sai_switch_api->get_switch_attribute(switch_id, 1, &attr));

    ASSERT_TRUE(attr.value.objlist.count == expected_ports);
}

int main()
{
    swss::Logger::getInstance().setMinPrio(swss::Logger::SWSS_DEBUG);

    SWSS_LOG_ENTER();

    swss::Logger::getInstance().setMinPrio(swss::Logger::SWSS_NOTICE);

    SUCCESS(sai_api_initialize(0, (service_method_table_t*)&test_services));

    sai_metadata_apis_query(sai_api_query);

    //swss::Logger::getInstance().setMinPrio(swss::Logger::SWSS_DEBUG);

    test_ports();

    return 0;
}
