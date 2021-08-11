#include "sai_redis.h"

REDIS_BULK_CREATE(STP_PORT,stp_ports);
REDIS_BULK_REMOVE(STP_PORT,stp_ports);
REDIS_GENERIC_QUAD(STP,stp);
REDIS_GENERIC_QUAD(STP_PORT,stp_port);

const sai_stp_api_t redis_stp_api = {

    REDIS_GENERIC_QUAD_API(stp)
    REDIS_GENERIC_QUAD_API(stp_port)

    redis_bulk_create_stp_ports,
    redis_bulk_remove_stp_ports,
};
