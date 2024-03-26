#include "sai_proxy.h"

PROXY_GENERIC_QUAD(QUEUE,queue);
PROXY_GENERIC_STATS(QUEUE,queue);

const sai_queue_api_t proxy_queue_api = {

    PROXY_GENERIC_QUAD_API(queue)
    PROXY_GENERIC_STATS_API(queue)
};
