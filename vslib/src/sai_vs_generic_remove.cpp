#include "sai_vs.h"
#include "sai_vs_state.h"
#include "sai_vs_switch_BCM56850.h"
#include "sai_vs_switch_MLNX2700.h"

sai_status_t internal_vs_generic_remove(
        _In_ sai_object_type_t object_type,
        _In_ const std::string &serialized_object_id,
        _In_ sai_object_id_t switch_id)
{
    SWSS_LOG_ENTER();

    auto &objectHash = g_switch_state_map.at(switch_id)->objectHash.at(object_type);

    auto it = objectHash.find(serialized_object_id);

    if (it == objectHash.end())
    {
        SWSS_LOG_ERROR("not found %s:%s",
                sai_serialize_object_type(object_type).c_str(),
                serialized_object_id.c_str());

        return SAI_STATUS_ITEM_NOT_FOUND;
    }

    objectHash.erase(it);

    return SAI_STATUS_SUCCESS;
}

sai_status_t vs_generic_remove(
        _In_ sai_object_type_t object_type,
        _In_ sai_object_id_t object_id)
{
    SWSS_LOG_ENTER();

    std::string str_object_id = sai_serialize_object_id(object_id);

    sai_object_id_t switch_id = sai_switch_id_query(object_id);

    sai_status_t status = internal_vs_generic_remove(
            object_type,
            str_object_id,
            switch_id);

    if (object_type == SAI_OBJECT_TYPE_SWITCH &&
            status == SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_NOTICE("removed switch: %s", sai_serialize_object_id(object_id).c_str());

        vs_free_real_object_id(object_id);

        switch (g_vs_switch_type)
        {
            case SAI_VS_SWITCH_TYPE_BCM56850:
                uninit_switch_BCM56850(object_id);
                break;

            case SAI_VS_SWITCH_TYPE_MLNX2700:
                uninit_switch_MLNX2700(object_id);
                break;

            default:
                SWSS_LOG_WARN("unknown switch type: %d", g_vs_switch_type);
                break;
        }
    }

    return status;
}

#define VS_ENTRY_REMOVE(OT,ot)                              \
    sai_status_t vs_generic_remove_ ## ot(                  \
            _In_ const sai_ ## ot ## _t *entry)             \
        {                                                   \
            SWSS_LOG_ENTER();                               \
            std::string str = sai_serialize_ ## ot(*entry); \
            return internal_vs_generic_remove(              \
                    SAI_OBJECT_TYPE_ ## OT,                 \
                    str,                                    \
                    entry->switch_id);                      \
        }

VS_ENTRY_REMOVE(FDB_ENTRY,fdb_entry);
VS_ENTRY_REMOVE(INSEG_ENTRY,inseg_entry);
VS_ENTRY_REMOVE(IPMC_ENTRY,ipmc_entry);
VS_ENTRY_REMOVE(L2MC_ENTRY,l2mc_entry);
VS_ENTRY_REMOVE(MCAST_FDB_ENTRY,mcast_fdb_entry);
VS_ENTRY_REMOVE(NEIGHBOR_ENTRY,neighbor_entry);
VS_ENTRY_REMOVE(ROUTE_ENTRY,route_entry);
