#include "config.h"
#include "VendorSai.h"

#include "meta/sai_serialize.h"

#include "swss/logger.h"

#include <cinttypes>
#include <cstring>

using namespace syncd;

#define MUTEX() std::lock_guard<std::mutex> _lock(m_apimutex)

#define VENDOR_CHECK_API_INITIALIZED()                                       \
    if (!m_apiInitialized) {                                                \
        SWSS_LOG_ERROR("%s: api not initialized", __PRETTY_FUNCTION__);     \
        return SAI_STATUS_FAILURE; }

VendorSai::VendorSai()
{
    SWSS_LOG_ENTER();

    m_apiInitialized = false;

    memset(&m_apis, 0, sizeof(m_apis));

    sai_global_apis_t ga =
    {
        .api_initialize = &sai_api_initialize,
        .api_query = &sai_api_query,
        .api_uninitialize = &sai_api_uninitialize,
        .bulk_get_attribute = nullptr,
#ifdef HAVE_SAI_BULK_OBJECT_CLEAR_STATS
        .bulk_object_clear_stats = &sai_bulk_object_clear_stats,
#else
        .bulk_object_clear_stats = nullptr,
#endif
#ifdef HAVE_SAI_BULK_OBJECT_GET_STATS
        .bulk_object_get_stats = &sai_bulk_object_get_stats,
#else
        .bulk_object_get_stats = nullptr,
#endif
        .dbg_generate_dump = nullptr,
        .get_maximum_attribute_count = nullptr,
        .get_object_count = nullptr,
        .get_object_key = nullptr,
        .log_set = &sai_log_set,
        .object_type_get_availability = &sai_object_type_get_availability,
        .object_type_query = &sai_object_type_query,
        .query_api_version = &sai_query_api_version,
        .query_attribute_capability = &sai_query_attribute_capability,
        .query_attribute_enum_values_capability = &sai_query_attribute_enum_values_capability,
        .query_object_stage = nullptr,
        .query_stats_capability = &sai_query_stats_capability,
        .query_stats_st_capability = &sai_query_stats_st_capability,
        .switch_id_query = &sai_switch_id_query,
        .tam_telemetry_get_data = nullptr,
    };

    m_globalApis = ga;
}

VendorSai::~VendorSai()
{
    SWSS_LOG_ENTER();

    if (m_apiInitialized)
    {
        apiUninitialize();
    }
}

// INITIALIZE UNINITIALIZE

sai_status_t VendorSai::apiInitialize(
        _In_ uint64_t flags,
        _In_ const sai_service_method_table_t *service_method_table)
{
    MUTEX();
    SWSS_LOG_ENTER();

    if (m_apiInitialized)
    {
        SWSS_LOG_ERROR("%s: api already initialized", __PRETTY_FUNCTION__);

        return SAI_STATUS_FAILURE;
    }

    if ((service_method_table == NULL))
    {
        SWSS_LOG_ERROR("invalid service_method_table handle passed to SAI API initialize");

        return SAI_STATUS_INVALID_PARAMETER;
    }

    memcpy(&m_service_method_table, service_method_table, sizeof(m_service_method_table));

    auto status = m_globalApis.api_initialize(flags, service_method_table);

    if (status == SAI_STATUS_SUCCESS)
    {
        memset(&m_apis, 0, sizeof(m_apis));

        int failed = sai_metadata_apis_query(m_globalApis.api_query, &m_apis);

        if (failed > 0)
        {
            SWSS_LOG_NOTICE("sai_api_query failed for %d apis", failed);
        }

    }

    sai_api_version_t version{};

    auto api_status = m_globalApis.query_api_version(&version);

    if (api_status != SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_ERROR("failed to query SAI API version: %s", sai_serialize_status(api_status).c_str());

        return api_status;
    }

    // please refer to https://github.com/sonic-net/sonic-sairedis/pull/1246 or commit 606703e1

    sai_api_version_t minversion = SAI_VERSION(1,9,0);

    SWSS_LOG_NOTICE("SAI API vendor version: %" PRId64, version);
    SWSS_LOG_NOTICE("SAI API min version: %" PRId64, minversion);
    SWSS_LOG_NOTICE("SAI API headers version: %d", SAI_API_VERSION);

    if ((version < minversion) || (SAI_API_VERSION < minversion))
    {
        SWSS_LOG_ERROR("SAI implementation API version %" PRId64 " or SAI headers API version %d does not meet minimum version requirements, min version required: %" PRId64,
                       version, SAI_API_VERSION, minversion);

        return SAI_STATUS_FAILURE;
    }

    m_apiInitialized = true;

    return status;
}

sai_status_t VendorSai::apiUninitialize(void)
{
    SWSS_LOG_ENTER();
    VENDOR_CHECK_API_INITIALIZED();

    auto status = m_globalApis.api_uninitialize();

    if (status == SAI_STATUS_SUCCESS)
    {
        m_apiInitialized = false;

        memset(&m_apis, 0, sizeof(m_apis));
    }

    return status;
}

// QUAD OID

sai_status_t VendorSai::create(
        _In_ sai_object_type_t objectType,
        _Out_ sai_object_id_t* objectId,
        _In_ sai_object_id_t switchId,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    MUTEX();
    SWSS_LOG_ENTER();
    VENDOR_CHECK_API_INITIALIZED();

    sai_object_meta_key_t mk = { .objecttype = objectType, .objectkey = { .key = { .object_id = 0 } } };

    auto status = sai_metadata_generic_create(&m_apis, &mk, switchId, attr_count, attr_list);

    if (status == SAI_STATUS_SUCCESS)
    {
        *objectId = mk.objectkey.key.object_id;
    }

    return status;
}

sai_status_t VendorSai::remove(
        _In_ sai_object_type_t objectType,
        _In_ sai_object_id_t objectId)
{
    MUTEX();
    SWSS_LOG_ENTER();
    VENDOR_CHECK_API_INITIALIZED();

    sai_object_meta_key_t mk = { .objecttype = objectType, .objectkey = { .key = { .object_id = objectId } } };

    return sai_metadata_generic_remove(&m_apis, &mk);
}

sai_status_t VendorSai::set(
        _In_ sai_object_type_t objectType,
        _In_ sai_object_id_t objectId,
        _In_ const sai_attribute_t *attr)
{
    std::unique_lock<std::mutex> _lock(m_apimutex);
    SWSS_LOG_ENTER();
    VENDOR_CHECK_API_INITIALIZED();

    sai_object_meta_key_t mk = { .objecttype = objectType, .objectkey = { .key = { .object_id = objectId } } };

    if (objectType == SAI_OBJECT_TYPE_SWITCH && attr && attr->id == SAI_SWITCH_ATTR_SWITCH_SHELL_ENABLE)
    {
        // in case of diagnostic shell, this vendor api can be blocking, so
        // release lock here to not cause deadlock for other events in syncd
        _lock.unlock();
    }

    return sai_metadata_generic_set(&m_apis, &mk, attr);
}

sai_status_t VendorSai::get(
        _In_ sai_object_type_t objectType,
        _In_ sai_object_id_t objectId,
        _In_ uint32_t attr_count,
        _Inout_ sai_attribute_t *attr_list)
{
    MUTEX();
    SWSS_LOG_ENTER();
    VENDOR_CHECK_API_INITIALIZED();

    sai_object_meta_key_t mk = { .objecttype = objectType, .objectkey = { .key = { .object_id = objectId } } };

    return sai_metadata_generic_get(&m_apis, &mk, attr_count, attr_list);
}

// QUAD ENTRY

