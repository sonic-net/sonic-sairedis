#include "sai_vs.h"
#include "sai_vs_internal.h"
#include "sai_vs_state.h"
#include <string.h>

bool                    g_api_initialized = false;
sai_vs_switch_type_t    g_vs_switch_type = SAI_VS_SWITCH_TYPE_NONE;
std::recursive_mutex    g_recursive_mutex;

/**
 * @brief Serviec method table.
 *
 * We could use this table to choose switch vendor.
 */
static service_method_table_t g_service_method_table;

void clear_local_state()
{
    SWSS_LOG_ENTER();

    SWSS_LOG_NOTICE("clearing local state");

    /*
     * Initialize metatada database.
     */

    meta_init_db();

    /*
     * Reset used switch ids.
     */

    vs_clear_switch_ids();

    /*
     * Reset real counter id values
     */

    vs_reset_id_counter();
}

sai_status_t sai_api_initialize(
        _In_ uint64_t flags,
        _In_ const service_method_table_t *service_method_table)
{
    MUTEX();

    SWSS_LOG_ENTER();

    if (g_api_initialized)
    {
        SWSS_LOG_ERROR("api already initialized");

        return SAI_STATUS_FAILURE;
    }

    if ((service_method_table == NULL) ||
            (service_method_table->profile_get_next_value == NULL) ||
            (service_method_table->profile_get_value == NULL))
    {
        SWSS_LOG_ERROR("invalid service_method_table handle passed to SAI API initialize");

        return SAI_STATUS_INVALID_PARAMETER;
    }

    memcpy(&g_service_method_table, service_method_table, sizeof(g_service_method_table));

    // TODO maybe this query should be done right before switch create
    const char *type = service_method_table->profile_get_value(0, SAI_KEY_VS_SWITCH_TYPE);

    if (type == NULL)
    {
        SWSS_LOG_ERROR("failed to obtain service method table value: %s", SAI_KEY_VS_SWITCH_TYPE);

        return SAI_STATUS_FAILURE;
    }

    std::string strType = type;

    if (strType == SAI_VALUE_VS_SWITCH_TYPE_BCM56850)
    {
        g_vs_switch_type = SAI_VS_SWITCH_TYPE_BCM56850;
    }
    else if (strType == SAI_VALUE_VS_SWITCH_TYPE_MLNX2700)
    {
        g_vs_switch_type = SAI_VS_SWITCH_TYPE_MLNX2700;
    }
    else
    {
        SWSS_LOG_ERROR("unknown switch type: '%s'", type);

        return SAI_STATUS_FAILURE;
    }

    if (flags != 0)
    {
        SWSS_LOG_ERROR("invalid flags passed to SAI API initialize");

        return SAI_STATUS_INVALID_PARAMETER;
    }

    clear_local_state();

    g_api_initialized = true;

    return SAI_STATUS_SUCCESS;
}

sai_status_t sai_api_uninitialize(void)
{
    MUTEX();

    SWSS_LOG_ENTER();

    if (!g_api_initialized)
    {
        SWSS_LOG_ERROR("api not initialized");

        return SAI_STATUS_FAILURE;
    }

    clear_local_state();

    g_api_initialized = false;

    return SAI_STATUS_SUCCESS;
}

sai_status_t sai_log_set(
        _In_ sai_api_t sai_api_id,
        _In_ sai_log_level_t log_level)
{
    MUTEX();

    SWSS_LOG_ENTER();

    return SAI_STATUS_NOT_IMPLEMENTED;
}

#define API_CASE(API,api)                                                       \
    case SAI_API_ ## API: {                                                     \
        *(const sai_ ## api ## _api_t**)api_method_table = &vs_ ## api ## _api; \
        return SAI_STATUS_SUCCESS; }

sai_status_t sai_api_query(
        _In_ sai_api_t sai_api_id,
        _Out_ void** api_method_table)
{
    MUTEX();

    SWSS_LOG_ENTER();

    if (api_method_table == NULL)
    {
        SWSS_LOG_ERROR("NULL method table passed to SAI API initialize");
        return SAI_STATUS_INVALID_PARAMETER;
    }

    if (!g_api_initialized)
    {
        SWSS_LOG_ERROR("SAI API not initialized before calling API query");
        return SAI_STATUS_UNINITIALIZED;
    }

    switch (sai_api_id)
    {
        API_CASE(ACL,acl);
        API_CASE(BRIDGE,bridge);
        API_CASE(BUFFER,buffer);
        API_CASE(FDB,fdb);
        API_CASE(HASH,hash);
        API_CASE(HOSTIF,hostif);
        //API_CASE(IPMC,ipmc);
        //API_CASE(IPMC_GROUP,ipmc_group);
        //API_CASE(L2MC,l2mc);
        //API_CASE(L2MC_GROUP,l2mc_group);
        API_CASE(LAG,lag);
        //API_CASE(MCAST_FDB,mcast_fdb);
        API_CASE(MIRROR,mirror);
        API_CASE(NEIGHBOR,neighbor);
        API_CASE(NEXT_HOP,next_hop);
        API_CASE(NEXT_HOP_GROUP,next_hop_group);
        API_CASE(POLICER,policer);
        API_CASE(PORT,port);
        API_CASE(QOS_MAP,qos_map);
        API_CASE(QUEUE,queue);
        API_CASE(ROUTE,route);
        API_CASE(ROUTER_INTERFACE,router_interface);
        //API_CASE(RPF_GROUP,rpf_group);
        API_CASE(SAMPLEPACKET,samplepacket);
        API_CASE(SCHEDULER,scheduler);
        API_CASE(SCHEDULER_GROUP,scheduler_group);
        API_CASE(STP,stp);
        API_CASE(SWITCH,switch);
        API_CASE(TUNNEL,tunnel);
        API_CASE(UDF,udf);
        API_CASE(VIRTUAL_ROUTER,virtual_router);
        API_CASE(VLAN,vlan);
        API_CASE(WRED,wred);

        default:
            SWSS_LOG_ERROR("Invalid API type %d", sai_api_id);
            return SAI_STATUS_INVALID_PARAMETER;
    }
}
