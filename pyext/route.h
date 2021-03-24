
// TODO auto generate

typedef struct _sai_route_api_t
{
    sai_status_t create_route_entry(
            _In_ const sai_route_entry_t *route_entry,
            _In_ uint32_t attr_count,
            _In_ const sai_attribute_t *attr_list);

    sai_status_t remove_route_entry(
            _In_ const sai_route_entry_t *route_entry);

    sai_status_t set_route_entry_attribute(
            _In_ const sai_route_entry_t *route_entry,
            _In_ const sai_attribute_t *attr);

    sai_status_t get_route_entry_attribute(
            _In_ const sai_route_entry_t *route_entry,
            _In_ uint32_t attr_count,
            _Inout_ sai_attribute_t *attr_list);

    sai_status_t create_route_entries(
            _In_ uint32_t object_count,
            _In_ const sai_route_entry_t *route_entry,
            _In_ const uint32_t *attr_count,
            _In_ const sai_attribute_t **attr_list,
            _In_ sai_bulk_op_error_mode_t mode,
            _Out_ sai_status_t *object_statuses);

    sai_status_t remove_route_entries(
            _In_ uint32_t object_count,
            _In_ const sai_route_entry_t *route_entry,
            _In_ sai_bulk_op_error_mode_t mode,
            _Out_ sai_status_t *object_statuses);

    sai_status_t set_route_entries_attribute(
            _In_ uint32_t object_count,
            _In_ const sai_route_entry_t *route_entry,
            _In_ const sai_attribute_t *attr_list,
            _In_ sai_bulk_op_error_mode_t mode,
            _Out_ sai_status_t *object_statuses);

    sai_status_t get_route_entries_attribute(
            _In_ uint32_t object_count,
            _In_ const sai_route_entry_t *route_entry,
            _In_ const uint32_t *attr_count,
            _Inout_ sai_attribute_t **attr_list,
            _In_ sai_bulk_op_error_mode_t mode,
            _Out_ sai_status_t *object_statuses);

} sai_route_api_t;
