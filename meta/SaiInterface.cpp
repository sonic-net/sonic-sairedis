#include "SaiInterface.h"

#include "swss/logger.h"

using namespace sairedis;

sai_status_t SaiInterface::create(
        _Inout_ sai_object_meta_key_t& metaKey,
        _In_ sai_object_id_t switch_id,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    auto info = sai_metadata_get_object_type_info(metaKey.objecttype);

    if (!info)
    {
        SWSS_LOG_ERROR("invalid object type: %d", metaKey.objecttype);

        return SAI_STATUS_FAILURE;
    }

    if (info->isobjectid)
    {
        return create(metaKey.objecttype, &metaKey.objectkey.key.object_id, switch_id, attr_count, attr_list);
    }

    switch ((int)info->objecttype)
    {
        case SAI_OBJECT_TYPE_FDB_ENTRY:
            return create(&metaKey.objectkey.key.fdb_entry, attr_count, attr_list);

        case SAI_OBJECT_TYPE_ROUTE_ENTRY:
            return create(&metaKey.objectkey.key.route_entry, attr_count, attr_list);

        case SAI_OBJECT_TYPE_NEIGHBOR_ENTRY:
            return create(&metaKey.objectkey.key.neighbor_entry, attr_count, attr_list);

        case SAI_OBJECT_TYPE_NAT_ENTRY:
            return create(&metaKey.objectkey.key.nat_entry, attr_count, attr_list);

        case SAI_OBJECT_TYPE_INSEG_ENTRY:
            return create(&metaKey.objectkey.key.inseg_entry, attr_count, attr_list);

        case SAI_OBJECT_TYPE_MY_SID_ENTRY:
            return create(&metaKey.objectkey.key.my_sid_entry, attr_count, attr_list);

        case SAI_OBJECT_TYPE_DIRECTION_LOOKUP_ENTRY:
            return create(&metaKey.objectkey.key.direction_lookup_entry, attr_count, attr_list);

        case SAI_OBJECT_TYPE_ENI_ETHER_ADDRESS_MAP_ENTRY:
            return create(&metaKey.objectkey.key.eni_ether_address_map_entry, attr_count, attr_list);

        case SAI_OBJECT_TYPE_VIP_ENTRY:
            return create(&metaKey.objectkey.key.vip_entry, attr_count, attr_list);

        case SAI_OBJECT_TYPE_INBOUND_ROUTING_ENTRY:
            return create(&metaKey.objectkey.key.inbound_routing_entry, attr_count, attr_list);

        case SAI_OBJECT_TYPE_PA_VALIDATION_ENTRY:
            return create(&metaKey.objectkey.key.pa_validation_entry, attr_count, attr_list);

        case SAI_OBJECT_TYPE_OUTBOUND_ROUTING_ENTRY:
            return create(&metaKey.objectkey.key.outbound_routing_entry, attr_count, attr_list);

        case SAI_OBJECT_TYPE_OUTBOUND_CA_TO_PA_ENTRY:
            return create(&metaKey.objectkey.key.outbound_ca_to_pa_entry, attr_count, attr_list);

        default:

            SWSS_LOG_ERROR("object type %s not implemented, FIXME", info->objecttypename);

            return SAI_STATUS_FAILURE;
    }
}

sai_status_t SaiInterface::remove(
        _In_ const sai_object_meta_key_t& metaKey)
{
    SWSS_LOG_ENTER();

    auto info = sai_metadata_get_object_type_info(metaKey.objecttype);

    if (!info)
    {
        SWSS_LOG_ERROR("invalid object type: %d", metaKey.objecttype);

        return SAI_STATUS_FAILURE;
    }

    if (info->isobjectid)
    {
        return remove(metaKey.objecttype, metaKey.objectkey.key.object_id);
    }

    switch ((int)info->objecttype)
    {
        case SAI_OBJECT_TYPE_FDB_ENTRY:
            return remove(&metaKey.objectkey.key.fdb_entry);

        case SAI_OBJECT_TYPE_ROUTE_ENTRY:
            return remove(&metaKey.objectkey.key.route_entry);

        case SAI_OBJECT_TYPE_NEIGHBOR_ENTRY:
            return remove(&metaKey.objectkey.key.neighbor_entry);

        case SAI_OBJECT_TYPE_NAT_ENTRY:
            return remove(&metaKey.objectkey.key.nat_entry);

        case SAI_OBJECT_TYPE_INSEG_ENTRY:
            return remove(&metaKey.objectkey.key.inseg_entry);

        case SAI_OBJECT_TYPE_MY_SID_ENTRY:
            return remove(&metaKey.objectkey.key.my_sid_entry);

        case SAI_OBJECT_TYPE_DIRECTION_LOOKUP_ENTRY:
            return remove(&metaKey.objectkey.key.direction_lookup_entry);

        case SAI_OBJECT_TYPE_ENI_ETHER_ADDRESS_MAP_ENTRY:
            return remove(&metaKey.objectkey.key.eni_ether_address_map_entry);

        case SAI_OBJECT_TYPE_VIP_ENTRY:
            return remove(&metaKey.objectkey.key.vip_entry);

        case SAI_OBJECT_TYPE_INBOUND_ROUTING_ENTRY:
            return remove(&metaKey.objectkey.key.inbound_routing_entry);

        case SAI_OBJECT_TYPE_PA_VALIDATION_ENTRY:
            return remove(&metaKey.objectkey.key.pa_validation_entry);

        case SAI_OBJECT_TYPE_OUTBOUND_ROUTING_ENTRY:
            return remove(&metaKey.objectkey.key.outbound_routing_entry);

        case SAI_OBJECT_TYPE_OUTBOUND_CA_TO_PA_ENTRY:
            return remove(&metaKey.objectkey.key.outbound_ca_to_pa_entry);

        default:

            SWSS_LOG_ERROR("object type %s not implemented, FIXME", info->objecttypename);

            return SAI_STATUS_FAILURE;
    }
}

