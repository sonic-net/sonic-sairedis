#include "sai_vs.h"
#include "sai_vs_state.h"

sai_status_t internal_vs_generic_set(
        _In_ sai_object_type_t object_type,
        _In_ const std::string &serialized_object_id,
        _In_ sai_object_id_t switch_id,
        _In_ const sai_attribute_t *attr)
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

    AttrHash &attrHash = it->second;

    auto a = std::make_shared<SaiAttrWrap>(object_type, attr);

    // set have only one attribute
    attrHash[a->getAttrMetadata()->attridname] = a;

    return SAI_STATUS_SUCCESS;
}

sai_status_t vs_generic_set(
        _In_ sai_object_type_t object_type,
        _In_ sai_object_id_t object_id,
        _In_ const sai_attribute_t *attr)
{
    SWSS_LOG_ENTER();

    std::string str_object_id = sai_serialize_object_id(object_id);

    sai_object_id_t switch_id = sai_switch_id_query(object_id);

    return internal_vs_generic_set(
            object_type,
            str_object_id,
            switch_id,
            attr);
}

#define VS_ENTRY_SET(OT,ot)                             \
sai_status_t vs_generic_set_ ## ot(                     \
        _In_ const sai_ ## ot ## _t *entry,             \
        _In_ const sai_attribute_t *attr)               \
{                                                       \
    SWSS_LOG_ENTER();                                   \
    std::string str = sai_serialize_ ## ot(*entry);     \
    return internal_vs_generic_set(                     \
            SAI_OBJECT_TYPE_ ## OT,                     \
            str,                                        \
            entry->switch_id,                           \
            attr);                                      \
}

VS_ENTRY_SET(FDB_ENTRY,fdb_entry);
VS_ENTRY_SET(INSEG_ENTRY,inseg_entry);
VS_ENTRY_SET(IPMC_ENTRY,ipmc_entry);
VS_ENTRY_SET(L2MC_ENTRY,l2mc_entry);
VS_ENTRY_SET(MCAST_FDB_ENTRY,mcast_fdb_entry);
VS_ENTRY_SET(NEIGHBOR_ENTRY,neighbor_entry);
VS_ENTRY_SET(ROUTE_ENTRY,route_entry);
VS_ENTRY_SET(NAT_ENTRY, nat_entry);
