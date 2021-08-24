#include "sai_vs.h"

VS_GENERIC_QUAD(DTEL,dtel);
VS_GENERIC_QUAD(DTEL_QUEUE_REPORT,dtel_queue_report);
VS_GENERIC_QUAD(DTEL_INT_SESSION,dtel_int_session);
VS_GENERIC_QUAD(DTEL_REPORT_SESSION,dtel_report_session);
VS_GENERIC_QUAD(DTEL_EVENT,dtel_event);

const sai_dtel_api_t vs_dtel_api = {

    VS_GENERIC_QUAD_API(dtel)
    VS_GENERIC_QUAD_API(dtel_queue_report)
    VS_GENERIC_QUAD_API(dtel_int_session)
    VS_GENERIC_QUAD_API(dtel_report_session)
    VS_GENERIC_QUAD_API(dtel_event)
};
