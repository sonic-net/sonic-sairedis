#include "sai_stub.h"

STUB_BULK_CREATE(STP_PORT,stp_ports);
STUB_BULK_REMOVE(STP_PORT,stp_ports);
STUB_GENERIC_QUAD(STP,stp);
STUB_GENERIC_QUAD(STP_PORT,stp_port);

const sai_stp_api_t stub_stp_api = {

    STUB_GENERIC_QUAD_API(stp)
    STUB_GENERIC_QUAD_API(stp_port)

    stub_bulk_create_stp_ports,
    stub_bulk_remove_stp_ports,
};
