#include "sai_vs.h"

VS_GENERIC_QUAD(SCHEDULER_GROUP,scheduler_group);

const sai_scheduler_group_api_t vs_scheduler_group_api = {

    VS_GENERIC_QUAD_API(scheduler_group)
};
