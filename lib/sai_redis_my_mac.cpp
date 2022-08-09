#include "sai_redis.h"

REDIS_GENERIC_QUAD(MY_MAC,my_mac);

const sai_my_mac_api_t redis_my_mac_api = {

    REDIS_GENERIC_QUAD_API(my_mac)
};
