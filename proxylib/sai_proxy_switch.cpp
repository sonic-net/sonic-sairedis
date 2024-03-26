#include "sai_proxy.h"

static sai_status_t proxy_switch_mdio_read(
        _In_ sai_object_id_t switch_id,
        _In_ uint32_t device_addr,
        _In_ uint32_t start_reg_addr,
        _In_ uint32_t number_of_registers,
        _Out_ uint32_t *reg_val)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_NOT_IMPLEMENTED;
}

static sai_status_t proxy_switch_mdio_write(
        _In_ sai_object_id_t switch_id,
        _In_ uint32_t device_addr,
        _In_ uint32_t start_reg_addr,
        _In_ uint32_t number_of_registers,
        _In_ const uint32_t *reg_val)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_NOT_IMPLEMENTED;
}

static sai_status_t proxy_switch_mdio_cl22_read(
        _In_ sai_object_id_t switch_id,
        _In_ uint32_t device_addr,
        _In_ uint32_t start_reg_addr,
        _In_ uint32_t number_of_registers,
        _Out_ uint32_t *reg_val)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_NOT_IMPLEMENTED;
}

static sai_status_t proxy_switch_mdio_cl22_write(
        _In_ sai_object_id_t switch_id,
        _In_ uint32_t device_addr,
        _In_ uint32_t start_reg_addr,
        _In_ uint32_t number_of_registers,
        _In_ const uint32_t *reg_val)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_NOT_IMPLEMENTED;
}

PROXY_GENERIC_QUAD(SWITCH,switch);
PROXY_GENERIC_STATS(SWITCH,switch);
PROXY_GENERIC_QUAD(SWITCH_TUNNEL,switch_tunnel);

static sai_status_t proxy_create_switch_uniq(
        _Out_ sai_object_id_t *switch_id,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    return proxy_create_switch(
            switch_id,
            SAI_NULL_OBJECT_ID, // no switch id since we create switch
            attr_count,
            attr_list);
}

const sai_switch_api_t proxy_switch_api = {

    proxy_create_switch_uniq,
    proxy_remove_switch,
    proxy_set_switch_attribute,
    proxy_get_switch_attribute,

    PROXY_GENERIC_STATS_API(switch)

    proxy_switch_mdio_read,
    proxy_switch_mdio_write,

    PROXY_GENERIC_QUAD_API(switch_tunnel)
    proxy_switch_mdio_cl22_read,
    proxy_switch_mdio_cl22_write
};
