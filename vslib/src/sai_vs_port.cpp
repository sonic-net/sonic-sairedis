#include "sai_vs.h"
#include "sai_vs_internal.h"
#include "sai_vs_state.h"
#include "sai_vs_switch_BCM56850.h"

sai_status_t vs_clear_port_all_stats(
        _In_ sai_object_id_t port_id)
{
    MUTEX();

    SWSS_LOG_ENTER();

    return SAI_STATUS_NOT_IMPLEMENTED;
}

sai_status_t vs_create_port(
            _Out_ sai_object_id_t *port_id,
            _In_ sai_object_id_t switch_id,
            _In_ uint32_t attr_count,
            _In_ const sai_attribute_t *attr_list)
{
    MUTEX();
    SWSS_LOG_ENTER();

    /* create port */
    CHECK_STATUS(meta_sai_create_oid((sai_object_type_t)SAI_OBJECT_TYPE_PORT,
                port_id,switch_id,attr_count,attr_list,&vs_generic_create));

    if (g_vs_switch_type == SAI_VS_SWITCH_TYPE_BCM56850)
    {
        vs_create_port_BCM56850(*port_id, switch_id);
    }

    return SAI_STATUS_SUCCESS;
}

VS_REMOVE(PORT,port);
VS_SET(PORT,port);
VS_GET(PORT,port);
VS_GENERIC_QUAD(PORT_POOL,port_pool);
VS_GENERIC_STATS(PORT,port);
VS_GENERIC_STATS(PORT_POOL,port_pool);

const sai_port_api_t vs_port_api = {

    VS_GENERIC_QUAD_API(port)
    VS_GENERIC_STATS_API(port)

    vs_clear_port_all_stats,

    VS_GENERIC_QUAD_API(port_pool)
    VS_GENERIC_STATS_API(port_pool)
};
