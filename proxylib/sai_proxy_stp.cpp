#include "sai_proxy.h"

PROXY_BULK_CREATE(STP_PORT,stp_ports);
PROXY_BULK_REMOVE(STP_PORT,stp_ports);
PROXY_GENERIC_QUAD(STP,stp);
PROXY_GENERIC_QUAD(STP_PORT,stp_port);

const sai_stp_api_t proxy_stp_api = {

    PROXY_GENERIC_QUAD_API(stp)
    PROXY_GENERIC_QUAD_API(stp_port)

    proxy_bulk_create_stp_ports,
    proxy_bulk_remove_stp_ports,
};
