#ifndef __SAI_META_H__
#define __SAI_META_H__

extern "C" {
#include "sai.h"
#include "saimetadata.h"
}

#define MAX_LIST_COUNT 0x1000

#define DEFAULT_VLAN_NUMBER 1
#define MINIMUM_VLAN_NUMBER 1
#define MAXIMUM_VLAN_NUMBER 4094

extern void dump_object_reference();
extern sai_status_t meta_init_db();

// GENERIC FUNCTION POINTERS

typedef sai_status_t (*sai_create_generic_fn)(
        _In_ sai_object_type_t object_type,
        _Out_ sai_object_id_t* object_id,
        _In_ sai_object_id_t switch_id,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list);

typedef sai_status_t (*sai_remove_generic_fn)(
        _In_ sai_object_type_t object_type,
        _In_ sai_object_id_t object_id);

typedef sai_status_t (*sai_set_generic_attribute_fn)(
        _In_ sai_object_type_t object_type,
        _In_ sai_object_id_t object_id,
        _In_ const sai_attribute_t *attr);

typedef sai_status_t (*sai_get_generic_attribute_fn)(
        _In_ sai_object_type_t object_type,
        _In_ sai_object_id_t object_id,
        _In_ uint32_t attr_count,
        _Inout_ sai_attribute_t *attr_list);

template <typename T>
using sai_get_generic_stats_fn = sai_status_t (*)(
        _In_ sai_object_type_t object_type,
        _In_ sai_object_id_t object_id,
        _In_ uint32_t count,
        _In_ const T* counter_id_list,
        _Out_ uint64_t *counter_list);

// META GENERIC

extern sai_status_t meta_sai_create_oid(
        _In_ sai_object_type_t object_type,
        _Out_ sai_object_id_t* object_id,
        _In_ sai_object_id_t switch_id,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list,
        _In_ sai_create_generic_fn create);

extern sai_status_t meta_sai_remove_oid(
        _In_ sai_object_type_t object_type,
        _In_ sai_object_id_t object_id,
        _In_ sai_remove_generic_fn remove);

extern sai_status_t meta_sai_set_oid(
        _In_ sai_object_type_t object_type,
        _In_ sai_object_id_t object_id,
        _In_ const sai_attribute_t *attr,
        _In_ sai_set_generic_attribute_fn set);

extern sai_status_t meta_sai_get_oid(
        _In_ sai_object_type_t object_type,
        _In_ sai_object_id_t object_id,
        _In_ uint32_t attr_count,
        _Inout_ sai_attribute_t *attr_list,
        _In_ sai_get_generic_attribute_fn get);

template<typename T>
extern sai_status_t meta_sai_get_stats_oid(
        _In_ sai_object_type_t object_type,
        _In_ sai_object_id_t object_id,
        _In_ uint32_t count,
        _In_ const T* counter_id_list,
        _Inout_ uint64_t *counter_list,
        _In_ sai_get_generic_stats_fn<T> get);

// META FDB

extern sai_status_t meta_sai_create_fdb_entry(
        _In_ const sai_fdb_entry_t* fdb_entry,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list,
        _In_ sai_create_fdb_entry_fn create);

extern sai_status_t meta_sai_remove_fdb_entry(
        _In_ const sai_fdb_entry_t* fdb_entry,
        _In_ sai_remove_fdb_entry_fn remove);

extern sai_status_t meta_sai_set_fdb_entry(
        _In_ const sai_fdb_entry_t* fdb_entry,
        _In_ const sai_attribute_t *attr,
        _In_ sai_set_fdb_entry_attribute_fn set);

extern sai_status_t meta_sai_get_fdb_entry(
        _In_ const sai_fdb_entry_t* fdb_entry,
        _In_ uint32_t attr_count,
        _Inout_ sai_attribute_t *attr_list,
        _In_ sai_get_fdb_entry_attribute_fn get);

// META NEIGHBOR

extern sai_status_t meta_sai_create_neighbor_entry(
        _In_ const sai_neighbor_entry_t* neighbor_entry,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list,
        _In_ sai_create_neighbor_entry_fn create);

