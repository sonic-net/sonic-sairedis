#include "sai_vs.h"
#include "sai_vs_state.h"
#include "sai_vs_switch_BCM56850.h"
#include "sai_vs_switch_MLNX2700.h"

sai_status_t refresh_read_only(
        _In_ const sai_attr_metadata_t *meta,
        _In_ sai_object_id_t object_id,
        _In_ sai_object_id_t switch_id)
{
    SWSS_LOG_ENTER();

    /*
     * Read only, must be per switch since maybe different implemented,
     * different order maybe on the queues.
     */

    switch (g_vs_switch_type)
    {
        case SAI_VS_SWITCH_TYPE_BCM56850:
            return refresh_read_only_BCM56850(meta, object_id, switch_id);

        case SAI_VS_SWITCH_TYPE_MLNX2700:
            return refresh_read_only_MLNX2700(meta, object_id, switch_id);

        default:
            break;
    }

    SWSS_LOG_WARN("need to recalculate RO: %s", meta->attridname);

    return SAI_STATUS_NOT_IMPLEMENTED;
}

sai_status_t internal_vs_generic_get(
        _In_ sai_object_type_t object_type,
        _In_ const std::string &serialized_object_id,
        _In_ sai_object_id_t switch_id,
        _In_ uint32_t attr_count,
        _Out_ sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    if (g_switch_state_map.find(switch_id) == g_switch_state_map.end())
    {
        SWSS_LOG_ERROR("failed to find switch %s in switch state map", sai_serialize_object_id(switch_id).c_str());

        return SAI_STATUS_FAILURE;
    }

    auto &objectHash = g_switch_state_map.at(switch_id)->objectHash.at(object_type);

    auto it = objectHash.find(serialized_object_id);

    if (it == objectHash.end())
    {
        SWSS_LOG_ERROR("not found %s:%s",
                sai_serialize_object_type(object_type).c_str(),
                serialized_object_id.c_str());

        return SAI_STATUS_ITEM_NOT_FOUND;
    }

    /*
     * We need reference here since we can potentially update attr hash for RO
     * object.
     */

    AttrHash& attrHash = it->second;

    /*
     * Some of the list query maybe for length, so we can't do
     * normal serialize, maybe with count only.
     */

    sai_status_t final_status = SAI_STATUS_SUCCESS;

    for (uint32_t idx = 0; idx < attr_count; ++idx)
    {
        sai_attr_id_t id = attr_list[idx].id;

        auto meta = sai_metadata_get_attr_metadata(object_type, id);

        if (meta == NULL)
        {
            SWSS_LOG_ERROR("failed to find attribute %d for %s:%s", id,
                    sai_serialize_object_type(object_type).c_str(),
                    serialized_object_id.c_str());

            return SAI_STATUS_FAILURE;
        }

        sai_status_t status;

        if (SAI_HAS_FLAG_READ_ONLY(meta->flags))
        {
            /*
             * Read only attributes may require recalculation.
             * Metadata makes sure that non object id's can't have
             * read only attributes. So here is definitely OID.
             */

            sai_object_id_t oid;
            sai_deserialize_object_id(serialized_object_id, oid);

            status = refresh_read_only(meta, oid, switch_id);

            if (status != SAI_STATUS_SUCCESS)
            {
                SWSS_LOG_ERROR("%s read only not implemented on %s",
                        meta->attridname,
                        serialized_object_id.c_str());

                return status;
            }
        }

        auto ait = attrHash.find(meta->attridname);

        if (ait == attrHash.end())
        {
            SWSS_LOG_ERROR("%s not implemented on %s",
                    meta->attridname,
                    serialized_object_id.c_str());

            return SAI_STATUS_NOT_IMPLEMENTED;
        }

        auto attr = ait->second->getAttr();

        status = transfer_attributes(object_type, 1, attr, &attr_list[idx], false);

        if (status == SAI_STATUS_BUFFER_OVERFLOW)
        {
            /*
             * This is considered partial success, since we get correct list
             * length.  Note that other items ARE processes on the list.
             */

            SWSS_LOG_NOTICE("BUFFER_OVERFLOW %s: %s",
                    serialized_object_id.c_str(),
                    meta->attridname);

            /*
             * We still continue processing other attributes for get as long as
             * we only will be getting buffer overflow error.
             */

            final_status = status;
            continue;
        }

        if (status != SAI_STATUS_SUCCESS)
        {
            // all other errors

            SWSS_LOG_ERROR("get failed %s: %s: %s",
                    serialized_object_id.c_str(),
                    meta->attridname,
                    sai_serialize_status(status).c_str());

            return status;
        }
    }

    return final_status;
}

sai_status_t vs_generic_get(
        _In_ sai_object_type_t object_type,
        _In_ sai_object_id_t object_id,
        _In_ uint32_t attr_count,
        _Out_ sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    std::string str_object_id = sai_serialize_object_id(object_id);

    sai_object_id_t switch_id = sai_switch_id_query(object_id);

    return internal_vs_generic_get(
            object_type,
            str_object_id,
            switch_id,
            attr_count,
            attr_list);
}

#define VS_ENTRY_GET(OT,ot)                             \
sai_status_t vs_generic_get_ ## ot(                     \
        _In_ const sai_ ## ot ## _t *entry,             \
        _In_ uint32_t attr_count,                       \
        _Out_ sai_attribute_t *attr_list)               \
{                                                       \
    SWSS_LOG_ENTER();                                   \
    std::string str = sai_serialize_ ## ot(*entry);     \
    return internal_vs_generic_get(                     \
            SAI_OBJECT_TYPE_ ## OT,                     \
            str,                                        \
            entry->switch_id,                           \
            attr_count,                                 \
            attr_list);                                 \
}

VS_ENTRY_GET(FDB_ENTRY,fdb_entry);
VS_ENTRY_GET(INSEG_ENTRY,inseg_entry);
VS_ENTRY_GET(IPMC_ENTRY,ipmc_entry);
VS_ENTRY_GET(L2MC_ENTRY,l2mc_entry);
VS_ENTRY_GET(MCAST_FDB_ENTRY,mcast_fdb_entry);
VS_ENTRY_GET(NEIGHBOR_ENTRY,neighbor_entry);
VS_ENTRY_GET(ROUTE_ENTRY,route_entry);
