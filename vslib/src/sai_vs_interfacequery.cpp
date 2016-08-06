#include "sai_vs.h"

#include <string.h>

std::recursive_mutex g_recursive_mutex;

service_method_table_t g_services;
bool                   g_apiInitialized = false;

sai_status_t sai_api_initialize(
        _In_ uint64_t flags,
        _In_ const service_method_table_t* services)
{
    std::lock_guard<std::recursive_mutex> lock(g_recursive_mutex);

    SWSS_LOG_ENTER();

    if ((NULL == services) || (NULL == services->profile_get_next_value) || (NULL == services->profile_get_value))
    {
        SWSS_LOG_ERROR("Invalid services handle passed to SAI API initialize");
        return SAI_STATUS_INVALID_PARAMETER;
    }

    memcpy(&g_services, services, sizeof(g_services));

    if (0 != flags)
    {
        SWSS_LOG_ERROR("Invalid flags passed to SAI API initialize");
        return SAI_STATUS_INVALID_PARAMETER;
    }

    g_apiInitialized = true;

    return SAI_STATUS_SUCCESS;
}

sai_status_t sai_log_set(
        _In_ sai_api_t sai_api_id,
        _In_ sai_log_level_t log_level)
{
    std::lock_guard<std::recursive_mutex> lock(g_recursive_mutex);

    SWSS_LOG_ENTER();

    switch (log_level)
    {
        case SAI_LOG_DEBUG:
            break;

        case SAI_LOG_INFO:
            break;

        case SAI_LOG_NOTICE:
            break;

        case SAI_LOG_WARN:
            break;

        case SAI_LOG_ERROR:
            break;

        case SAI_LOG_CRITICAL:
            break;

        default:
            SWSS_LOG_ERROR("Invalid log level %d", log_level);
            return SAI_STATUS_INVALID_PARAMETER;
    }

    switch (sai_api_id)
    {
        case SAI_API_SWITCH:
            break;

        case SAI_API_PORT:
            break;

        case SAI_API_FDB:
            break;

        case SAI_API_VLAN:
            break;

        case SAI_API_VIRTUAL_ROUTER:
            break;

        case SAI_API_ROUTE:
            break;

        case SAI_API_NEXT_HOP:
            break;

        case SAI_API_NEXT_HOP_GROUP:
            break;

        case SAI_API_ROUTER_INTERFACE:
            break;

        case SAI_API_NEIGHBOR:
            break;

        case SAI_API_ACL:
            break;

        case SAI_API_HOST_INTERFACE:
            break;

        case SAI_API_MIRROR:
            break;

        case SAI_API_SAMPLEPACKET:
            break;

        case SAI_API_STP:
            break;

        case SAI_API_LAG:
            break;

        default:
            SWSS_LOG_ERROR("Invalid API type %d", sai_api_id);
            return SAI_STATUS_INVALID_PARAMETER;
    }

    return SAI_STATUS_SUCCESS;
}

