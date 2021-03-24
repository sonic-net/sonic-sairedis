
// TODO auto generate

typedef struct _sai_lag_api_t
{
    sai_status_t create_lag(
            _Out_ sai_object_id_t *lag_id,
            _In_ sai_object_id_t switch_id,
            _In_ uint32_t attr_count,
            _In_ const sai_attribute_t *attr_list);

    sai_status_t remove_lag(
            _In_ sai_object_id_t lag_id);

    sai_status_t set_lag_attribute(
            _In_ sai_object_id_t lag_id,
            _In_ const sai_attribute_t *attr);

    sai_status_t get_lag_attribute(
            _In_ sai_object_id_t lag_id,
            _In_ uint32_t attr_count,
            _Inout_ sai_attribute_t *attr_list);

    sai_status_t create_lag_member(
            _Out_ sai_object_id_t *lag_member_id,
            _In_ sai_object_id_t switch_id,
            _In_ uint32_t attr_count,
            _In_ const sai_attribute_t *attr_list);

    sai_status_t remove_lag_member(
            _In_ sai_object_id_t lag_member_id);

    sai_status_t set_lag_member_attribute(
            _In_ sai_object_id_t lag_member_id,
            _In_ const sai_attribute_t *attr);

    sai_status_t get_lag_member_attribute(
            _In_ sai_object_id_t lag_member_id,
            _In_ uint32_t attr_count,
            _Inout_ sai_attribute_t *attr_list);

    sai_status_t create_lag_members(
            _In_ sai_object_id_t switch_id,
            _In_ uint32_t object_count,
            _In_ const uint32_t *attr_count,
            _In_ const sai_attribute_t **attr_list,
            _In_ sai_bulk_op_error_mode_t mode,
            _Out_ sai_object_id_t *object_id,
            _Out_ sai_status_t *object_statuses);

    sai_status_t remove_lag_members(
            _In_ uint32_t object_count,
            _In_ const sai_object_id_t *object_id,
            _In_ sai_bulk_op_error_mode_t mode,
            _Out_ sai_status_t *object_statuses);

} sai_lag_api_t;
