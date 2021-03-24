
// TODO auto generate

typedef struct _sai_router_interface_api_t
{
    sai_status_t create_router_interface(
            _Out_ sai_object_id_t *router_interface_id,
            _In_ sai_object_id_t switch_id,
            _In_ uint32_t attr_count,
            _In_ const sai_attribute_t *attr_list);

    sai_status_t remove_router_interface(
            _In_ sai_object_id_t router_interface_id);

    sai_status_t set_router_interface_attribute(
            _In_ sai_object_id_t router_interface_id,
            _In_ const sai_attribute_t *attr);

    sai_status_t get_router_interface_attribute(
            _In_ sai_object_id_t router_interface_id,
            _In_ uint32_t attr_count,
            _Inout_ sai_attribute_t *attr_list);

    sai_status_t get_router_interface_stats(
            _In_ sai_object_id_t router_interface_id,
            _In_ uint32_t number_of_counters,
            _In_ const sai_stat_id_t *counter_ids,
            _Out_ uint64_t *counters);

    sai_status_t get_router_interface_stats_ext(
            _In_ sai_object_id_t router_interface_id,
            _In_ uint32_t number_of_counters,
            _In_ const sai_stat_id_t *counter_ids,
            _In_ sai_stats_mode_t mode,
            _Out_ uint64_t *counters);

    sai_status_t clear_router_interface_stats(
            _In_ sai_object_id_t router_interface_id,
            _In_ uint32_t number_of_counters,
            _In_ const sai_stat_id_t *counter_ids);

} sai_router_interface_api_t;
