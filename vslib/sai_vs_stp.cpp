#include "sai_vs.h"

VS_BULK_CREATE(STP_PORT,stp_ports);
VS_BULK_REMOVE(STP_PORT,stp_ports);

VS_GENERIC_QUAD(STP,stp);
VS_GENERIC_QUAD(STP_PORT,stp_port);

const sai_stp_api_t vs_stp_api = {

    VS_GENERIC_QUAD_API(stp)
    VS_GENERIC_QUAD_API(stp_port)

    vs_bulk_create_stp_ports,
    vs_bulk_remove_stp_ports,
};
