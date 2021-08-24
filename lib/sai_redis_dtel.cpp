#include "sai_redis.h"

REDIS_GENERIC_QUAD(DTEL,dtel);
REDIS_GENERIC_QUAD(DTEL_QUEUE_REPORT,dtel_queue_report);
REDIS_GENERIC_QUAD(DTEL_INT_SESSION,dtel_int_session);
REDIS_GENERIC_QUAD(DTEL_REPORT_SESSION,dtel_report_session);
REDIS_GENERIC_QUAD(DTEL_EVENT,dtel_event);

const sai_dtel_api_t redis_dtel_api = {

    REDIS_GENERIC_QUAD_API(dtel)
    REDIS_GENERIC_QUAD_API(dtel_queue_report)
    REDIS_GENERIC_QUAD_API(dtel_int_session)
    REDIS_GENERIC_QUAD_API(dtel_report_session)
    REDIS_GENERIC_QUAD_API(dtel_event)
};
