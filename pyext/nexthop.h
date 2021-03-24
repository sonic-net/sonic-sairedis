
// TODO auto generate

typedef struct _sai_next_hop_api_t
{
    sai_status_t create_next_hop(
            _Out_ sai_object_id_t *next_hop_id,
            _In_ sai_object_id_t switch_id,
            _In_ uint32_t attr_count,
            _In_ const sai_attribute_t *attr_list);

    sai_status_t remove_next_hop(
            _In_ sai_object_id_t next_hop_id);

    sai_status_t set_next_hop_attribute(
            _In_ sai_object_id_t next_hop_id,
            _In_ const sai_attribute_t *attr);

    sai_status_t get_next_hop_attribute(
            _In_ sai_object_id_t next_hop_id,
            _In_ uint32_t attr_count,
            _Inout_ sai_attribute_t *attr_list);

} sai_next_hop_api_t;

