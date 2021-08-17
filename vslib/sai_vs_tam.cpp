#include "sai_vs.h"

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

VS_GENERIC_QUAD(TAM,tam);
VS_GENERIC_QUAD(TAM_MATH_FUNC,tam_math_func);
VS_GENERIC_QUAD(TAM_REPORT,tam_report);
VS_GENERIC_QUAD(TAM_EVENT_THRESHOLD,tam_event_threshold);
VS_GENERIC_QUAD(TAM_INT,tam_int);
VS_GENERIC_QUAD(TAM_TEL_TYPE,tam_tel_type);
VS_GENERIC_QUAD(TAM_TRANSPORT,tam_transport);
VS_GENERIC_QUAD(TAM_TELEMETRY,tam_telemetry);
VS_GENERIC_QUAD(TAM_COLLECTOR,tam_collector);
VS_GENERIC_QUAD(TAM_EVENT_ACTION,tam_event_action);
VS_GENERIC_QUAD(TAM_EVENT,tam_event);

const sai_tam_api_t vs_tam_api = {

    VS_GENERIC_QUAD_API(tam)
    VS_GENERIC_QUAD_API(tam_math_func)
    VS_GENERIC_QUAD_API(tam_report)
    VS_GENERIC_QUAD_API(tam_event_threshold)
    VS_GENERIC_QUAD_API(tam_int)
    VS_GENERIC_QUAD_API(tam_tel_type)
    VS_GENERIC_QUAD_API(tam_transport)
    VS_GENERIC_QUAD_API(tam_telemetry)
    VS_GENERIC_QUAD_API(tam_collector)
    VS_GENERIC_QUAD_API(tam_event_action)
    VS_GENERIC_QUAD_API(tam_event)

};
