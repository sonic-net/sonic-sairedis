#include "sai_proxy.h"

PROXY_GENERIC_QUAD(QOS_MAP,qos_map);

const sai_qos_map_api_t proxy_qos_map_api = {

    PROXY_GENERIC_QUAD_API(qos_map)
};
