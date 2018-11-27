#include "sai_vs.h"
#include "sai_vs_state.h"
#include "sai_vs_switch_BCM56850.h"
#include "sai_vs_switch_MLNX2700.h"

#include <fstream>

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

void vs_dump_switch_database_for_warm_restart(
        _In_ sai_object_id_t switch_id)
{
    SWSS_LOG_ENTER();

    auto it = g_switch_state_map.find(switch_id);

    if (it == g_switch_state_map.end())
    {
        SWSS_LOG_THROW("switch don't exists 0x%lx", switch_id);
    }

    if (g_warm_boot_write_file == NULL)
    {
        SWSS_LOG_ERROR("warm boot write file is NULL");
        return;
    }

    std::ofstream dumpFile;

    dumpFile.open(g_warm_boot_write_file);

    if (!dumpFile.is_open())
    {
        SWSS_LOG_ERROR("failed to open: %s", g_warm_boot_write_file);
        return;
    }

    auto switchState = it->second;

    auto objectHash = switchState->objectHash;

    // dump all objects and attributes to file

    size_t count = 0;

    for (auto kvp: objectHash)
    {
        auto singleTypeObjectMap = kvp.second;

        count += singleTypeObjectMap.size();

        for (auto o: singleTypeObjectMap)
        {
            // if object don't have attributes, size can be zero
            if (o.second.size() == 0)
            {
                dumpFile << sai_serialize_object_type(kvp.first) << " " << o.first << " NULL NULL" << std::endl;
            }
            else
            {
                for (auto a: o.second)
                {
                    dumpFile << sai_serialize_object_type(kvp.first) << " ";
                    dumpFile << o.first.c_str();
                    dumpFile << " ";
                    dumpFile << a.first.c_str();
                    dumpFile << " ";
                    dumpFile << a.second->getAttrStrValue();
                    dumpFile << std::endl;
                }
            }
        }
    }

    if (g_vs_hostif_use_tap_device)
    {
        /*
         * If user is using tap devices we also need to dump local fdb info
         * data and restore it on warm start.
         */

        for (auto fi: g_fdb_info_set)
        {
            dumpFile << SAI_VS_FDB_INFO << " " << sai_vs_serialize_fdb_info(fi) << std::endl;
        }

        SWSS_LOG_NOTICE("dumped %zu fdb infos", g_fdb_info_set.size());
    }

    dumpFile.close();

    SWSS_LOG_NOTICE("dumped %zu objects to %s", count, g_warm_boot_write_file);
}

sai_status_t vs_generic_remove(
        _In_ sai_object_type_t object_type,
        _In_ sai_object_id_t object_id)
{
    SWSS_LOG_ENTER();

    std::string str_object_id = sai_serialize_object_id(object_id);

    sai_object_id_t switch_id = sai_switch_id_query(object_id);

    /*
     * Perform db dump if warm restart was requested.
     */

    if (object_type == SAI_OBJECT_TYPE_SWITCH)
    {
        sai_attribute_t attr;

        attr.id = SAI_SWITCH_ATTR_RESTART_WARM;

        if (vs_generic_get(object_type, object_id, 1, &attr) == SAI_STATUS_SUCCESS)
        {
            SWSS_LOG_NOTICE("SAI_SWITCH_ATTR_RESTART_WARM = %s", attr.value.booldata ? "true" : "false");

            if (attr.value.booldata)
            {
                vs_dump_switch_database_for_warm_restart(object_id);
            }
        }
        else
        {
            SWSS_LOG_ERROR("failed to get SAI_SWITCH_ATTR_RESTART_WARM, no DB dump will be performed");
        }
    }

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
