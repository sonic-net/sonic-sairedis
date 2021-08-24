#include "sai_vs.h"

static sai_status_t vs_clear_port_all_stats(
        _In_ sai_object_id_t port_id)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_NOT_IMPLEMENTED;
}

VS_GENERIC_QUAD(PORT,port);
VS_GENERIC_QUAD(PORT_POOL,port_pool);
VS_GENERIC_QUAD(PORT_CONNECTOR,port_connector);
VS_GENERIC_QUAD(PORT_SERDES,port_serdes);
VS_GENERIC_STATS(PORT,port);
VS_GENERIC_STATS(PORT_POOL,port_pool);

const sai_port_api_t vs_port_api = {

    VS_GENERIC_QUAD_API(port)
    VS_GENERIC_STATS_API(port)

    vs_clear_port_all_stats,

    VS_GENERIC_QUAD_API(port_pool)
    VS_GENERIC_STATS_API(port_pool)

    VS_GENERIC_QUAD_API(port_connector)

    VS_GENERIC_QUAD_API(port_serdes)
};
