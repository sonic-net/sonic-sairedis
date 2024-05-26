#include "sai_stub.h"

static sai_status_t stub_clear_port_all_stats(
        _In_ sai_object_id_t port_id)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_NOT_IMPLEMENTED;
}

STUB_GENERIC_QUAD(PORT,port);
STUB_GENERIC_QUAD(PORT_POOL,port_pool);
STUB_GENERIC_QUAD(PORT_SERDES,port_serdes);
STUB_GENERIC_QUAD(PORT_CONNECTOR,port_connector);
STUB_GENERIC_STATS(PORT,port);
STUB_GENERIC_STATS(PORT_POOL,port_pool);
STUB_BULK_QUAD(PORT, ports);
STUB_BULK_QUAD(PORT_SERDES, port_serdeses);

const sai_port_api_t stub_port_api = {

    STUB_GENERIC_QUAD_API(port)
    STUB_GENERIC_STATS_API(port)

    stub_clear_port_all_stats,

    STUB_GENERIC_QUAD_API(port_pool)
    STUB_GENERIC_STATS_API(port_pool)
    STUB_GENERIC_QUAD_API(port_connector)
    STUB_GENERIC_QUAD_API(port_serdes)
    STUB_BULK_QUAD_API(ports)
    STUB_BULK_QUAD_API(port_serdeses)
};