sai_status_t SaiInterface::set(
        _In_ const sai_object_meta_key_t& metaKey,
        _In_ const sai_attribute_t *attr)
{
    SWSS_LOG_ENTER();

    auto info = sai_metadata_get_object_type_info(metaKey.objecttype);

    if (!info)
    {
        SWSS_LOG_ERROR("invalid object type: %d", metaKey.objecttype);

        return SAI_STATUS_FAILURE;
    }

    if (info->isobjectid)
    {
        return set(metaKey.objecttype, metaKey.objectkey.key.object_id, attr);
    }

    switch ((int)info->objecttype)
    {
        case SAI_OBJECT_TYPE_FDB_ENTRY:
            return set(&metaKey.objectkey.key.fdb_entry, attr);

        case SAI_OBJECT_TYPE_ROUTE_ENTRY:
            return set(&metaKey.objectkey.key.route_entry, attr);

        case SAI_OBJECT_TYPE_NEIGHBOR_ENTRY:
            return set(&metaKey.objectkey.key.neighbor_entry, attr);

        case SAI_OBJECT_TYPE_NAT_ENTRY:
            return set(&metaKey.objectkey.key.nat_entry, attr);

        case SAI_OBJECT_TYPE_INSEG_ENTRY:
            return set(&metaKey.objectkey.key.inseg_entry, attr);

        case SAI_OBJECT_TYPE_MY_SID_ENTRY:
            return set(&metaKey.objectkey.key.my_sid_entry, attr);

        case SAI_OBJECT_TYPE_DIRECTION_LOOKUP_ENTRY:
            return set(&metaKey.objectkey.key.direction_lookup_entry, attr);

        case SAI_OBJECT_TYPE_ENI_ETHER_ADDRESS_MAP_ENTRY:
            return set(&metaKey.objectkey.key.eni_ether_address_map_entry, attr);

        case SAI_OBJECT_TYPE_VIP_ENTRY:
            return set(&metaKey.objectkey.key.vip_entry, attr);

        case SAI_OBJECT_TYPE_INBOUND_ROUTING_ENTRY:
            return set(&metaKey.objectkey.key.inbound_routing_entry, attr);

        case SAI_OBJECT_TYPE_PA_VALIDATION_ENTRY:
            return set(&metaKey.objectkey.key.pa_validation_entry, attr);

        case SAI_OBJECT_TYPE_OUTBOUND_ROUTING_ENTRY:
            return set(&metaKey.objectkey.key.outbound_routing_entry, attr);

        case SAI_OBJECT_TYPE_OUTBOUND_CA_TO_PA_ENTRY:
            return set(&metaKey.objectkey.key.outbound_ca_to_pa_entry, attr);

        default:

            SWSS_LOG_ERROR("object type %s not implemented, FIXME", info->objecttypename);

            return SAI_STATUS_FAILURE;
    }
}