#define DECLARE_CREATE_ENTRY(OT,ot)                                         \
sai_status_t VendorSai::create(                                             \
        _In_ const sai_ ## ot ## _t* entry,                                 \
        _In_ uint32_t attr_count,                                           \
        _In_ const sai_attribute_t *attr_list)                              \
{                                                                           \
    MUTEX();                                                                \
    SWSS_LOG_ENTER();                                                       \
    VENDOR_CHECK_API_INITIALIZED();                                         \
    auto info = sai_metadata_get_object_type_info(                          \
        (sai_object_type_t)SAI_OBJECT_TYPE_ ## OT);                         \
    sai_object_meta_key_t mk = { .objecttype = info->objecttype,            \
        .objectkey = { .key = { .ot = *entry } } };                         \
    return info->create(&mk, 0, attr_count, attr_list);                     \
}

SAIREDIS_DECLARE_EVERY_ENTRY(DECLARE_CREATE_ENTRY);

#define DECLARE_REMOVE_ENTRY(OT,ot)                                         \
sai_status_t VendorSai::remove(                                             \
        _In_ const sai_ ## ot ## _t* entry)                                 \
{                                                                           \
    MUTEX();                                                                \
    SWSS_LOG_ENTER();                                                       \
    VENDOR_CHECK_API_INITIALIZED();                                         \
    auto info = sai_metadata_get_object_type_info(                          \
        (sai_object_type_t)SAI_OBJECT_TYPE_ ## OT);                         \
    sai_object_meta_key_t mk = { .objecttype = info->objecttype,            \
        .objectkey = { .key = { .ot = *entry } } };                         \
    return info->remove(&mk);                                               \
}

SAIREDIS_DECLARE_EVERY_ENTRY(DECLARE_REMOVE_ENTRY);

#define DECLARE_SET_ENTRY(OT,ot)                                            \
sai_status_t VendorSai::set(                                                \
        _In_ const sai_ ## ot ## _t* entry,                                 \
        _In_ const sai_attribute_t *attr)                                   \
{                                                                           \
    MUTEX();                                                                \
    SWSS_LOG_ENTER();                                                       \
    VENDOR_CHECK_API_INITIALIZED();                                         \
    auto info = sai_metadata_get_object_type_info(                          \
        (sai_object_type_t) SAI_OBJECT_TYPE_ ## OT);                        \
    sai_object_meta_key_t mk = { .objecttype = info->objecttype,            \
        .objectkey = { .key = { .ot = *entry } } };                         \
    return info->set(&mk, attr);                                            \
}

SAIREDIS_DECLARE_EVERY_ENTRY(DECLARE_SET_ENTRY);

#define DECLARE_GET_ENTRY(OT,ot)                                            \
sai_status_t VendorSai::get(                                                \
        _In_ const sai_ ## ot ## _t* entry,                                 \
        _In_ uint32_t attr_count,                                           \
        _Inout_ sai_attribute_t *attr_list)                                 \
{                                                                           \
    MUTEX();                                                                \
    SWSS_LOG_ENTER();                                                       \
    VENDOR_CHECK_API_INITIALIZED();                                         \
    auto info = sai_metadata_get_object_type_info(                          \
        (sai_object_type_t) SAI_OBJECT_TYPE_ ## OT);                        \
    sai_object_meta_key_t mk = { .objecttype = info->objecttype,            \
        .objectkey = { .key = { .ot = *entry } } };                         \
    return info->get(&mk, attr_count, attr_list);                           \
}

SAIREDIS_DECLARE_EVERY_ENTRY(DECLARE_GET_ENTRY);

// STATS

sai_status_t VendorSai::getStats(
        _In_ sai_object_type_t object_type,
        _In_ sai_object_id_t object_id,
        _In_ uint32_t number_of_counters,
        _In_ const sai_stat_id_t *counter_ids,
        _Out_ uint64_t *counters)
{
    MUTEX();
    SWSS_LOG_ENTER();
    VENDOR_CHECK_API_INITIALIZED();

    if (!counter_ids || !counters)
    {
        SWSS_LOG_ERROR("NULL pointer function argument");
        return SAI_STATUS_INVALID_PARAMETER;
    }

    sai_object_meta_key_t mk = { .objecttype = object_type, .objectkey = { .key = { .object_id = object_id} } };

    return sai_metadata_generic_get_stats(&m_apis, &mk, number_of_counters, counter_ids, counters);
}

sai_status_t VendorSai::queryStatsCapability(
        _In_ sai_object_id_t switchId,
        _In_ sai_object_type_t objectType,
        _Inout_ sai_stat_capability_list_t *stats_capability)
{
    MUTEX();
    SWSS_LOG_ENTER();
    VENDOR_CHECK_API_INITIALIZED();

    return m_globalApis.query_stats_capability(
            switchId,
            objectType,
            stats_capability);
}

sai_status_t VendorSai::queryStatsStCapability(
    _In_ sai_object_id_t switchId,
    _In_ sai_object_type_t objectType,
    _Inout_ sai_stat_st_capability_list_t *stats_capability)
{
    MUTEX();
    SWSS_LOG_ENTER();
    VENDOR_CHECK_API_INITIALIZED();

    return m_globalApis.query_stats_st_capability(
            switchId,
            objectType,
            stats_capability);
}

sai_status_t VendorSai::getStatsExt(
        _In_ sai_object_type_t object_type,
        _In_ sai_object_id_t object_id,
        _In_ uint32_t number_of_counters,
        _In_ const sai_stat_id_t *counter_ids,
        _In_ sai_stats_mode_t mode,
        _Out_ uint64_t *counters)
{
    MUTEX();
    SWSS_LOG_ENTER();
    VENDOR_CHECK_API_INITIALIZED();

    sai_object_meta_key_t mk = { .objecttype = object_type, .objectkey = { .key = { .object_id = object_id} } };

    return sai_metadata_generic_get_stats_ext(&m_apis, &mk, number_of_counters, counter_ids, mode, counters);
}

sai_status_t VendorSai::clearStats(
        _In_ sai_object_type_t object_type,
        _In_ sai_object_id_t object_id,
        _In_ uint32_t number_of_counters,
        _In_ const sai_stat_id_t *counter_ids)
{
    MUTEX();
    SWSS_LOG_ENTER();
    VENDOR_CHECK_API_INITIALIZED();

    sai_object_meta_key_t mk = { .objecttype = object_type, .objectkey = { .key = { .object_id = object_id} } };

    return sai_metadata_generic_clear_stats(&m_apis, &mk, number_of_counters, counter_ids);
}

sai_status_t VendorSai::bulkGetStats(
        _In_ sai_object_id_t switchId,
        _In_ sai_object_type_t object_type,
        _In_ uint32_t object_count,
        _In_ const sai_object_key_t *object_key,
        _In_ uint32_t number_of_counters,
        _In_ const sai_stat_id_t *counter_ids,
        _In_ sai_stats_mode_t mode,
        _Inout_ sai_status_t *object_statuses,
        _Out_ uint64_t *counters)
{
    MUTEX();
    SWSS_LOG_ENTER();
    VENDOR_CHECK_API_INITIALIZED();

    return (m_globalApis.bulk_object_get_stats == nullptr)
        ? SAI_STATUS_NOT_IMPLEMENTED
        : m_globalApis.bulk_object_get_stats(
                switchId,
                object_type,
                object_count,
                object_key,
                number_of_counters,
                counter_ids,
                mode,
                object_statuses,
                counters);
}

sai_status_t VendorSai::bulkClearStats(
        _In_ sai_object_id_t switchId,
        _In_ sai_object_type_t object_type,
        _In_ uint32_t object_count,
        _In_ const sai_object_key_t *object_key,
        _In_ uint32_t number_of_counters,
        _In_ const sai_stat_id_t *counter_ids,
        _In_ sai_stats_mode_t mode,
        _Inout_ sai_status_t *object_statuses)
{
    MUTEX();
    SWSS_LOG_ENTER();
    VENDOR_CHECK_API_INITIALIZED();

    return (m_globalApis.bulk_object_clear_stats == nullptr)
        ? SAI_STATUS_NOT_IMPLEMENTED
        : m_globalApis.bulk_object_clear_stats(
                switchId,
                object_type,
                object_count,
                object_key,
                number_of_counters,
                counter_ids,
                mode,
                object_statuses);
}

// BULK QUAD OID

sai_status_t VendorSai::bulkCreate(
        _In_ sai_object_type_t object_type,
        _In_ sai_object_id_t switch_id,
        _In_ uint32_t object_count,
        _In_ const uint32_t *attr_count,
        _In_ const sai_attribute_t **attr_list,
        _In_ sai_bulk_op_error_mode_t mode,
        _Out_ sai_object_id_t *object_id,
        _Out_ sai_status_t *object_statuses)
{
    MUTEX();
    SWSS_LOG_ENTER();
    VENDOR_CHECK_API_INITIALIZED();

    sai_status_t (*ptr)(
            _In_ sai_object_id_t switch_id,
            _In_ uint32_t object_count,
            _In_ const uint32_t *attr_count,
            _In_ const sai_attribute_t **attr_list,
            _In_ sai_bulk_op_error_mode_t mode,
            _Out_ sai_object_id_t *object_id,
            _Out_ sai_status_t *object_statuses);

    switch ((int)object_type)
    {
        case SAI_OBJECT_TYPE_PORT:
            ptr = m_apis.port_api->create_ports;
            break;

        case SAI_OBJECT_TYPE_LAG_MEMBER:
            ptr = m_apis.lag_api->create_lag_members;
            break;

        case SAI_OBJECT_TYPE_NEXT_HOP_GROUP_MEMBER:
            ptr = m_apis.next_hop_group_api->create_next_hop_group_members;
            break;

        case SAI_OBJECT_TYPE_NEXT_HOP:
            ptr = m_apis.next_hop_api->create_next_hops;
            break;

        case SAI_OBJECT_TYPE_SRV6_SIDLIST:
            ptr = m_apis.srv6_api->create_srv6_sidlists;
            break;

        case SAI_OBJECT_TYPE_STP_PORT:
            ptr = m_apis.stp_api->create_stp_ports;
            break;

        case SAI_OBJECT_TYPE_VLAN_MEMBER:
            ptr = m_apis.vlan_api->create_vlan_members;
            break;

        case SAI_OBJECT_TYPE_ENI:
            ptr = m_apis.dash_eni_api->create_enis;
            break;

        case SAI_OBJECT_TYPE_VNET:
            ptr = m_apis.dash_vnet_api->create_vnets;
            break;

        case SAI_OBJECT_TYPE_DASH_ACL_GROUP:
            ptr = m_apis.dash_acl_api->create_dash_acl_groups;
            break;

        case SAI_OBJECT_TYPE_DASH_ACL_RULE:
            ptr = m_apis.dash_acl_api->create_dash_acl_rules;
            break;

        case SAI_OBJECT_TYPE_METER_RULE:
            ptr = m_apis.dash_meter_api->create_meter_rules;
            break;

        default:
            SWSS_LOG_ERROR("not implemented %s, FIXME", sai_serialize_object_type(object_type).c_str());
            return SAI_STATUS_NOT_IMPLEMENTED;
    }

    if (!ptr)
    {
        SWSS_LOG_INFO("create bulk not supported from SAI, object_type = %s",  sai_serialize_object_type(object_type).c_str());
        return SAI_STATUS_NOT_SUPPORTED;
    }

    return ptr(switch_id,
            object_count,
            attr_count,
            attr_list,
            mode,
            object_id,
            object_statuses);
}

sai_status_t VendorSai::bulkRemove(
        _In_ sai_object_type_t object_type,
        _In_ uint32_t object_count,
        _In_ const sai_object_id_t *object_id,
        _In_ sai_bulk_op_error_mode_t mode,
        _Out_ sai_status_t *object_statuses)
{
    MUTEX();
    SWSS_LOG_ENTER();
    VENDOR_CHECK_API_INITIALIZED();

    sai_status_t (*ptr)(
            _In_ uint32_t object_count,
            _In_ const sai_object_id_t *object_id,
            _In_ sai_bulk_op_error_mode_t mode,
            _Out_ sai_status_t *object_statuses);

    switch ((int)object_type)
    {
        case SAI_OBJECT_TYPE_PORT:
            ptr = m_apis.port_api->remove_ports;
            break;

        case SAI_OBJECT_TYPE_LAG_MEMBER:
            ptr = m_apis.lag_api->remove_lag_members;
            break;

        case SAI_OBJECT_TYPE_NEXT_HOP_GROUP_MEMBER:
            ptr = m_apis.next_hop_group_api->remove_next_hop_group_members;
            break;

        case SAI_OBJECT_TYPE_NEXT_HOP:
            ptr = m_apis.next_hop_api->remove_next_hops;
            break;

        case SAI_OBJECT_TYPE_SRV6_SIDLIST:
            ptr = m_apis.srv6_api->remove_srv6_sidlists;
            break;

        case SAI_OBJECT_TYPE_STP_PORT:
            ptr = m_apis.stp_api->remove_stp_ports;
            break;

        case SAI_OBJECT_TYPE_VLAN_MEMBER:
            ptr = m_apis.vlan_api->remove_vlan_members;
            break;

        case SAI_OBJECT_TYPE_ENI:
            ptr = m_apis.dash_eni_api->remove_enis;
            break;

        case SAI_OBJECT_TYPE_VNET:
            ptr = m_apis.dash_vnet_api->remove_vnets;
            break;

        case SAI_OBJECT_TYPE_DASH_ACL_GROUP:
            ptr = m_apis.dash_acl_api->remove_dash_acl_groups;
            break;

        case SAI_OBJECT_TYPE_DASH_ACL_RULE:
            ptr = m_apis.dash_acl_api->remove_dash_acl_rules;
            break;

        case SAI_OBJECT_TYPE_METER_RULE:
            ptr = m_apis.dash_meter_api->remove_meter_rules;
            break;

        default:
            SWSS_LOG_ERROR("not implemented %s, FIXME", sai_serialize_object_type(object_type).c_str());
            return SAI_STATUS_NOT_IMPLEMENTED;
    }

    if (!ptr)
    {
        SWSS_LOG_INFO("remove bulk not supported from SAI, object_type = %s",  sai_serialize_object_type(object_type).c_str());
        return SAI_STATUS_NOT_SUPPORTED;
    }

    return ptr(object_count, object_id, mode, object_statuses);
}

sai_status_t VendorSai::bulkSet(
        _In_ sai_object_type_t object_type,
        _In_ uint32_t object_count,
        _In_ const sai_object_id_t *object_id,
        _In_ const sai_attribute_t *attr_list,
        _In_ sai_bulk_op_error_mode_t mode,
        _Out_ sai_status_t *object_statuses)
{
    MUTEX();
    SWSS_LOG_ENTER();
    VENDOR_CHECK_API_INITIALIZED();

    sai_bulk_object_set_attribute_fn ptr;

    switch (object_type)
    {
        case SAI_OBJECT_TYPE_PORT:
            ptr = m_apis.port_api->set_ports_attribute;
            break;

        case SAI_OBJECT_TYPE_INGRESS_PRIORITY_GROUP:
            ptr = m_apis.buffer_api->set_ingress_priority_groups_attribute;
            break;

        case SAI_OBJECT_TYPE_QUEUE:
            ptr = m_apis.queue_api->set_queues_attribute;
            break;

        default:
            SWSS_LOG_ERROR("not implemented %s, FIXME", sai_serialize_object_type(object_type).c_str());
            return SAI_STATUS_NOT_IMPLEMENTED;
    }

    if (!ptr)
    {
        SWSS_LOG_INFO("create bulk not supported from SAI, object_type = %s",  sai_serialize_object_type(object_type).c_str());
        return SAI_STATUS_NOT_SUPPORTED;
    }

    return ptr(object_count,
            object_id,
            attr_list,
            mode,
            object_statuses);
}

sai_status_t VendorSai::bulkGet(
        _In_ sai_object_type_t object_type,
        _In_ uint32_t object_count,
        _In_ const sai_object_id_t *object_id,
        _In_ const uint32_t *attr_count,
        _Inout_ sai_attribute_t **attr_list,
        _In_ sai_bulk_op_error_mode_t mode,
        _Out_ sai_status_t *object_statuses)
{
    MUTEX();
    SWSS_LOG_ENTER();
    VENDOR_CHECK_API_INITIALIZED();

    sai_bulk_object_get_attribute_fn ptr;

    switch (object_type)
    {
        case SAI_OBJECT_TYPE_PORT:
            ptr = m_apis.port_api->get_ports_attribute;
            break;

        default:
            SWSS_LOG_ERROR("not implemented %s, FIXME", sai_serialize_object_type(object_type).c_str());
            return SAI_STATUS_NOT_IMPLEMENTED;
    }

    if (!ptr)
    {
        SWSS_LOG_INFO("get bulk not supported in SAI, object_type = %s",  sai_serialize_object_type(object_type).c_str());
        return SAI_STATUS_NOT_SUPPORTED;
    }

    return ptr(object_count,
            object_id,
            attr_count,
            attr_list,
            mode,
            object_statuses);
}

// BULK GET

#define DECLARE_BULK_GET_ENTRY(OT,ot)                       \
sai_status_t VendorSai::bulkGet(                            \
        _In_ uint32_t object_count,                         \
        _In_ const sai_ ## ot ## _t *ot,                    \
        _In_ const uint32_t *attr_count,                    \
        _Inout_ sai_attribute_t **attr_list,                \
        _In_ sai_bulk_op_error_mode_t mode,                 \
        _Out_ sai_status_t *object_statuses)                \
{                                                           \
    MUTEX();                                                \
    SWSS_LOG_ENTER();                                       \
    VENDOR_CHECK_API_INITIALIZED();                         \
    SWSS_LOG_ERROR("FIXME not implemented");                \
    return SAI_STATUS_NOT_IMPLEMENTED;                      \
}

SAIREDIS_DECLARE_EVERY_BULK_ENTRY(DECLARE_BULK_GET_ENTRY);


// BULK QUAD ENTRY

sai_status_t VendorSai::bulkCreate(
        _In_ uint32_t object_count,
        _In_ const sai_route_entry_t* entries,
        _In_ const uint32_t *attr_count,
        _In_ const sai_attribute_t **attr_list,
        _In_ sai_bulk_op_error_mode_t mode,
        _Out_ sai_status_t *object_statuses)
{
    MUTEX();
    SWSS_LOG_ENTER();
    VENDOR_CHECK_API_INITIALIZED();

    if (!m_apis.route_api->create_route_entries)
    {
        SWSS_LOG_INFO("create_route_entries is not supported");
        return SAI_STATUS_NOT_SUPPORTED;
    }

    return m_apis.route_api->create_route_entries(
            object_count,
            entries,
            attr_count,
            attr_list,
            mode,
            object_statuses);
}

sai_status_t VendorSai::bulkCreate(
        _In_ uint32_t object_count,
        _In_ const sai_fdb_entry_t* entries,
        _In_ const uint32_t *attr_count,
        _In_ const sai_attribute_t **attr_list,
        _In_ sai_bulk_op_error_mode_t mode,
        _Out_ sai_status_t *object_statuses)
{
    MUTEX();
    SWSS_LOG_ENTER();
    VENDOR_CHECK_API_INITIALIZED();

    if (!m_apis.fdb_api->create_fdb_entries)
    {
        SWSS_LOG_INFO("create_fdb_entries is not supported");
        return SAI_STATUS_NOT_SUPPORTED;
    }

    return m_apis.fdb_api->create_fdb_entries(
            object_count,
            entries,
            attr_count,
            attr_list,
            mode,
            object_statuses);
}

sai_status_t VendorSai::bulkCreate(
        _In_ uint32_t object_count,
        _In_ const sai_inseg_entry_t* entries,
        _In_ const uint32_t *attr_count,
        _In_ const sai_attribute_t **attr_list,
        _In_ sai_bulk_op_error_mode_t mode,
        _Out_ sai_status_t *object_statuses)
{
    MUTEX();
    SWSS_LOG_ENTER();
    VENDOR_CHECK_API_INITIALIZED();

    if (!m_apis.mpls_api->create_inseg_entries)
    {
        SWSS_LOG_INFO("create_inseg_entries is not supported");
        return SAI_STATUS_NOT_SUPPORTED;
    }

    return m_apis.mpls_api->create_inseg_entries(
            object_count,
            entries,
            attr_count,
            attr_list,
            mode,
            object_statuses);
}

sai_status_t VendorSai::bulkCreate(
        _In_ uint32_t object_count,
        _In_ const sai_nat_entry_t* entries,
        _In_ const uint32_t *attr_count,
        _In_ const sai_attribute_t **attr_list,
        _In_ sai_bulk_op_error_mode_t mode,
        _Out_ sai_status_t *object_statuses)
{
    MUTEX();
    SWSS_LOG_ENTER();
    VENDOR_CHECK_API_INITIALIZED();

    if (!m_apis.nat_api->create_nat_entries)
    {
        SWSS_LOG_INFO("create_nat_entries is not supported");
        return SAI_STATUS_NOT_SUPPORTED;
    }

    return m_apis.nat_api->create_nat_entries(
            object_count,
            entries,
            attr_count,
            attr_list,
            mode,
            object_statuses);
}

sai_status_t VendorSai::bulkCreate(
        _In_ uint32_t object_count,
        _In_ const sai_my_sid_entry_t* entries,
        _In_ const uint32_t *attr_count,
        _In_ const sai_attribute_t **attr_list,
        _In_ sai_bulk_op_error_mode_t mode,
        _Out_ sai_status_t *object_statuses)
{
    MUTEX();
    SWSS_LOG_ENTER();
    VENDOR_CHECK_API_INITIALIZED();

    if (!m_apis.srv6_api->create_my_sid_entries)
    {
        SWSS_LOG_INFO("create_my_sid_entries is not supported");
        return SAI_STATUS_NOT_SUPPORTED;
    }

    return m_apis.srv6_api->create_my_sid_entries(
            object_count,
            entries,
            attr_count,
            attr_list,
            mode,
            object_statuses);
}

sai_status_t VendorSai::bulkCreate(
        _In_ uint32_t object_count,
        _In_ const sai_neighbor_entry_t* entries,
        _In_ const uint32_t *attr_count,
        _In_ const sai_attribute_t **attr_list,
        _In_ sai_bulk_op_error_mode_t mode,
        _Out_ sai_status_t *object_statuses)
{
    MUTEX();
    SWSS_LOG_ENTER();
    VENDOR_CHECK_API_INITIALIZED();

    if (!m_apis.neighbor_api->create_neighbor_entries)
    {
        SWSS_LOG_INFO("create_neighbor_entries is not supported");
        return SAI_STATUS_NOT_SUPPORTED;
    }

    return m_apis.neighbor_api->create_neighbor_entries(
            object_count,
            entries,
            attr_count,
            attr_list,
            mode,
            object_statuses);
}

sai_status_t VendorSai::bulkCreate(
        _In_ uint32_t object_count,
        _In_ const sai_direction_lookup_entry_t* entries,
        _In_ const uint32_t *attr_count,
        _In_ const sai_attribute_t **attr_list,
        _In_ sai_bulk_op_error_mode_t mode,
        _Out_ sai_status_t *object_statuses)
{
    MUTEX();
    SWSS_LOG_ENTER();
    VENDOR_CHECK_API_INITIALIZED();

    if (!m_apis.dash_direction_lookup_api->create_direction_lookup_entries)
    {
        SWSS_LOG_INFO("create_direction_lookup_entries is not supported");
        return SAI_STATUS_NOT_SUPPORTED;
    }

    return m_apis.dash_direction_lookup_api->create_direction_lookup_entries(
            object_count,
            entries,
            attr_count,
            attr_list,
            mode,
            object_statuses);
}

sai_status_t VendorSai::bulkCreate(
        _In_ uint32_t object_count,
        _In_ const sai_eni_ether_address_map_entry_t* entries,
        _In_ const uint32_t *attr_count,
        _In_ const sai_attribute_t **attr_list,
        _In_ sai_bulk_op_error_mode_t mode,
        _Out_ sai_status_t *object_statuses)
{
    MUTEX();
    SWSS_LOG_ENTER();
    VENDOR_CHECK_API_INITIALIZED();

    if (!m_apis.dash_eni_api->create_eni_ether_address_map_entries)
    {
        SWSS_LOG_INFO("create_eni_ether_address_map_entries is not supported");
        return SAI_STATUS_NOT_SUPPORTED;
    }

    return m_apis.dash_eni_api->create_eni_ether_address_map_entries(
            object_count,
            entries,
            attr_count,
            attr_list,
            mode,
            object_statuses);
}

sai_status_t VendorSai::bulkCreate(
        _In_ uint32_t object_count,
        _In_ const sai_vip_entry_t* entries,
        _In_ const uint32_t *attr_count,
        _In_ const sai_attribute_t **attr_list,
        _In_ sai_bulk_op_error_mode_t mode,
        _Out_ sai_status_t *object_statuses)
{
    MUTEX();
    SWSS_LOG_ENTER();
    VENDOR_CHECK_API_INITIALIZED();

    if (!m_apis.dash_vip_api->create_vip_entries)
    {
        SWSS_LOG_INFO("create_vip_entries is not supported");
        return SAI_STATUS_NOT_SUPPORTED;
    }

    return m_apis.dash_vip_api->create_vip_entries(
            object_count,
            entries,
            attr_count,
            attr_list,
            mode,
            object_statuses);
}

sai_status_t VendorSai::bulkCreate(
        _In_ uint32_t object_count,
        _In_ const sai_inbound_routing_entry_t* entries,
        _In_ const uint32_t *attr_count,
        _In_ const sai_attribute_t **attr_list,
        _In_ sai_bulk_op_error_mode_t mode,
        _Out_ sai_status_t *object_statuses)
{
    MUTEX();
    SWSS_LOG_ENTER();
    VENDOR_CHECK_API_INITIALIZED();

    if (!m_apis.dash_inbound_routing_api->create_inbound_routing_entries)
    {
        SWSS_LOG_INFO("create_inbound_routing_entries is not supported");
        return SAI_STATUS_NOT_SUPPORTED;
    }

    return m_apis.dash_inbound_routing_api->create_inbound_routing_entries(
            object_count,
            entries,
            attr_count,
            attr_list,
            mode,
            object_statuses);
}

sai_status_t VendorSai::bulkCreate(
        _In_ uint32_t object_count,
        _In_ const sai_pa_validation_entry_t* entries,
        _In_ const uint32_t *attr_count,
        _In_ const sai_attribute_t **attr_list,
        _In_ sai_bulk_op_error_mode_t mode,
        _Out_ sai_status_t *object_statuses)
{
    MUTEX();
    SWSS_LOG_ENTER();
    VENDOR_CHECK_API_INITIALIZED();

    if (!m_apis.dash_pa_validation_api->create_pa_validation_entries)
    {
        SWSS_LOG_INFO("create_pa_validation_entries is not supported");
        return SAI_STATUS_NOT_SUPPORTED;
    }

    return m_apis.dash_pa_validation_api->create_pa_validation_entries(
            object_count,
            entries,
            attr_count,
            attr_list,
            mode,
            object_statuses);
}

sai_status_t VendorSai::bulkCreate(
        _In_ uint32_t object_count,
        _In_ const sai_outbound_routing_entry_t* entries,
        _In_ const uint32_t *attr_count,
        _In_ const sai_attribute_t **attr_list,
        _In_ sai_bulk_op_error_mode_t mode,
        _Out_ sai_status_t *object_statuses)
{
    MUTEX();
    SWSS_LOG_ENTER();
    VENDOR_CHECK_API_INITIALIZED();

    if (!m_apis.dash_outbound_routing_api->create_outbound_routing_entries)
    {
        SWSS_LOG_INFO("create_outbound_routing_entries is not supported");
        return SAI_STATUS_NOT_SUPPORTED;
    }

    return m_apis.dash_outbound_routing_api->create_outbound_routing_entries(
            object_count,
            entries,
            attr_count,
            attr_list,
            mode,
            object_statuses);
}

sai_status_t VendorSai::bulkCreate(
        _In_ uint32_t object_count,
        _In_ const sai_outbound_ca_to_pa_entry_t* entries,
        _In_ const uint32_t *attr_count,
        _In_ const sai_attribute_t **attr_list,
        _In_ sai_bulk_op_error_mode_t mode,
        _Out_ sai_status_t *object_statuses)
{
    MUTEX();
    SWSS_LOG_ENTER();
    VENDOR_CHECK_API_INITIALIZED();

    if (!m_apis.dash_outbound_ca_to_pa_api->create_outbound_ca_to_pa_entries)
    {
        SWSS_LOG_INFO("create_outbound_ca_to_pa_entries is not supported");
        return SAI_STATUS_NOT_SUPPORTED;
    }

    return m_apis.dash_outbound_ca_to_pa_api->create_outbound_ca_to_pa_entries(
            object_count,
            entries,
            attr_count,
            attr_list,
            mode,
            object_statuses);
}

sai_status_t VendorSai::bulkCreate(
        _In_ uint32_t object_count,
        _In_ const sai_flow_entry_t* entries,
        _In_ const uint32_t *attr_count,
        _In_ const sai_attribute_t **attr_list,
        _In_ sai_bulk_op_error_mode_t mode,
        _Out_ sai_status_t *object_statuses)
{
    MUTEX();
    SWSS_LOG_ENTER();
    VENDOR_CHECK_API_INITIALIZED();

    if (!m_apis.dash_flow_api->create_flow_entries)
    {
        SWSS_LOG_INFO("create_flow_entries is not supported");
        return SAI_STATUS_NOT_SUPPORTED;
    }

    return m_apis.dash_flow_api->create_flow_entries(
            object_count,
            entries,
            attr_count,
            attr_list,
            mode,
            object_statuses);
}

sai_status_t VendorSai::bulkCreate(
        _In_ uint32_t object_count,
        _In_ const sai_meter_bucket_entry_t* entries,
        _In_ const uint32_t *attr_count,
        _In_ const sai_attribute_t **attr_list,
        _In_ sai_bulk_op_error_mode_t mode,
        _Out_ sai_status_t *object_statuses)
{
    MUTEX();
    SWSS_LOG_ENTER();
    VENDOR_CHECK_API_INITIALIZED();

    if (!m_apis.dash_meter_api->create_meter_bucket_entries)
    {
        SWSS_LOG_INFO("create_meter_bucket_entries is not supported");
        return SAI_STATUS_NOT_SUPPORTED;
    }

    return m_apis.dash_meter_api->create_meter_bucket_entries(
            object_count,
            entries,
            attr_count,
            attr_list,
            mode,
            object_statuses);
}

sai_status_t VendorSai::bulkCreate(
        _In_ uint32_t object_count,
        _In_ const sai_prefix_compression_entry_t* entries,
        _In_ const uint32_t *attr_count,
        _In_ const sai_attribute_t **attr_list,
        _In_ sai_bulk_op_error_mode_t mode,
        _Out_ sai_status_t *object_statuses)
{
    MUTEX();
    SWSS_LOG_ENTER();
    VENDOR_CHECK_API_INITIALIZED();

    if (!m_apis.prefix_compression_api->create_prefix_compression_entries)
    {
        SWSS_LOG_INFO("create_prefix_compression_entries is not supported");
        return SAI_STATUS_NOT_SUPPORTED;
    }

    return m_apis.prefix_compression_api->create_prefix_compression_entries(
            object_count,
            entries,
            attr_count,
            attr_list,
            mode,
            object_statuses);
}

// BULK REMOVE

sai_status_t VendorSai::bulkRemove(
        _In_ uint32_t object_count,
        _In_ const sai_route_entry_t *entries,
        _In_ sai_bulk_op_error_mode_t mode,
        _Out_ sai_status_t *object_statuses)
{
    MUTEX();
    SWSS_LOG_ENTER();
    VENDOR_CHECK_API_INITIALIZED();

    if (!m_apis.route_api->remove_route_entries)
    {
        SWSS_LOG_INFO("remove_route_entries is not supported");
        return SAI_STATUS_NOT_SUPPORTED;
    }

    return m_apis.route_api->remove_route_entries(
            object_count,
            entries,
            mode,
            object_statuses);
}


sai_status_t VendorSai::bulkRemove(
        _In_ uint32_t object_count,
        _In_ const sai_fdb_entry_t *entries,
        _In_ sai_bulk_op_error_mode_t mode,
        _Out_ sai_status_t *object_statuses)
{
    MUTEX();
    SWSS_LOG_ENTER();
    VENDOR_CHECK_API_INITIALIZED();

    if (!m_apis.fdb_api->remove_fdb_entries)
    {
        SWSS_LOG_INFO("remove_fdb_entries is not supported");
        return SAI_STATUS_NOT_SUPPORTED;
    }

    return m_apis.fdb_api->remove_fdb_entries(
            object_count,
            entries,
            mode,
            object_statuses);;
}

sai_status_t VendorSai::bulkRemove(
        _In_ uint32_t object_count,
        _In_ const sai_inseg_entry_t *entries,
        _In_ sai_bulk_op_error_mode_t mode,
        _Out_ sai_status_t *object_statuses)
{
    MUTEX();
    SWSS_LOG_ENTER();
    VENDOR_CHECK_API_INITIALIZED();

    if (!m_apis.mpls_api->remove_inseg_entries)
    {
        SWSS_LOG_INFO("remove_inseg_entries is not supported");
        return SAI_STATUS_NOT_SUPPORTED;
    }

    return m_apis.mpls_api->remove_inseg_entries(
            object_count,
            entries,
            mode,
            object_statuses);;
}

sai_status_t VendorSai::bulkRemove(
        _In_ uint32_t object_count,
        _In_ const sai_nat_entry_t *entries,
        _In_ sai_bulk_op_error_mode_t mode,
        _Out_ sai_status_t *object_statuses)
{
    MUTEX();
    SWSS_LOG_ENTER();
    VENDOR_CHECK_API_INITIALIZED();

    if (!m_apis.nat_api->remove_nat_entries)
    {
        SWSS_LOG_INFO("remove_nat_entries is not supported");
        return SAI_STATUS_NOT_SUPPORTED;
    }

    return m_apis.nat_api->remove_nat_entries(
            object_count,
            entries,
            mode,
            object_statuses);
}

sai_status_t VendorSai::bulkRemove(
        _In_ uint32_t object_count,
        _In_ const sai_my_sid_entry_t *entries,
        _In_ sai_bulk_op_error_mode_t mode,
        _Out_ sai_status_t *object_statuses)
{
    MUTEX();
    SWSS_LOG_ENTER();
    VENDOR_CHECK_API_INITIALIZED();

    if (!m_apis.srv6_api->remove_my_sid_entries)
    {
        SWSS_LOG_INFO("remove_my_sid_entries is not supported");
        return SAI_STATUS_NOT_SUPPORTED;
    }

    return m_apis.srv6_api->remove_my_sid_entries(
            object_count,
            entries,
            mode,
            object_statuses);
}

sai_status_t VendorSai::bulkRemove(
        _In_ uint32_t object_count,
        _In_ const sai_neighbor_entry_t *entries,
        _In_ sai_bulk_op_error_mode_t mode,
        _Out_ sai_status_t *object_statuses)
{
    MUTEX();
    SWSS_LOG_ENTER();
    VENDOR_CHECK_API_INITIALIZED();

    if (!m_apis.neighbor_api->remove_neighbor_entries)
    {
        SWSS_LOG_INFO("remove_neighbor_entries is not supported");
        return SAI_STATUS_NOT_SUPPORTED;
    }

    return m_apis.neighbor_api->remove_neighbor_entries(
            object_count,
            entries,
            mode,
            object_statuses);
}

sai_status_t VendorSai::bulkRemove(
        _In_ uint32_t object_count,
        _In_ const sai_direction_lookup_entry_t *entries,
        _In_ sai_bulk_op_error_mode_t mode,
        _Out_ sai_status_t *object_statuses)
{
    MUTEX();
    SWSS_LOG_ENTER();
    VENDOR_CHECK_API_INITIALIZED();

    if (!m_apis.dash_direction_lookup_api->remove_direction_lookup_entries)
    {
        SWSS_LOG_INFO("remove_direction_lookup_entries is not supported");
        return SAI_STATUS_NOT_SUPPORTED;
    }

    return m_apis.dash_direction_lookup_api->remove_direction_lookup_entries(
            object_count,
            entries,
            mode,
            object_statuses);
}

sai_status_t VendorSai::bulkRemove(
        _In_ uint32_t object_count,
        _In_ const sai_eni_ether_address_map_entry_t *entries,
        _In_ sai_bulk_op_error_mode_t mode,
        _Out_ sai_status_t *object_statuses)
{
    MUTEX();
    SWSS_LOG_ENTER();
    VENDOR_CHECK_API_INITIALIZED();

    if (!m_apis.dash_eni_api->remove_eni_ether_address_map_entries)
    {
        SWSS_LOG_INFO("remove_eni_ether_address_map_entries is not supported");
        return SAI_STATUS_NOT_SUPPORTED;
    }

    return m_apis.dash_eni_api->remove_eni_ether_address_map_entries(
            object_count,
            entries,
            mode,
            object_statuses);
}

sai_status_t VendorSai::bulkRemove(
        _In_ uint32_t object_count,
        _In_ const sai_vip_entry_t *entries,
        _In_ sai_bulk_op_error_mode_t mode,
        _Out_ sai_status_t *object_statuses)
{
    MUTEX();
    SWSS_LOG_ENTER();
    VENDOR_CHECK_API_INITIALIZED();

    if (!m_apis.dash_vip_api->remove_vip_entries)
    {
        SWSS_LOG_INFO("remove_vip_entries is not supported");
        return SAI_STATUS_NOT_SUPPORTED;
    }

    return m_apis.dash_vip_api->remove_vip_entries(
            object_count,
            entries,
            mode,
            object_statuses);
}

sai_status_t VendorSai::bulkRemove(
        _In_ uint32_t object_count,
        _In_ const sai_inbound_routing_entry_t *entries,
        _In_ sai_bulk_op_error_mode_t mode,
        _Out_ sai_status_t *object_statuses)
{
    MUTEX();
    SWSS_LOG_ENTER();
    VENDOR_CHECK_API_INITIALIZED();

    if (!m_apis.dash_inbound_routing_api->remove_inbound_routing_entries)
    {
        SWSS_LOG_INFO("remove_inbound_routing_entries is not supported");
        return SAI_STATUS_NOT_SUPPORTED;
    }

    return m_apis.dash_inbound_routing_api->remove_inbound_routing_entries(
            object_count,
            entries,
            mode,
            object_statuses);
}

sai_status_t VendorSai::bulkRemove(
        _In_ uint32_t object_count,
        _In_ const sai_pa_validation_entry_t *entries,
        _In_ sai_bulk_op_error_mode_t mode,
        _Out_ sai_status_t *object_statuses)
{
    MUTEX();
    SWSS_LOG_ENTER();
    VENDOR_CHECK_API_INITIALIZED();

    if (!m_apis.dash_pa_validation_api->remove_pa_validation_entries)
    {
        SWSS_LOG_INFO("remove_pa_validation_entries is not supported");
        return SAI_STATUS_NOT_SUPPORTED;
    }

    return m_apis.dash_pa_validation_api->remove_pa_validation_entries(
            object_count,
            entries,
            mode,
            object_statuses);
}

sai_status_t VendorSai::bulkRemove(
        _In_ uint32_t object_count,
        _In_ const sai_outbound_routing_entry_t *entries,
        _In_ sai_bulk_op_error_mode_t mode,
        _Out_ sai_status_t *object_statuses)
{
    MUTEX();
    SWSS_LOG_ENTER();
    VENDOR_CHECK_API_INITIALIZED();

    if (!m_apis.dash_outbound_routing_api->remove_outbound_routing_entries)
    {
        SWSS_LOG_INFO("remove_outbound_routing_entries is not supported");
        return SAI_STATUS_NOT_SUPPORTED;
    }

    return m_apis.dash_outbound_routing_api->remove_outbound_routing_entries(
            object_count,
            entries,
            mode,
            object_statuses);
}

sai_status_t VendorSai::bulkRemove(
        _In_ uint32_t object_count,
        _In_ const sai_outbound_ca_to_pa_entry_t *entries,
        _In_ sai_bulk_op_error_mode_t mode,
        _Out_ sai_status_t *object_statuses)
{
    MUTEX();
    SWSS_LOG_ENTER();
    VENDOR_CHECK_API_INITIALIZED();

    if (!m_apis.dash_outbound_ca_to_pa_api->remove_outbound_ca_to_pa_entries)
    {
        SWSS_LOG_INFO("remove_outbound_ca_to_pa_entries is not supported");
        return SAI_STATUS_NOT_SUPPORTED;
    }

    return m_apis.dash_outbound_ca_to_pa_api->remove_outbound_ca_to_pa_entries(
            object_count,
            entries,
            mode,
            object_statuses);
}

sai_status_t VendorSai::bulkRemove(
        _In_ uint32_t object_count,
        _In_ const sai_flow_entry_t *entries,
        _In_ sai_bulk_op_error_mode_t mode,
        _Out_ sai_status_t *object_statuses)
{
    MUTEX();
    SWSS_LOG_ENTER();
    VENDOR_CHECK_API_INITIALIZED();

    if (!m_apis.dash_flow_api->remove_flow_entries)
    {
        SWSS_LOG_INFO("remove_flow_entries is not supported");
        return SAI_STATUS_NOT_SUPPORTED;
    }

    return m_apis.dash_flow_api->remove_flow_entries(
            object_count,
            entries,
            mode,
            object_statuses);
}

sai_status_t VendorSai::bulkRemove(
        _In_ uint32_t object_count,
        _In_ const sai_meter_bucket_entry_t *entries,
        _In_ sai_bulk_op_error_mode_t mode,
        _Out_ sai_status_t *object_statuses)
{
    MUTEX();
    SWSS_LOG_ENTER();
    VENDOR_CHECK_API_INITIALIZED();

    if (!m_apis.dash_meter_api->remove_meter_bucket_entries)
    {
        SWSS_LOG_INFO("remove_meter_bucket_entries is not supported");
        return SAI_STATUS_NOT_SUPPORTED;
    }

    return m_apis.dash_meter_api->remove_meter_bucket_entries(
            object_count,
            entries,
            mode,
            object_statuses);
}

sai_status_t VendorSai::bulkRemove(
        _In_ uint32_t object_count,
        _In_ const sai_prefix_compression_entry_t *entries,
        _In_ sai_bulk_op_error_mode_t mode,
        _Out_ sai_status_t *object_statuses)
{
    MUTEX();
    SWSS_LOG_ENTER();
    VENDOR_CHECK_API_INITIALIZED();

    if (!m_apis.prefix_compression_api->remove_prefix_compression_entries)
    {
        SWSS_LOG_INFO("remove_prefix_compression_entries is not supported");
        return SAI_STATUS_NOT_SUPPORTED;
    }

    return m_apis.prefix_compression_api->remove_prefix_compression_entries(
            object_count,
            entries,
            mode,
            object_statuses);
}

// BULK SET

sai_status_t VendorSai::bulkSet(
        _In_ uint32_t object_count,
        _In_ const sai_route_entry_t *entries,
        _In_ const sai_attribute_t *attr_list,
        _In_ sai_bulk_op_error_mode_t mode,
        _Out_ sai_status_t *object_statuses)
{
    MUTEX();
    SWSS_LOG_ENTER();
    VENDOR_CHECK_API_INITIALIZED();

    if (!m_apis.route_api->set_route_entries_attribute)
    {
        SWSS_LOG_INFO("set_route_entries_attribute is not supported");
        return SAI_STATUS_NOT_SUPPORTED;
    }

    return m_apis.route_api->set_route_entries_attribute(
            object_count,
            entries,
            attr_list,
            mode,
            object_statuses);
}

sai_status_t VendorSai::bulkSet(
        _In_ uint32_t object_count,
        _In_ const sai_fdb_entry_t *entries,
        _In_ const sai_attribute_t *attr_list,
        _In_ sai_bulk_op_error_mode_t mode,
        _Out_ sai_status_t *object_statuses)
{
    MUTEX();
    SWSS_LOG_ENTER();
    VENDOR_CHECK_API_INITIALIZED();

    if (!m_apis.fdb_api->set_fdb_entries_attribute)
    {
        SWSS_LOG_INFO("set_fdb_entries_attribute is not supported");
        return SAI_STATUS_NOT_SUPPORTED;
    }

    return m_apis.fdb_api->set_fdb_entries_attribute(
            object_count,
            entries,
            attr_list,
            mode,
            object_statuses);;
}

sai_status_t VendorSai::bulkSet(
        _In_ uint32_t object_count,
        _In_ const sai_inseg_entry_t *entries,
        _In_ const sai_attribute_t *attr_list,
        _In_ sai_bulk_op_error_mode_t mode,
        _Out_ sai_status_t *object_statuses)
{
    MUTEX();
    SWSS_LOG_ENTER();
    VENDOR_CHECK_API_INITIALIZED();

    if (!m_apis.mpls_api->set_inseg_entries_attribute)
    {
        SWSS_LOG_INFO("set_inseg_entries_attribute is not supported");
        return SAI_STATUS_NOT_SUPPORTED;
    }

    return m_apis.mpls_api->set_inseg_entries_attribute(
            object_count,
            entries,
            attr_list,
            mode,
            object_statuses);;
}

sai_status_t VendorSai::bulkSet(
        _In_ uint32_t object_count,
        _In_ const sai_nat_entry_t *entries,
        _In_ const sai_attribute_t *attr_list,
        _In_ sai_bulk_op_error_mode_t mode,
        _Out_ sai_status_t *object_statuses)
{
    MUTEX();
    SWSS_LOG_ENTER();
    VENDOR_CHECK_API_INITIALIZED();

    if (!m_apis.nat_api->set_nat_entries_attribute)
    {
        SWSS_LOG_INFO("set_nat_entries_attribute is not supported");
        return SAI_STATUS_NOT_SUPPORTED;
    }

    return m_apis.nat_api->set_nat_entries_attribute(
            object_count,
            entries,
            attr_list,
            mode,
            object_statuses);
}

sai_status_t VendorSai::bulkSet(
        _In_ uint32_t object_count,
        _In_ const sai_my_sid_entry_t *entries,
        _In_ const sai_attribute_t *attr_list,
        _In_ sai_bulk_op_error_mode_t mode,
        _Out_ sai_status_t *object_statuses)
{
    MUTEX();
    SWSS_LOG_ENTER();
    VENDOR_CHECK_API_INITIALIZED();

    if (!m_apis.srv6_api->set_my_sid_entries_attribute)
    {
        SWSS_LOG_INFO("set_my_sid_entries_attribute is not supported");
        return SAI_STATUS_NOT_SUPPORTED;
    }

    return m_apis.srv6_api->set_my_sid_entries_attribute(
            object_count,
            entries,
            attr_list,
            mode,
            object_statuses);
}

sai_status_t VendorSai::bulkSet(
        _In_ uint32_t object_count,
        _In_ const sai_neighbor_entry_t *entries,
        _In_ const sai_attribute_t *attr_list,
        _In_ sai_bulk_op_error_mode_t mode,
        _Out_ sai_status_t *object_statuses)
{
    MUTEX();
    SWSS_LOG_ENTER();
    VENDOR_CHECK_API_INITIALIZED();

    if (!m_apis.neighbor_api->set_neighbor_entries_attribute)
    {
        SWSS_LOG_INFO("set_neighbor_entries_attribute is not supported");
        return SAI_STATUS_NOT_SUPPORTED;
    }

    return m_apis.neighbor_api->set_neighbor_entries_attribute(
            object_count,
            entries,
            attr_list,
            mode,
            object_statuses);
}

sai_status_t VendorSai::bulkSet(
        _In_ uint32_t object_count,
        _In_ const sai_direction_lookup_entry_t *entries,
        _In_ const sai_attribute_t *attr_list,
        _In_ sai_bulk_op_error_mode_t mode,
        _Out_ sai_status_t *object_statuses)
{
    MUTEX();
    SWSS_LOG_ENTER();
    VENDOR_CHECK_API_INITIALIZED();

    return SAI_STATUS_NOT_SUPPORTED;
}

sai_status_t VendorSai::bulkSet(
        _In_ uint32_t object_count,
        _In_ const sai_eni_ether_address_map_entry_t *entries,
        _In_ const sai_attribute_t *attr_list,
        _In_ sai_bulk_op_error_mode_t mode,
        _Out_ sai_status_t *object_statuses)
{
    MUTEX();
    SWSS_LOG_ENTER();
    VENDOR_CHECK_API_INITIALIZED();

    return SAI_STATUS_NOT_SUPPORTED;
}

sai_status_t VendorSai::bulkSet(
        _In_ uint32_t object_count,
        _In_ const sai_vip_entry_t *entries,
        _In_ const sai_attribute_t *attr_list,
        _In_ sai_bulk_op_error_mode_t mode,
        _Out_ sai_status_t *object_statuses)
{
    MUTEX();
    SWSS_LOG_ENTER();
    VENDOR_CHECK_API_INITIALIZED();

    return SAI_STATUS_NOT_SUPPORTED;
}

sai_status_t VendorSai::bulkSet(
        _In_ uint32_t object_count,
        _In_ const sai_inbound_routing_entry_t *entries,
        _In_ const sai_attribute_t *attr_list,
        _In_ sai_bulk_op_error_mode_t mode,
        _Out_ sai_status_t *object_statuses)
{
    MUTEX();
    SWSS_LOG_ENTER();
    VENDOR_CHECK_API_INITIALIZED();

    return SAI_STATUS_NOT_SUPPORTED;
}

sai_status_t VendorSai::bulkSet(
        _In_ uint32_t object_count,
        _In_ const sai_pa_validation_entry_t *entries,
        _In_ const sai_attribute_t *attr_list,
        _In_ sai_bulk_op_error_mode_t mode,
        _Out_ sai_status_t *object_statuses)
{
    MUTEX();
    SWSS_LOG_ENTER();
    VENDOR_CHECK_API_INITIALIZED();

    return SAI_STATUS_NOT_SUPPORTED;
}

sai_status_t VendorSai::bulkSet(
        _In_ uint32_t object_count,
        _In_ const sai_outbound_routing_entry_t *entries,
        _In_ const sai_attribute_t *attr_list,
        _In_ sai_bulk_op_error_mode_t mode,
        _Out_ sai_status_t *object_statuses)
{
    MUTEX();
    SWSS_LOG_ENTER();
    VENDOR_CHECK_API_INITIALIZED();

    return SAI_STATUS_NOT_SUPPORTED;
}

sai_status_t VendorSai::bulkSet(
        _In_ uint32_t object_count,
        _In_ const sai_outbound_ca_to_pa_entry_t *entries,
        _In_ const sai_attribute_t *attr_list,
        _In_ sai_bulk_op_error_mode_t mode,
        _Out_ sai_status_t *object_statuses)
{
    MUTEX();
    SWSS_LOG_ENTER();
    VENDOR_CHECK_API_INITIALIZED();

    return SAI_STATUS_NOT_SUPPORTED;
}

sai_status_t VendorSai::bulkSet(
        _In_ uint32_t object_count,
        _In_ const sai_flow_entry_t *entries,
        _In_ const sai_attribute_t *attr_list,
        _In_ sai_bulk_op_error_mode_t mode,
        _Out_ sai_status_t *object_statuses)
{
    MUTEX();
    SWSS_LOG_ENTER();
    VENDOR_CHECK_API_INITIALIZED();

    return SAI_STATUS_NOT_SUPPORTED;
}

sai_status_t VendorSai::bulkSet(
        _In_ uint32_t object_count,
        _In_ const sai_meter_bucket_entry_t *entries,
        _In_ const sai_attribute_t *attr_list,
        _In_ sai_bulk_op_error_mode_t mode,
        _Out_ sai_status_t *object_statuses)
{
    MUTEX();
    SWSS_LOG_ENTER();
    VENDOR_CHECK_API_INITIALIZED();

    return SAI_STATUS_NOT_SUPPORTED;
}

sai_status_t VendorSai::bulkSet(
        _In_ uint32_t object_count,
        _In_ const sai_prefix_compression_entry_t *entries,
        _In_ const sai_attribute_t *attr_list,
        _In_ sai_bulk_op_error_mode_t mode,
        _Out_ sai_status_t *object_statuses)
{
    MUTEX();
    SWSS_LOG_ENTER();
    VENDOR_CHECK_API_INITIALIZED();

    return SAI_STATUS_NOT_SUPPORTED;
}

// NON QUAD API

sai_status_t VendorSai::flushFdbEntries(
        _In_ sai_object_id_t switch_id,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    MUTEX();
    SWSS_LOG_ENTER();
    VENDOR_CHECK_API_INITIALIZED();

    return m_apis.fdb_api->flush_fdb_entries(switch_id, attr_count, attr_list);
}

sai_status_t VendorSai::switchMdioRead(
        _In_ sai_object_id_t switch_id,
        _In_ uint32_t device_addr,
        _In_ uint32_t start_reg_addr,
        _In_ uint32_t number_of_registers,
        _Out_ uint32_t *reg_val)
{
    MUTEX();
    SWSS_LOG_ENTER();
    VENDOR_CHECK_API_INITIALIZED();

    return m_apis.switch_api->switch_mdio_read(switch_id, device_addr, start_reg_addr, number_of_registers, reg_val);
}

sai_status_t VendorSai::switchMdioWrite(
        _In_ sai_object_id_t switch_id,
        _In_ uint32_t device_addr,
        _In_ uint32_t start_reg_addr,
        _In_ uint32_t number_of_registers,
        _In_ const uint32_t *reg_val)
{
    MUTEX();
    SWSS_LOG_ENTER();
    VENDOR_CHECK_API_INITIALIZED();

    return m_apis.switch_api->switch_mdio_write(switch_id, device_addr, start_reg_addr, number_of_registers, reg_val);
}

sai_status_t VendorSai::switchMdioCl22Read(
        _In_ sai_object_id_t switch_id,
        _In_ uint32_t device_addr,
        _In_ uint32_t start_reg_addr,
        _In_ uint32_t number_of_registers,
        _Out_ uint32_t *reg_val)
{
    MUTEX();
    SWSS_LOG_ENTER();
    VENDOR_CHECK_API_INITIALIZED();

#if (SAI_API_VERSION >= SAI_VERSION(1, 11, 0))
    return m_apis.switch_api->switch_mdio_cl22_read(switch_id, device_addr, start_reg_addr, number_of_registers, reg_val);
#else
    return m_apis.switch_api->switch_mdio_read(switch_id, device_addr, start_reg_addr, number_of_registers, reg_val);
#endif
}

sai_status_t VendorSai::switchMdioCl22Write(
        _In_ sai_object_id_t switch_id,
        _In_ uint32_t device_addr,
        _In_ uint32_t start_reg_addr,
        _In_ uint32_t number_of_registers,
        _In_ const uint32_t *reg_val)
{
    MUTEX();
    SWSS_LOG_ENTER();
    VENDOR_CHECK_API_INITIALIZED();

#if (SAI_API_VERSION >= SAI_VERSION(1, 11, 0))
    return m_apis.switch_api->switch_mdio_cl22_write(switch_id, device_addr, start_reg_addr, number_of_registers, reg_val);
#else
    return m_apis.switch_api->switch_mdio_write(switch_id, device_addr, start_reg_addr, number_of_registers, reg_val);
#endif
}

// SAI API

sai_status_t VendorSai::objectTypeGetAvailability(
        _In_ sai_object_id_t switchId,
        _In_ sai_object_type_t objectType,
        _In_ uint32_t attrCount,
        _In_ const sai_attribute_t *attrList,
        _Out_ uint64_t *count)
{
    MUTEX();
    SWSS_LOG_ENTER();
    VENDOR_CHECK_API_INITIALIZED();

    return m_globalApis.object_type_get_availability(
            switchId,
            objectType,
            attrCount,
            attrList,
            count);
}

sai_status_t VendorSai::queryAttributeCapability(
        _In_ sai_object_id_t switchId,
        _In_ sai_object_type_t objectType,
        _In_ sai_attr_id_t attrId,
        _Out_ sai_attr_capability_t *capability)
{
    MUTEX();
    SWSS_LOG_ENTER();
    VENDOR_CHECK_API_INITIALIZED();

    return m_globalApis.query_attribute_capability(
            switchId,
            objectType,
            attrId,
            capability);
}

sai_status_t VendorSai::queryAttributeEnumValuesCapability(
        _In_ sai_object_id_t switchId,
        _In_ sai_object_type_t objectType,
        _In_ sai_attr_id_t attrId,
        _Inout_ sai_s32_list_t *enum_values_capability)
{
    MUTEX();
    SWSS_LOG_ENTER();
    VENDOR_CHECK_API_INITIALIZED();

    return m_globalApis.query_attribute_enum_values_capability(
            switchId,
            objectType,
            attrId,
            enum_values_capability);
}

sai_object_type_t VendorSai::objectTypeQuery(
        _In_ sai_object_id_t objectId)
{
    SWSS_LOG_ENTER();

    if (!m_apiInitialized)
    {
        SWSS_LOG_ERROR("%s: SAI API not initialized", __PRETTY_FUNCTION__);

        return SAI_OBJECT_TYPE_NULL;
    }

    return m_globalApis.object_type_query(objectId);
}

sai_object_id_t VendorSai::switchIdQuery(
        _In_ sai_object_id_t objectId)
{
    SWSS_LOG_ENTER();

    if (!m_apiInitialized)
    {
        SWSS_LOG_ERROR("%s: SAI API not initialized", __PRETTY_FUNCTION__);

        return SAI_NULL_OBJECT_ID;
    }

    return m_globalApis.switch_id_query(objectId);
}

sai_status_t VendorSai::logSet(
        _In_ sai_api_t api,
        _In_ sai_log_level_t log_level)
{
    MUTEX();
    SWSS_LOG_ENTER();

    m_logLevelMap[api] = log_level;

    return m_globalApis.log_set(api, log_level);
}

sai_status_t VendorSai::queryApiVersion(
        _Out_ sai_api_version_t *version)
{
    SWSS_LOG_ENTER();

    return m_globalApis.query_api_version(version);
}

sai_log_level_t VendorSai::logGet(
        _In_ sai_api_t api)
{
    MUTEX();
    SWSS_LOG_ENTER();

    auto it = m_logLevelMap.find(api);

    if (it != m_logLevelMap.end())
    {
        return it->second;
    }

    // no level defined yet, just return default

    return SAI_LOG_LEVEL_NOTICE;
}
