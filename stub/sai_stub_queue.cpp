#include "sai_stub.h"

STUB_GENERIC_QUAD(QUEUE,queue);
STUB_GENERIC_STATS(QUEUE,queue);

const sai_queue_api_t stub_queue_api = {

    STUB_GENERIC_QUAD_API(queue)
    STUB_GENERIC_STATS_API(queue)
};
