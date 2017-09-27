#include "sai_vs.h"
#include "sai_vs_internal.h"

sai_status_t vs_create_switch(
        _Out_ sai_object_id_t *switch_id,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    MUTEX();

    SWSS_LOG_ENTER();

    return meta_sai_create_oid(
            SAI_OBJECT_TYPE_SWITCH,
            switch_id,
            SAI_NULL_OBJECT_ID, // no switch id since we create switch
            attr_count,
            attr_list,
            &vs_generic_create);
}

VS_REMOVE(SWITCH,switch);
VS_SET(SWITCH,switch);
VS_GET(SWITCH,switch);

const sai_switch_api_t vs_switch_api = {

    vs_create_switch,
    vs_remove_switch,
    vs_set_switch_attribute,
    vs_get_switch_attribute,
};
