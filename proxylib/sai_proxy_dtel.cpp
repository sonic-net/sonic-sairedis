#include "sai_proxy.h"

PROXY_GENERIC_QUAD(DTEL,dtel);
PROXY_GENERIC_QUAD(DTEL_QUEUE_REPORT,dtel_queue_report);
PROXY_GENERIC_QUAD(DTEL_INT_SESSION,dtel_int_session);
PROXY_GENERIC_QUAD(DTEL_REPORT_SESSION,dtel_report_session);
PROXY_GENERIC_QUAD(DTEL_EVENT,dtel_event);

const sai_dtel_api_t proxy_dtel_api = {

    PROXY_GENERIC_QUAD_API(dtel)
    PROXY_GENERIC_QUAD_API(dtel_queue_report)
    PROXY_GENERIC_QUAD_API(dtel_int_session)
    PROXY_GENERIC_QUAD_API(dtel_report_session)
    PROXY_GENERIC_QUAD_API(dtel_event)
};
