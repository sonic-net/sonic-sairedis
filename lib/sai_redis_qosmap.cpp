#include "sai_redis.h"

REDIS_GENERIC_QUAD(QOS_MAP,qos_map);

const sai_qos_map_api_t redis_qos_map_api = {

    REDIS_GENERIC_QUAD_API(qos_map)
};
