#include "sai_redis.h"
#include "sai_redis_internal.h"

REDIS_GENERIC_QUAD(TAM,tam);
REDIS_GENERIC_QUAD(TAM_MATH_FUNC,tam_math_func);
REDIS_GENERIC_QUAD(TAM_REPORT,tam_report);
REDIS_GENERIC_QUAD(TAM_EVENT_THRESHOLD,tam_event_threshold);
REDIS_GENERIC_QUAD(TAM_INT,tam_int);
REDIS_GENERIC_QUAD(TAM_TEL_TYPE,tam_tel_type);
REDIS_GENERIC_QUAD(TAM_TRANSPORT,tam_transport);
REDIS_GENERIC_QUAD(TAM_TELEMETRY,tam_telemetry);
REDIS_GENERIC_QUAD(TAM_COLLECTOR,tam_collector);
REDIS_GENERIC_QUAD(TAM_EVENT_ACTION,tam_event_action);
REDIS_GENERIC_QUAD(TAM_EVENT,tam_event);

const sai_tam_api_t redis_tam_api = {

    REDIS_GENERIC_QUAD_API(tam)
    REDIS_GENERIC_QUAD_API(tam_math_func)
    REDIS_GENERIC_QUAD_API(tam_report)
    REDIS_GENERIC_QUAD_API(tam_event_threshold)
    REDIS_GENERIC_QUAD_API(tam_int)
    REDIS_GENERIC_QUAD_API(tam_tel_type)
    REDIS_GENERIC_QUAD_API(tam_transport)
    REDIS_GENERIC_QUAD_API(tam_telemetry)
    REDIS_GENERIC_QUAD_API(tam_collector)
    REDIS_GENERIC_QUAD_API(tam_event_action)
    REDIS_GENERIC_QUAD_API(tam_event)
};
