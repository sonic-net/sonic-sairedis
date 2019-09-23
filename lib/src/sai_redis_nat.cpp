#include "sai_redis.h"

sai_status_t sai_bulk_create_nat_entry(
        _In_ uint32_t object_count,
        _In_ const sai_nat_entry_t *nat_entry,
        _In_ const uint32_t *attr_count,
        _In_ const sai_attribute_t **attr_list,
        _In_ sai_bulk_op_error_mode_t mode,
        _Out_ sai_status_t *object_statuses)
{
   MUTEX();
   SWSS_LOG_ENTER();

   return SAI_STATUS_NOT_IMPLEMENTED;
}

sai_status_t sai_bulk_remove_nat_entry(
        _In_ uint32_t object_count,
        _In_ const sai_nat_entry_t *nat_entry,
        _In_ sai_bulk_op_error_mode_t mode,
        _Out_ sai_status_t *object_statuses)
{
   MUTEX();
   SWSS_LOG_ENTER();

   return SAI_STATUS_NOT_IMPLEMENTED;

}

sai_status_t sai_bulk_set_nat_entry_attribute(
        _In_ uint32_t object_count,
        _In_ const sai_nat_entry_t *nat_entry,
        _In_ const sai_attribute_t *attr_list,
        _In_ sai_bulk_op_error_mode_t mode,
        _Out_ sai_status_t *object_statuses)
{
   MUTEX();
   SWSS_LOG_ENTER();

   return SAI_STATUS_NOT_IMPLEMENTED;

}

sai_status_t sai_bulk_get_nat_entry_attribute(
        _In_ uint32_t object_count,
        _In_ const sai_nat_entry_t *nat_entry,
        _In_ const uint32_t *attr_count,
        _Inout_ sai_attribute_t **attr_list,
        _In_ sai_bulk_op_error_mode_t mode,
        _Out_ sai_status_t *object_statuses)
{
   MUTEX();
   SWSS_LOG_ENTER();

   return SAI_STATUS_NOT_IMPLEMENTED;

}

REDIS_GENERIC_QUAD_ENTRY(NAT_ENTRY,nat_entry);
REDIS_GENERIC_QUAD(NAT_ZONE_COUNTER,nat_zone_counter);

const sai_nat_api_t redis_nat_api = {

   REDIS_GENERIC_QUAD_API(nat_entry)

   sai_bulk_create_nat_entry,
   sai_bulk_remove_nat_entry,
   sai_bulk_set_nat_entry_attribute,
   sai_bulk_get_nat_entry_attribute,

   REDIS_GENERIC_QUAD_API(nat_zone_counter)
};
