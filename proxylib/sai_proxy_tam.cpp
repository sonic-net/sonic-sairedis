#include "sai_proxy.h"

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

PROXY_GENERIC_QUAD(TAM,tam);
PROXY_GENERIC_QUAD(TAM_MATH_FUNC,tam_math_func);
PROXY_GENERIC_QUAD(TAM_REPORT,tam_report);
PROXY_GENERIC_QUAD(TAM_EVENT_THRESHOLD,tam_event_threshold);
PROXY_GENERIC_QUAD(TAM_INT,tam_int);
PROXY_GENERIC_QUAD(TAM_TEL_TYPE,tam_tel_type);
PROXY_GENERIC_QUAD(TAM_TRANSPORT,tam_transport);
PROXY_GENERIC_QUAD(TAM_TELEMETRY,tam_telemetry);
PROXY_GENERIC_QUAD(TAM_COLLECTOR,tam_collector);
PROXY_GENERIC_QUAD(TAM_EVENT_ACTION,tam_event_action);
PROXY_GENERIC_QUAD(TAM_EVENT,tam_event);
PROXY_GENERIC_QUAD(TAM_COUNTER_SUBSCRIPTION,tam_counter_subscription);

const sai_tam_api_t proxy_tam_api = {

    PROXY_GENERIC_QUAD_API(tam)
    PROXY_GENERIC_QUAD_API(tam_math_func)
    PROXY_GENERIC_QUAD_API(tam_report)
    PROXY_GENERIC_QUAD_API(tam_event_threshold)
    PROXY_GENERIC_QUAD_API(tam_int)
    PROXY_GENERIC_QUAD_API(tam_tel_type)
    PROXY_GENERIC_QUAD_API(tam_transport)
    PROXY_GENERIC_QUAD_API(tam_telemetry)
    PROXY_GENERIC_QUAD_API(tam_collector)
    PROXY_GENERIC_QUAD_API(tam_event_action)
    PROXY_GENERIC_QUAD_API(tam_event)
    PROXY_GENERIC_QUAD_API(tam_counter_subscription)
};
