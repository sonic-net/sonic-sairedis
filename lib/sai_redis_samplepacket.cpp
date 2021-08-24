#include "sai_redis.h"

REDIS_GENERIC_QUAD(SAMPLEPACKET,samplepacket);

const sai_samplepacket_api_t redis_samplepacket_api = {

    REDIS_GENERIC_QUAD_API(samplepacket)
};
