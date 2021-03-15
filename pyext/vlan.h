
// TODO auto generate

typedef struct _sai_vlan_api_t
{
    sai_status_t create_vlan(
            _Out_ sai_object_id_t *vlan_id,
            _In_ sai_object_id_t switch_id,
            _In_ uint32_t attr_count,
            _In_ const sai_attribute_t *attr_list);

    sai_status_t remove_vlan(
            _In_ sai_object_id_t vlan_id);

    sai_status_t set_vlan_attribute(
            _In_ sai_object_id_t vlan_id,
            _In_ const sai_attribute_t *attr);

    sai_status_t get_vlan_attribute(
            _In_ sai_object_id_t vlan_id,
            _In_ uint32_t attr_count,
            _Inout_ sai_attribute_t *attr_list);

    sai_status_t create_vlan_member(
            _Out_ sai_object_id_t *vlan_member_id,
            _In_ sai_object_id_t switch_id,
            _In_ uint32_t attr_count,
            _In_ const sai_attribute_t *attr_list);

    sai_status_t remove_vlan_member(
            _In_ sai_object_id_t vlan_member_id);

    sai_status_t set_vlan_member_attribute(
            _In_ sai_object_id_t vlan_member_id,
            _In_ const sai_attribute_t *attr);

    sai_status_t get_vlan_member_attribute(
            _In_ sai_object_id_t vlan_member_id,
            _In_ uint32_t attr_count,
            _Inout_ sai_attribute_t *attr_list);

    sai_status_t create_vlan_members(
            _In_ sai_object_id_t switch_id,
            _In_ uint32_t object_count,
            _In_ const uint32_t *attr_count,
            _In_ const sai_attribute_t **attr_list,
            _In_ sai_bulk_op_error_mode_t mode,
            _Out_ sai_object_id_t *object_id,
            _Out_ sai_status_t *object_statuses);

    sai_status_t remove_vlan_members(
            _In_ uint32_t object_count,
            _In_ const sai_object_id_t *object_id,
            _In_ sai_bulk_op_error_mode_t mode,
            _Out_ sai_status_t *object_statuses);

    sai_status_t get_vlan_stats(
            _In_ sai_object_id_t vlan_id,
            _In_ uint32_t number_of_counters,
            _In_ const sai_stat_id_t *counter_ids,
            _Out_ uint64_t *counters);

    sai_status_t get_vlan_stats_ext(
            _In_ sai_object_id_t vlan_id,
            _In_ uint32_t number_of_counters,
            _In_ const sai_stat_id_t *counter_ids,
            _In_ sai_stats_mode_t mode,
            _Out_ uint64_t *counters);

    sai_status_t clear_vlan_stats(
            _In_ sai_object_id_t vlan_id,
            _In_ uint32_t number_of_counters,
            _In_ const sai_stat_id_t *counter_ids);

} sai_vlan_api_t;

