#include "sai_redis.h"

sai_status_t redis_switch_mdio_read(
        _In_ sai_object_id_t switch_id,
        _In_ uint32_t device_addr,
        _In_ uint32_t start_reg_addr,
        _In_ uint32_t number_of_registers,
        _Out_ uint32_t *reg_val)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_NOT_IMPLEMENTED;
}

sai_status_t redis_switch_mdio_write(
        _In_ sai_object_id_t switch_id,
        _In_ uint32_t device_addr,
        _In_ uint32_t start_reg_addr,
        _In_ uint32_t number_of_registers,
        _In_ const uint32_t *reg_val)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_NOT_IMPLEMENTED;
}

REDIS_GENERIC_QUAD(SWITCH,switch);
REDIS_GENERIC_STATS(SWITCH,switch);

static sai_status_t redis_create_switch_uniq(
        _Out_ sai_object_id_t *switch_id,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    return redis_create_switch(
            switch_id,
            SAI_NULL_OBJECT_ID, // no switch id since we create switch
            attr_count,
            attr_list);
}

static sai_status_t redis_mdio_read(
        _In_ sai_object_id_t switch_id,
        _In_ uint32_t device_addr,
        _In_ uint32_t start_reg_addr,
        _In_ uint32_t number_of_registers,
        _Out_ uint32_t *reg_val) 
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_NOT_IMPLEMENTED;
}

static sai_status_t redis_mdio_write(
        _In_ sai_object_id_t switch_id,
        _In_ uint32_t device_addr,
        _In_ uint32_t start_reg_addr,
        _In_ uint32_t number_of_registers,
        _In_ const uint32_t *reg_val)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_NOT_IMPLEMENTED;
}

const sai_switch_api_t redis_switch_api = {

    redis_create_switch_uniq,
    redis_remove_switch,
    redis_set_switch_attribute,
    redis_get_switch_attribute,

    REDIS_GENERIC_STATS_API(switch)

    redis_mdio_read,
    redis_mdio_write,
};
