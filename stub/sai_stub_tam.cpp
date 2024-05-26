#include "sai_stub.h"

sai_status_t sai_tam_telemetry_get_data(
        _In_ sai_object_id_t switch_id,
        _In_ sai_object_list_t obj_list,
        _In_ bool clear_on_read,
        _Inout_ sai_size_t *buffer_size,
        _Out_ void *buffer)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_NOT_IMPLEMENTED;
}

STUB_GENERIC_QUAD(TAM,tam);
STUB_GENERIC_QUAD(TAM_MATH_FUNC,tam_math_func);
STUB_GENERIC_QUAD(TAM_REPORT,tam_report);
STUB_GENERIC_QUAD(TAM_EVENT_THRESHOLD,tam_event_threshold);
STUB_GENERIC_QUAD(TAM_INT,tam_int);
STUB_GENERIC_QUAD(TAM_TEL_TYPE,tam_tel_type);
STUB_GENERIC_QUAD(TAM_TRANSPORT,tam_transport);
STUB_GENERIC_QUAD(TAM_TELEMETRY,tam_telemetry);
STUB_GENERIC_QUAD(TAM_COLLECTOR,tam_collector);
STUB_GENERIC_QUAD(TAM_EVENT_ACTION,tam_event_action);
STUB_GENERIC_QUAD(TAM_EVENT,tam_event);
STUB_GENERIC_QUAD(TAM_COUNTER_SUBSCRIPTION,tam_counter_subscription);

const sai_tam_api_t stub_tam_api = {

    STUB_GENERIC_QUAD_API(tam)
    STUB_GENERIC_QUAD_API(tam_math_func)
    STUB_GENERIC_QUAD_API(tam_report)
    STUB_GENERIC_QUAD_API(tam_event_threshold)
    STUB_GENERIC_QUAD_API(tam_int)
    STUB_GENERIC_QUAD_API(tam_tel_type)
    STUB_GENERIC_QUAD_API(tam_transport)
    STUB_GENERIC_QUAD_API(tam_telemetry)
    STUB_GENERIC_QUAD_API(tam_collector)
    STUB_GENERIC_QUAD_API(tam_event_action)
    STUB_GENERIC_QUAD_API(tam_event)
    STUB_GENERIC_QUAD_API(tam_counter_subscription)
};
