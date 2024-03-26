#include "sai_proxy.h"

PROXY_GENERIC_QUAD(SCHEDULER_GROUP,scheduler_group);

const sai_scheduler_group_api_t proxy_scheduler_group_api = {

    PROXY_GENERIC_QUAD_API(scheduler_group)
};
