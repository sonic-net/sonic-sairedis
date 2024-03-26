#include "sai_proxy.h"

PROXY_GENERIC_QUAD(SCHEDULER,scheduler);

const sai_scheduler_api_t proxy_scheduler_api = {

    PROXY_GENERIC_QUAD_API(scheduler)
};
