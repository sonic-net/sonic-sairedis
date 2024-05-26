#include "sai_stub.h"

STUB_GENERIC_QUAD(DTEL,dtel);
STUB_GENERIC_QUAD(DTEL_QUEUE_REPORT,dtel_queue_report);
STUB_GENERIC_QUAD(DTEL_INT_SESSION,dtel_int_session);
STUB_GENERIC_QUAD(DTEL_REPORT_SESSION,dtel_report_session);
STUB_GENERIC_QUAD(DTEL_EVENT,dtel_event);

const sai_dtel_api_t stub_dtel_api = {

    STUB_GENERIC_QUAD_API(dtel)
    STUB_GENERIC_QUAD_API(dtel_queue_report)
    STUB_GENERIC_QUAD_API(dtel_int_session)
    STUB_GENERIC_QUAD_API(dtel_report_session)
    STUB_GENERIC_QUAD_API(dtel_event)
};
