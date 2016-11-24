#include <map>
#include <string>
#include <vector>

#include "swss/logger.h"

extern "C" {
#include <sai.h>
}

const char* profile_get_value(
        _In_ sai_switch_profile_id_t profile_id,
        _In_ const char* variable)
{
    SWSS_LOG_ENTER();

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
    SWSS_LOG_ERROR("expected success on: %s", #x);\
    throw std::runtime_error(std::string(#x));\
}

#define ASSERT_TRUE(x)\
{\
    if (!(x))\
    {\
        SWSS_LOG_ERROR("assert true failed %s", #x);\
    }\
}

sai_acl_api_t                *sai_acl_api;
sai_buffer_api_t             *sai_buffer_api;
sai_fdb_api_t                *sai_fdb_api;
sai_hash_api_t               *sai_hash_api;
sai_hostif_api_t             *sai_hostif_api;
sai_lag_api_t                *sai_lag_api;
sai_mirror_api_t             *sai_mirror_api;
sai_neighbor_api_t           *sai_neighbor_api;
sai_next_hop_api_t           *sai_next_hop_api;
sai_next_hop_group_api_t     *sai_next_hop_group_api;
sai_policer_api_t            *sai_policer_api;
sai_port_api_t               *sai_port_api;
sai_qos_map_api_t            *sai_qos_map_api;
sai_queue_api_t              *sai_queue_api;
sai_route_api_t              *sai_route_api;
sai_router_interface_api_t   *sai_router_interface_api;
sai_samplepacket_api_t       *sai_samplepacket_api;
sai_scheduler_api_t          *sai_scheduler_api;
sai_scheduler_group_api_t    *sai_scheduler_group_api;
sai_stp_api_t                *sai_stp_api;
sai_switch_api_t             *sai_switch_api;
sai_tunnel_api_t             *sai_tunnel_api;
sai_udf_api_t                *sai_udf_api;
sai_virtual_router_api_t     *sai_router_api;
sai_vlan_api_t               *sai_vlan_api;
sai_wred_api_t               *sai_wred_api;

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

void on_port_event(
        _In_ uint32_t count,
        _In_ sai_port_event_notification_t *data)
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

sai_switch_notification_t switch_notifications
{
    on_switch_state_change,
        on_fdb_event,
        on_port_state_change,
        on_port_event,
        on_switch_shutdown_request,
        on_packet_event
};

void init()
{
    SWSS_LOG_ENTER();

    sai_api_query(SAI_API_ACL, (void**)&sai_acl_api);
    sai_api_query(SAI_API_BUFFERS, (void**)&sai_buffer_api);
    sai_api_query(SAI_API_FDB, (void**)&sai_fdb_api);
    sai_api_query(SAI_API_HASH, (void**)&sai_hash_api);
    sai_api_query(SAI_API_HOST_INTERFACE, (void**)&sai_hostif_api);
    sai_api_query(SAI_API_LAG, (void**)&sai_lag_api);
    sai_api_query(SAI_API_MIRROR, (void**)&sai_mirror_api);
    sai_api_query(SAI_API_NEIGHBOR, (void**)&sai_neighbor_api);
    sai_api_query(SAI_API_NEXT_HOP, (void**)&sai_next_hop_api);
    sai_api_query(SAI_API_NEXT_HOP_GROUP, (void**)&sai_next_hop_group_api);
    sai_api_query(SAI_API_POLICER, (void**)&sai_policer_api);
    sai_api_query(SAI_API_PORT, (void**)&sai_port_api);
    sai_api_query(SAI_API_QOS_MAPS, (void**)&sai_qos_map_api);
    sai_api_query(SAI_API_QUEUE, (void**)&sai_queue_api);
    sai_api_query(SAI_API_ROUTE, (void**)&sai_route_api);
    sai_api_query(SAI_API_ROUTER_INTERFACE, (void**)&sai_router_interface_api);
    sai_api_query(SAI_API_SAMPLEPACKET, (void**)&sai_samplepacket_api);
    sai_api_query(SAI_API_SCHEDULER, (void**)&sai_scheduler_api);
    sai_api_query(SAI_API_SCHEDULER_GROUP, (void**)&sai_scheduler_group_api);
    sai_api_query(SAI_API_STP, (void**)&sai_stp_api);
    sai_api_query(SAI_API_SWITCH, (void**)&sai_switch_api);
    sai_api_query(SAI_API_TUNNEL, (void**)&sai_tunnel_api);
    sai_api_query(SAI_API_UDF, (void**)&sai_udf_api);
    sai_api_query(SAI_API_VIRTUAL_ROUTER, (void**)&sai_router_api);
    sai_api_query(SAI_API_VLAN, (void**)&sai_vlan_api);
    sai_api_query(SAI_API_WRED, (void**)&sai_wred_api);
}

void test_ports()
{
    SWSS_LOG_ENTER();

    uint32_t expected_ports = 32;

    sai_attribute_t attr;

    attr.id = SAI_SWITCH_ATTR_PORT_NUMBER;

    SUCCESS(sai_switch_api->get_switch_attribute(1, &attr));

    ASSERT_TRUE(attr.value.u32 == expected_ports);

    std::vector<sai_object_id_t> ports;

    ports.resize(expected_ports);

    attr.id = SAI_SWITCH_ATTR_PORT_LIST;
    attr.value.objlist.count = expected_ports;
    attr.value.objlist.list = ports.data();

    SUCCESS(sai_switch_api->get_switch_attribute(1, &attr));

    ASSERT_TRUE(attr.value.objlist.count == expected_ports);
}

int main()
{
    swss::Logger::getInstance().setMinPrio(swss::Logger::SWSS_DEBUG);

    SWSS_LOG_ENTER();

    swss::Logger::getInstance().setMinPrio(swss::Logger::SWSS_NOTICE);

    SUCCESS(sai_api_initialize(0, (service_method_table_t*)&test_services));

    init();

    SUCCESS(sai_switch_api->initialize_switch(0, "", "", &switch_notifications));

    swss::Logger::getInstance().setMinPrio(swss::Logger::SWSS_DEBUG);

    test_ports();

    return 0;
}