sai_status_t SaiInterface::get(
        _In_ const sai_object_meta_key_t& metaKey,
        _In_ uint32_t attr_count,
        _Inout_ sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    auto info = sai_metadata_get_object_type_info(metaKey.objecttype);

    if (!info)
    {
        SWSS_LOG_ERROR("invalid object type: %d", metaKey.objecttype);

        return SAI_STATUS_FAILURE;
    }

    if (info->isobjectid)
    {
        return get(metaKey.objecttype, metaKey.objectkey.key.object_id, attr_count, attr_list);
    }

    switch ((int)info->objecttype)
    {
        case SAI_OBJECT_TYPE_FDB_ENTRY:
            return get(&metaKey.objectkey.key.fdb_entry, attr_count, attr_list);

        case SAI_OBJECT_TYPE_ROUTE_ENTRY:
            return get(&metaKey.objectkey.key.route_entry, attr_count, attr_list);

        case SAI_OBJECT_TYPE_NEIGHBOR_ENTRY:
            return get(&metaKey.objectkey.key.neighbor_entry, attr_count, attr_list);

        case SAI_OBJECT_TYPE_NAT_ENTRY:
            return get(&metaKey.objectkey.key.nat_entry, attr_count, attr_list);

        case SAI_OBJECT_TYPE_INSEG_ENTRY:
            return get(&metaKey.objectkey.key.inseg_entry, attr_count, attr_list);

        case SAI_OBJECT_TYPE_MY_SID_ENTRY:
            return get(&metaKey.objectkey.key.my_sid_entry, attr_count, attr_list);

        case SAI_OBJECT_TYPE_DIRECTION_LOOKUP_ENTRY:
            return get(&metaKey.objectkey.key.direction_lookup_entry, attr_count, attr_list);

        case SAI_OBJECT_TYPE_ENI_ETHER_ADDRESS_MAP_ENTRY:
            return get(&metaKey.objectkey.key.eni_ether_address_map_entry, attr_count, attr_list);

        case SAI_OBJECT_TYPE_VIP_ENTRY:
            return get(&metaKey.objectkey.key.vip_entry, attr_count, attr_list);

        case SAI_OBJECT_TYPE_INBOUND_ROUTING_ENTRY:
            return get(&metaKey.objectkey.key.inbound_routing_entry, attr_count, attr_list);

        case SAI_OBJECT_TYPE_PA_VALIDATION_ENTRY:
            return get(&metaKey.objectkey.key.pa_validation_entry, attr_count, attr_list);

        case SAI_OBJECT_TYPE_OUTBOUND_ROUTING_ENTRY:
            return get(&metaKey.objectkey.key.outbound_routing_entry, attr_count, attr_list);

        case SAI_OBJECT_TYPE_OUTBOUND_CA_TO_PA_ENTRY:
            return get(&metaKey.objectkey.key.outbound_ca_to_pa_entry, attr_count, attr_list);

        default:

            SWSS_LOG_ERROR("object type %s not implemented, FIXME", info->objecttypename);

            return SAI_STATUS_FAILURE;
    }
}

sai_status_t SaiInterface::switchMdioRead(
        _In_ sai_object_id_t switch_id,
        _In_ uint32_t device_addr,
        _In_ uint32_t start_reg_addr,
        _In_ uint32_t number_of_registers,
        _Out_ uint32_t *reg_val)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_FAILURE;
}

sai_status_t SaiInterface::switchMdioWrite(
        _In_ sai_object_id_t switch_id,
        _In_ uint32_t device_addr,
        _In_ uint32_t start_reg_addr,
        _In_ uint32_t number_of_registers,
        _In_ const uint32_t *reg_val)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_FAILURE;
}

sai_status_t SaiInterface::switchMdioCl22Read(
        _In_ sai_object_id_t switch_id,
        _In_ uint32_t device_addr,
        _In_ uint32_t start_reg_addr,
        _In_ uint32_t number_of_registers,
        _Out_ uint32_t *reg_val)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_FAILURE;
}

sai_status_t SaiInterface::switchMdioCl22Write(
        _In_ sai_object_id_t switch_id,
        _In_ uint32_t device_addr,
        _In_ uint32_t start_reg_addr,
        _In_ uint32_t number_of_registers,
        _In_ const uint32_t *reg_val)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_FAILURE;
}

sai_log_level_t SaiInterface::logGet(
        _In_ sai_api_t api)
{
    SWSS_LOG_ENTER();

    // default for all apis

    return SAI_LOG_LEVEL_NOTICE;
}

sai_status_t SaiInterface::getStats(
        _In_ const sai_meter_bucket_entry_t* entry,
        _In_ uint32_t number_of_counters,
        _In_ const sai_stat_id_t *counter_ids,
        _Out_ uint64_t *counters)
{
    SWSS_LOG_ENTER();

    SWSS_LOG_ERROR("not implemented");

    return SAI_STATUS_NOT_IMPLEMENTED;
}

sai_status_t SaiInterface::getStatsExt(
        _In_ const sai_meter_bucket_entry_t* entry,
        _In_ uint32_t number_of_counters,
        _In_ const sai_stat_id_t *counter_ids,
        _In_ sai_stats_mode_t mode,
        _Out_ uint64_t *counters)
{
    SWSS_LOG_ENTER();

    SWSS_LOG_ERROR("not implemented");

    return SAI_STATUS_NOT_IMPLEMENTED;
}

sai_status_t SaiInterface::clearStats(
        _In_ const sai_meter_bucket_entry_t* entry,
        _In_ uint32_t number_of_counters,
        _In_ const sai_stat_id_t *counter_ids)
{
    SWSS_LOG_ENTER();

    SWSS_LOG_ERROR("not implemented");

    return SAI_STATUS_NOT_IMPLEMENTED;
}
