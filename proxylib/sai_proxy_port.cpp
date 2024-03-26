#include "sai_proxy.h"

static sai_status_t proxy_clear_port_all_stats(
        _In_ sai_object_id_t port_id)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_NOT_IMPLEMENTED;
}

PROXY_GENERIC_QUAD(PORT,port);
PROXY_GENERIC_QUAD(PORT_POOL,port_pool);
PROXY_GENERIC_QUAD(PORT_SERDES,port_serdes);
PROXY_GENERIC_QUAD(PORT_CONNECTOR,port_connector);
PROXY_GENERIC_STATS(PORT,port);
PROXY_GENERIC_STATS(PORT_POOL,port_pool);
PROXY_BULK_QUAD(PORT, ports);
PROXY_BULK_QUAD(PORT_SERDES, port_serdeses);

const sai_port_api_t proxy_port_api = {

    PROXY_GENERIC_QUAD_API(port)
    PROXY_GENERIC_STATS_API(port)

    proxy_clear_port_all_stats,

    PROXY_GENERIC_QUAD_API(port_pool)
    PROXY_GENERIC_STATS_API(port_pool)
    PROXY_GENERIC_QUAD_API(port_connector)
    PROXY_GENERIC_QUAD_API(port_serdes)
    PROXY_BULK_QUAD_API(ports)
    PROXY_BULK_QUAD_API(port_serdeses)
};
