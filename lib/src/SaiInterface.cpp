#include "SaiInterface.h"

#include "swss/logger.h"

#include <vector>

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

    switch (info->objecttype)
    {
        case SAI_OBJECT_TYPE_FDB_ENTRY:
            return create(&metaKey.objectkey.key.fdb_entry, attr_count, attr_list);

        case SAI_OBJECT_TYPE_ROUTE_ENTRY:
            return create(&metaKey.objectkey.key.route_entry, attr_count, attr_list);

        case SAI_OBJECT_TYPE_NEIGHBOR_ENTRY:
            return create(&metaKey.objectkey.key.neighbor_entry, attr_count, attr_list);

        case SAI_OBJECT_TYPE_NAT_ENTRY:
            return create(&metaKey.objectkey.key.nat_entry, attr_count, attr_list);

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

    switch (info->objecttype)
    {
        case SAI_OBJECT_TYPE_FDB_ENTRY:
            return remove(&metaKey.objectkey.key.fdb_entry);

        case SAI_OBJECT_TYPE_ROUTE_ENTRY:
            return remove(&metaKey.objectkey.key.route_entry);

        case SAI_OBJECT_TYPE_NEIGHBOR_ENTRY:
            return remove(&metaKey.objectkey.key.neighbor_entry);

        case SAI_OBJECT_TYPE_NAT_ENTRY:
            return remove(&metaKey.objectkey.key.nat_entry);

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

    switch (info->objecttype)
    {
        case SAI_OBJECT_TYPE_FDB_ENTRY:
            return set(&metaKey.objectkey.key.fdb_entry, attr);

        case SAI_OBJECT_TYPE_ROUTE_ENTRY:
            return set(&metaKey.objectkey.key.route_entry, attr);

        case SAI_OBJECT_TYPE_NEIGHBOR_ENTRY:
            return set(&metaKey.objectkey.key.neighbor_entry, attr);

        case SAI_OBJECT_TYPE_NAT_ENTRY:
            return set(&metaKey.objectkey.key.nat_entry, attr);

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

    switch (info->objecttype)
    {
        case SAI_OBJECT_TYPE_FDB_ENTRY:
            return get(&metaKey.objectkey.key.fdb_entry, attr_count, attr_list);

        case SAI_OBJECT_TYPE_ROUTE_ENTRY:
            return get(&metaKey.objectkey.key.route_entry, attr_count, attr_list);

        case SAI_OBJECT_TYPE_NEIGHBOR_ENTRY:
            return get(&metaKey.objectkey.key.neighbor_entry, attr_count, attr_list);

        case SAI_OBJECT_TYPE_NAT_ENTRY:
            return get(&metaKey.objectkey.key.nat_entry, attr_count, attr_list);

        default:

            SWSS_LOG_ERROR("object type %s not implemented, FIXME", info->objecttypename);

            return SAI_STATUS_FAILURE;
    }
}

sai_status_t SaiInterface::bulkCreate(
        _Inout_ sai_object_meta_key_t* metaKey,
        _In_ sai_object_id_t switch_id,
        _In_ uint32_t object_count,
        _In_ uint32_t *attr_count,
        _In_ const sai_attribute_t **attr_list,
        _In_ sai_bulk_op_error_mode_t mode,
        _Out_ sai_status_t *object_statuses)
{
    SWSS_LOG_ENTER();

    if (!metaKey)
    {
        return SAI_STATUS_FAILURE;
    }

    if (!object_count)
    {
        SWSS_LOG_WARN("Attributes array is empty in bulkCreate");
        return SAI_STATUS_SUCCESS;
    }

    auto info = sai_metadata_get_object_type_info(metaKey[0].objecttype);

    if (info->isobjectid)
    {
        return SAI_STATUS_NOT_SUPPORTED;
    }

    switch (info->objecttype)
    {
        case SAI_OBJECT_TYPE_FDB_ENTRY: {
            std::vector<sai_fdb_entry_t> entries;
            for (size_t idx = 0; idx < object_count; idx++) {
                entries.push_back(metaKey[idx].objectkey.key.fdb_entry);
            }
            return bulkCreate(object_count, &entries.front(), attr_count, attr_list, mode, object_statuses);
        }

        case SAI_OBJECT_TYPE_ROUTE_ENTRY: {
            std::vector<sai_route_entry_t> entries;
            for (size_t idx = 0; idx < object_count; idx++) {
                entries.push_back(metaKey[idx].objectkey.key.route_entry);
            }
            return bulkCreate(object_count, &entries.front(), attr_count, attr_list, mode, object_statuses);
        }

        default:
            SWSS_LOG_ERROR("object type %s not implemented, FIXME", info->objecttypename);
            return SAI_STATUS_NOT_SUPPORTED;
    }
}

sai_status_t SaiInterface::bulkRemove(
        _Inout_ sai_object_meta_key_t* metaKey,
        _In_ uint32_t object_count,
        _In_ sai_bulk_op_error_mode_t mode,
        _Out_ sai_status_t *object_statuses)
{
    SWSS_LOG_ENTER();

    if (!metaKey)
    {
        return SAI_STATUS_FAILURE;
    }

    if (!object_count)
    {
        SWSS_LOG_WARN("Attributes array is empty in bulkRemove");
        return SAI_STATUS_SUCCESS;
    }

    auto info = sai_metadata_get_object_type_info(metaKey[0].objecttype);

    if (info->isobjectid)
    {
        return SAI_STATUS_NOT_SUPPORTED;
    }

    switch (info->objecttype)
    {
        case SAI_OBJECT_TYPE_FDB_ENTRY: {
            std::vector<sai_fdb_entry_t> entries(object_count);
            for (size_t idx = 0; idx < object_count; idx++) {
                entries.push_back(metaKey[idx].objectkey.key.fdb_entry);
            }
            return bulkRemove(object_count, &entries.front(), mode, object_statuses);
        }

        case SAI_OBJECT_TYPE_ROUTE_ENTRY: {
            std::vector<sai_route_entry_t> entries(object_count);
            for (size_t idx = 0; idx < object_count; idx++) {
                entries.push_back(metaKey[idx].objectkey.key.route_entry);
            }
            return bulkRemove(object_count, &entries.front(), mode, object_statuses);
        }

        default:
            SWSS_LOG_ERROR("object type %s not implemented, FIXME", info->objecttypename);
            return SAI_STATUS_NOT_SUPPORTED;
    }
}

sai_status_t SaiInterface::bulkSet(
        _Inout_ sai_object_meta_key_t* metaKey,
        _In_ uint32_t object_count,
        _In_ const sai_attribute_t *attr_list,
        _In_ sai_bulk_op_error_mode_t mode,
        _Out_ sai_status_t *object_statuses)
{
    SWSS_LOG_ENTER();

    if (!metaKey)
    {
        return SAI_STATUS_FAILURE;
    }

    if (!object_count)
    {
        SWSS_LOG_WARN("Attributes array is empty in bulkSet");
        return SAI_STATUS_SUCCESS;
    }

    auto info = sai_metadata_get_object_type_info(metaKey[0].objecttype);

    if (info->isobjectid)
    {
        return SAI_STATUS_NOT_SUPPORTED;
    }

    switch (info->objecttype)
    {
        case SAI_OBJECT_TYPE_FDB_ENTRY: {
            std::vector<sai_fdb_entry_t> entries(object_count);
            for (size_t idx = 0; idx < object_count; idx++) {
                entries.push_back(metaKey[idx].objectkey.key.fdb_entry);
            }
            return bulkSet(object_count, &entries.front(), attr_list, mode, object_statuses);
        }

        case SAI_OBJECT_TYPE_ROUTE_ENTRY: {
            std::vector<sai_route_entry_t> entries(object_count);
            for (size_t idx = 0; idx < object_count; idx++) {
                entries.push_back(metaKey[idx].objectkey.key.route_entry);
            }
            return bulkSet(object_count, &entries.front(), attr_list, mode, object_statuses);
        }

        default:
            SWSS_LOG_ERROR("object type %s not implemented, FIXME", info->objecttypename);
            return SAI_STATUS_NOT_SUPPORTED;
    }
}