extern sai_status_t meta_sai_remove_neighbor_entry(
        _In_ const sai_neighbor_entry_t* neighbor_entry,
        _In_ sai_remove_neighbor_entry_fn remove);

extern sai_status_t meta_sai_set_neighbor_entry(
        _In_ const sai_neighbor_entry_t* neighbor_entry,
        _In_ const sai_attribute_t *attr,
        _In_ sai_set_neighbor_entry_attribute_fn set);

extern sai_status_t meta_sai_get_neighbor_entry(
        _In_ const sai_neighbor_entry_t* neighbor_entry,
        _In_ uint32_t attr_count,
        _Inout_ sai_attribute_t *attr_list,
        _In_ sai_get_neighbor_entry_attribute_fn get);

// META ROUTE

extern sai_status_t meta_sai_create_route_entry(
        _In_ const sai_route_entry_t* route_entry,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list,
        _In_ sai_create_route_entry_fn create);

extern sai_status_t meta_sai_remove_route_entry(
        _In_ const sai_route_entry_t* route_entry,
        _In_ sai_remove_route_entry_fn remove);

extern sai_status_t meta_sai_set_route_entry(
        _In_ const sai_route_entry_t* route_entry,
        _In_ const sai_attribute_t *attr,
        _In_ sai_set_route_entry_attribute_fn set);

extern sai_status_t meta_sai_get_route_entry(
        _In_ const sai_route_entry_t* route_entry,
        _In_ uint32_t attr_count,
        _Inout_ sai_attribute_t *attr_list,
        _In_ sai_get_route_entry_attribute_fn get);

// NOTIFICATIONS

extern void meta_sai_on_fdb_event(
        _In_ uint32_t count,
        _In_ sai_fdb_event_notification_data_t *data);

// FDB FLUSH

extern sai_status_t meta_sai_flush_fdb_entries(
        _In_ sai_object_id_t switch_id,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list,
        _In_ sai_flush_fdb_entries_fn flush_fdb_entries);

// UNIT TESTS HELPERS

/**
 * @brief Enable unittest globally.
 *
 * @param[in] enable If set to true unittests are enabled.
 */
void meta_unittests_enable(
        _In_ bool enable);

/**
 * @brief Indicates whethre unittests are enabled;
 */
bool meta_unittests_enabled();

/**
 * @bried Allow to perform SET operation on READ_ONLY attribue only once.
 *
 * This function relaxes metadata checking on SET operation, it allows to
 * perform SET api on READ_ONLY attribute only once on specific object type and
 * specific attribue.
 *
 * Once means that SET operation is only relaxed for the very next SET call on
 * that specific object type and attrirbute id.
 *
 * Function is explicitly named ONCE, since it will force test developer to not
 * forget that SET check is relaxed, and not forget for future unittests.
 *
 * Function is provided for more flexible testing using virtual switch.  Since
 * some of the read only attributes maybe very complex to simulate (for example
 * resources used by actual asic when adding next hop or next hop group), then
 * it's easier to write such unittest:
 *
 * TestCase:
 * 1. meta_unittests_allow_readonly_set_once(x,y);
 * 2. object_x_api->set_attribyte(object_id, attr, foo); // attr.id == y
 * 3. object_x_api->get_attribute(object_id, 1, attr); // attr.id == y
 * 4. check if get result is equal to set result.
 *
 * On real ASIC, even after allowing SET on read only attribute, actual SET
 * should fail.
 *
 * It can be dangerous to set any readonly attribute to different values since
 * internal metadata logic maybe using that value and in some cases metadata
 * database may get out of sync and cause unexpected results in api calls up to
 * application carash.
 *
 * This function is not thread safe.
 *
 * @param[in] object_type Object type on which SET will be possible.
 * @param[in] attr_id Attribute ID on which SET will be possible.
 *
 * @return #SAI_STATUS_SUCCESS on success Failure status code on error
 */
sai_status_t meta_unittests_allow_readonly_set_once(
        _In_ sai_object_type_t object_type,
        _In_ int32_t attr_id);

#endif // __SAI_META_H__