sai_status_t sai_api_query(
        _In_ sai_api_t sai_api_id,
        _Out_ void** api_method_table)
{
    std::lock_guard<std::recursive_mutex> lock(g_recursive_mutex);

    SWSS_LOG_ENTER();

    if (NULL == api_method_table)
    {
        SWSS_LOG_ERROR("NULL method table passed to SAI API initialize");
        return SAI_STATUS_INVALID_PARAMETER;
    }

    if (!g_apiInitialized)
    {
        SWSS_LOG_ERROR("SAI API not initialized before calling API query");
        return SAI_STATUS_UNINITIALIZED;
    }

    switch (sai_api_id) {
        case SAI_API_BUFFERS:
            *(const sai_buffer_api_t**)api_method_table = &vs_buffer_api;
            return SAI_STATUS_SUCCESS;

        case SAI_API_HASH:
            *(const sai_hash_api_t**)api_method_table = &vs_hash_api;
            return SAI_STATUS_SUCCESS;

        case SAI_API_SWITCH:
            *(const sai_switch_api_t**)api_method_table = &vs_switch_api;
            return SAI_STATUS_SUCCESS;

        case SAI_API_PORT:
            *(const sai_port_api_t**)api_method_table = &vs_port_api;
            return SAI_STATUS_SUCCESS;

        case SAI_API_FDB:
            *(const sai_fdb_api_t**)api_method_table = &vs_fdb_api;
            return SAI_STATUS_SUCCESS;

        case SAI_API_VLAN:
            *(const sai_vlan_api_t**)api_method_table = &vs_vlan_api;
            return SAI_STATUS_SUCCESS;

        case SAI_API_WRED:
            *(const sai_wred_api_t**)api_method_table = &vs_wred_api;
            return SAI_STATUS_SUCCESS;

        case SAI_API_VIRTUAL_ROUTER:
            *(const sai_virtual_router_api_t**)api_method_table = &vs_router_api;
            return SAI_STATUS_SUCCESS;

        case SAI_API_ROUTE:
            *(const sai_route_api_t**)api_method_table = &vs_route_api;
            return SAI_STATUS_SUCCESS;

        case SAI_API_NEXT_HOP:
            *(const sai_next_hop_api_t**)api_method_table = &vs_next_hop_api;
            return SAI_STATUS_SUCCESS;

        case SAI_API_NEXT_HOP_GROUP:
            *(const sai_next_hop_group_api_t**)api_method_table = &vs_next_hop_group_api;
            return SAI_STATUS_SUCCESS;

        case SAI_API_ROUTER_INTERFACE:
            *(const sai_router_interface_api_t**)api_method_table = &vs_router_interface_api;
            return SAI_STATUS_SUCCESS;

        case SAI_API_NEIGHBOR:
            *(const sai_neighbor_api_t**)api_method_table = &vs_neighbor_api;
            return SAI_STATUS_SUCCESS;

        case SAI_API_ACL:
            *(const sai_acl_api_t**)api_method_table = &vs_acl_api;
            return SAI_STATUS_SUCCESS;

        case SAI_API_HOST_INTERFACE:
            *(const sai_hostif_api_t**)api_method_table = &vs_host_interface_api;
            return SAI_STATUS_SUCCESS;

        case SAI_API_POLICER:
            *(const sai_policer_api_t**)api_method_table = &vs_policer_api;
            return SAI_STATUS_SUCCESS;

        case SAI_API_QOS_MAPS:
            *(const sai_qos_map_api_t**)api_method_table = &vs_qos_map_api;
            return SAI_STATUS_SUCCESS;

        case SAI_API_QUEUE:
            *(const sai_queue_api_t**)api_method_table = &vs_queue_api;
            return SAI_STATUS_SUCCESS;

        case SAI_API_SCHEDULER:
            *(const sai_scheduler_api_t**)api_method_table = &vs_scheduler_api;
            return SAI_STATUS_SUCCESS;

        case SAI_API_SCHEDULER_GROUP:
            *(const sai_scheduler_group_api_t**)api_method_table = &vs_scheduler_group_api;
            return SAI_STATUS_SUCCESS;

        case SAI_API_MIRROR:
            *(const sai_mirror_api_t**)api_method_table = &vs_mirror_api;
            return SAI_STATUS_SUCCESS;

        case SAI_API_UDF:
            *(const sai_udf_api_t**)api_method_table = &vs_udf_api;
            return SAI_STATUS_SUCCESS;

        case SAI_API_SAMPLEPACKET:
            *(const sai_samplepacket_api_t**)api_method_table = &vs_samplepacket_api;
            return SAI_STATUS_SUCCESS;

        case SAI_API_STP:
            *(const sai_stp_api_t**)api_method_table = &vs_stp_api;
            return SAI_STATUS_NOT_IMPLEMENTED;

        case SAI_API_LAG:
            *(const sai_lag_api_t**)api_method_table = &vs_lag_api;
            return SAI_STATUS_SUCCESS;

        case SAI_API_TUNNEL:
            *(const sai_tunnel_api_t**)api_method_table = &vs_tunnel_api;
            return SAI_STATUS_SUCCESS;

        default:
            SWSS_LOG_ERROR("Invalid API type %d", sai_api_id);
            return SAI_STATUS_INVALID_PARAMETER;
    }
}

sai_status_t sai_api_uninitialize(void)
{
    std::lock_guard<std::recursive_mutex> lock(g_recursive_mutex);

    SWSS_LOG_ENTER();

    g_apiInitialized = false;

    SWSS_LOG_ERROR("not implemented");

    return SAI_STATUS_NOT_IMPLEMENTED;
}
