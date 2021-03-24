
// TODO auto generate

typedef struct _sai_switch_api_t
{
    sai_status_t create_switch(
            _Out_ sai_object_id_t *switch_id,
            _In_ uint32_t attr_count,
            _In_ const sai_attribute_t *attr_list);

    sai_status_t remove_switch(
            _In_ sai_object_id_t switch_id);

    sai_status_t set_switch_attribute(
            _In_ sai_object_id_t switch_id,
            _In_ const sai_attribute_t *attr);

    sai_status_t get_switch_attribute(
            _In_ sai_object_id_t switch_id,
            _In_ uint32_t attr_count,
            _Inout_ sai_attribute_t *attr_list);

    sai_status_t get_switch_stats(
            _In_ sai_object_id_t switch_id,
            _In_ uint32_t number_of_counters,
            _In_ const sai_stat_id_t *counter_ids,
            _Out_ uint64_t *counters);

    sai_status_t get_switch_stats_ext(
            _In_ sai_object_id_t switch_id,
            _In_ uint32_t number_of_counters,
            _In_ const sai_stat_id_t *counter_ids,
            _In_ sai_stats_mode_t mode,
            _Out_ uint64_t *counters);

    sai_status_t clear_switch_stats(
            _In_ sai_object_id_t switch_id,
            _In_ uint32_t number_of_counters,
            _In_ const sai_stat_id_t *counter_ids);

    sai_status_t switch_mdio_read(
            _In_ sai_object_id_t switch_id,
            _In_ uint32_t device_addr,
            _In_ uint32_t start_reg_addr,
            _In_ uint32_t number_of_registers,
            _Out_ uint32_t *reg_val);

    sai_status_t switch_mdio_write(
            _In_ sai_object_id_t switch_id,
            _In_ uint32_t device_addr,
            _In_ uint32_t start_reg_addr,
            _In_ uint32_t number_of_registers,
            _In_ const uint32_t *reg_val);

} sai_switch_api_t;

