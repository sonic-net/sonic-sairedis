#include "sai_vs.h"

VS_GENERIC_QUAD(SCHEDULER,scheduler);

const sai_scheduler_api_t vs_scheduler_api = {

    VS_GENERIC_QUAD_API(scheduler)
};
