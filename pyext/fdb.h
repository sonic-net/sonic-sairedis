
// TODO auto generate

typedef struct _sai_fdb_api_t
{
    sai_status_t create_fdb_entry(
            _In_ const sai_fdb_entry_t *fdb_entry,
            _In_ uint32_t attr_count,
            _In_ const sai_attribute_t *attr_list);

    sai_status_t remove_fdb_entry(
            _In_ const sai_fdb_entry_t *fdb_entry);

    sai_status_t set_fdb_entry_attribute(
            _In_ const sai_fdb_entry_t *fdb_entry,
            _In_ const sai_attribute_t *attr);

    sai_status_t get_fdb_entry_attribute(
            _In_ const sai_fdb_entry_t *fdb_entry,
            _In_ uint32_t attr_count,
            _Inout_ sai_attribute_t *attr_list);

    sai_status_t flush_fdb_entries(
            _In_ sai_object_id_t switch_id,
            _In_ uint32_t attr_count,
            _In_ const sai_attribute_t *attr_list);

    sai_status_t create_fdb_entries(
            _In_ uint32_t object_count,
            _In_ const sai_fdb_entry_t *fdb_entry,
            _In_ const uint32_t *attr_count,
            _In_ const sai_attribute_t **attr_list,
            _In_ sai_bulk_op_error_mode_t mode,
            _Out_ sai_status_t *object_statuses);

    sai_status_t remove_fdb_entries(
            _In_ uint32_t object_count,
            _In_ const sai_fdb_entry_t *fdb_entry,
            _In_ sai_bulk_op_error_mode_t mode,
            _Out_ sai_status_t *object_statuses);

    sai_status_t set_fdb_entries_attribute(
            _In_ uint32_t object_count,
            _In_ const sai_fdb_entry_t *fdb_entry,
            _In_ const sai_attribute_t *attr_list,
            _In_ sai_bulk_op_error_mode_t mode,
            _Out_ sai_status_t *object_statuses);

    sai_status_t get_fdb_entries_attribute(
            _In_ uint32_t object_count,
            _In_ const sai_fdb_entry_t *fdb_entry,
            _In_ const uint32_t *attr_count,
            _Inout_ sai_attribute_t **attr_list,
            _In_ sai_bulk_op_error_mode_t mode,
            _Out_ sai_status_t *object_statuses);

} sai_fdb_api_t;
