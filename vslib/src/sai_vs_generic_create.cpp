#include "sai_vs.h"
#include "sai_vs_state.h"
#include "sai_vs_switch_BCM56850.h"
#include "sai_vs_switch_MLNX2700.h"

SwitchStateMap g_switch_state_map;

#define MAX_SWITCHES 0x100
#define OT_POSITION 32
#define SWID_POSITION 40

/*
 * TODO those real id's should also be per switch index
 */

uint64_t real_ids[SAI_OBJECT_TYPE_MAX];

void vs_reset_id_counter()
{
    SWSS_LOG_ENTER();

    memset(real_ids, 0, sizeof(real_ids));
}

bool switch_ids[MAX_SWITCHES] = {};

void vs_clear_switch_ids()
{
    SWSS_LOG_ENTER();

    for (int idx = 0; idx < MAX_SWITCHES; ++idx)
    {
        switch_ids[idx] = false;
    }
}

int vs_get_free_switch_id_index()
{
    SWSS_LOG_ENTER();

    for (int index = 0; index < MAX_SWITCHES; ++index)
    {
        if (!switch_ids[index])
        {
            switch_ids[index] = true;

            SWSS_LOG_NOTICE("got new switch index 0x%x", index);

            return index;
        }
    }

    SWSS_LOG_THROW("no more available switch id indexes");
}

/*
 * NOTE: Need to be executed when removing switch.
 */

void vs_free_switch_id_index(int index)
{
    SWSS_LOG_ENTER();

    if (index < 0 || index >= MAX_SWITCHES)
    {
        SWSS_LOG_THROW("switch index is invalid 0x%x", index);
    }
    else
    {
        switch_ids[index] = false;

        SWSS_LOG_DEBUG("marked switch index 0x%x as unused", index);
    }
}

sai_object_id_t vs_construct_object_id(
        _In_ sai_object_type_t object_type,
        _In_ int switch_index,
        _In_ uint64_t real_id)
{
    SWSS_LOG_ENTER();

    return (sai_object_id_t)(((uint64_t)switch_index << SWID_POSITION) | ((uint64_t)object_type << OT_POSITION) | real_id);
}

sai_object_id_t vs_create_switch_real_object_id()
{
    SWSS_LOG_ENTER();

    /*
     * NOTE: Switch ids are deterministic.
     */

    int index = vs_get_free_switch_id_index();

    return vs_construct_object_id(SAI_OBJECT_TYPE_SWITCH, index, index);
}

sai_object_type_t sai_object_type_query(
        _In_ sai_object_id_t object_id)
{
    SWSS_LOG_ENTER();

    if (object_id == SAI_NULL_OBJECT_ID)
    {
        return SAI_OBJECT_TYPE_NULL;
    }

    sai_object_type_t ot = (sai_object_type_t)((object_id >> OT_POSITION) & 0xFF);

    if (ot == SAI_OBJECT_TYPE_NULL || ot >= SAI_OBJECT_TYPE_MAX)
    {
        SWSS_LOG_ERROR("invalid object id 0x%lx", object_id);

        /*
         * We can't throw here since it would not give meaningfull message.
         * Tthrowing at one level up is better.
         */

        return SAI_OBJECT_TYPE_NULL;
    }

    return ot;
}

sai_object_id_t sai_switch_id_query(
        _In_ sai_object_id_t oid)
{
    SWSS_LOG_ENTER();

    if (oid == SAI_NULL_OBJECT_ID)
    {
        return oid;
    }

    sai_object_type_t object_type = sai_object_type_query(oid);

    if (object_type == SAI_OBJECT_TYPE_NULL)
    {
        SWSS_LOG_THROW("invalid object type of oid 0x%lx", oid);
    }

    if (object_type == SAI_OBJECT_TYPE_SWITCH)
    {
        return oid;
    }

    int sw_index = (int)((oid >> SWID_POSITION) & 0xFF);

    sai_object_id_t sw_id = vs_construct_object_id(SAI_OBJECT_TYPE_SWITCH, sw_index, sw_index);

    return sw_id;
}

int vs_get_switch_id_index(
        _In_ sai_object_id_t switch_id)
{
    SWSS_LOG_ENTER();

    sai_object_type_t switch_object_type = sai_object_type_query(switch_id);

    if (switch_object_type == SAI_OBJECT_TYPE_SWITCH)
    {
        return (int)((switch_id >> SWID_POSITION) & 0xFF);
    }

    SWSS_LOG_THROW("object type of switch %s is %s, should be SWITCH",
            sai_serialize_object_id(switch_id).c_str(),
            sai_serialize_object_type(switch_object_type).c_str());
}

sai_object_id_t vs_create_real_object_id(
        _In_ sai_object_type_t object_type,
        _In_ sai_object_id_t switch_id)
{
    SWSS_LOG_ENTER();

    if ((object_type <= SAI_OBJECT_TYPE_NULL) ||
            (object_type >= SAI_OBJECT_TYPE_MAX))
    {
        SWSS_LOG_THROW("invalid objct type: %d", object_type);
    }

    // object_id:
    // bits 63..56 - switch index
    // bits 55..48 - object type
    // bits 47..0  - object id

    if (object_type == SAI_OBJECT_TYPE_SWITCH)
    {
        sai_object_id_t object_id = vs_create_switch_real_object_id();

        SWSS_LOG_NOTICE("created swith RID 0x%lx", object_id);

        return object_id;
    }

    int index = vs_get_switch_id_index(switch_id);

    // count from zero for each type separetly
    uint64_t real_id = real_ids[object_type]++;

    sai_object_id_t object_id = vs_construct_object_id(object_type, index, real_id);

    SWSS_LOG_DEBUG("created RID 0x%lx", object_id);

    return object_id;
}

void vs_update_real_object_ids(
        _In_ const std::shared_ptr<SwitchState> warmBootState)
{
    SWSS_LOG_ENTER();

    /*
     * Since we loaded state from warm boot, we need to update real object id's
     * in case a new object will be created. We need this so new objects will
     * not have the same ID as existing ones.
     */

    for (auto oh: warmBootState->objectHash)
    {
        sai_object_type_t ot = oh.first;

        if (ot == SAI_OBJECT_TYPE_NULL)
            continue;

        auto oi = sai_metadata_get_object_type_info(ot);

        if (oi == NULL)
        {
            SWSS_LOG_THROW("failed to find object type info for object type: %d", ot);
        }

        if (oi->isnonobjectid)
            continue;

        for (auto o: oh.second)
        {
            sai_object_id_t oid;

            sai_deserialize_object_id(o.first, oid);

            // lower 32 bits on VS is real id on specific object type
            uint64_t real_id = oid & ((uint64_t)-1) >> (64 - OT_POSITION);

            if (real_ids[ot] <= real_id)
            {
                real_ids[ot] = real_id + 1; // +1 since this will be next object number
            }

            SWSS_LOG_INFO("update %s:%s real id to from %lu to %lu", oi->objecttypename, o.first.c_str(), real_id, real_ids[ot]);
        }
    }
}

void vs_free_real_object_id(
        _In_ sai_object_id_t object_id)
{
    SWSS_LOG_ENTER();

    if (sai_object_type_query(object_id) == SAI_OBJECT_TYPE_SWITCH)
    {
        vs_free_switch_id_index(vs_get_switch_id_index(object_id));
    }
}

std::shared_ptr<SwitchState> vs_read_switch_database_for_warm_restart(
        _In_ sai_object_id_t switch_id)
{
    SWSS_LOG_ENTER();

    if (g_warm_boot_read_file == NULL)
    {
        SWSS_LOG_ERROR("warm boot read file is NULL");
        return nullptr;
    }

    std::ifstream dumpFile;

    dumpFile.open(g_warm_boot_read_file);

    if (!dumpFile.is_open())
    {
        SWSS_LOG_ERROR("failed to open: %s, switching to cold boot", g_warm_boot_read_file);

        g_vs_boot_type = SAI_VS_COLD_BOOT;

        return nullptr;
    }

    std::shared_ptr<SwitchState> ss = std::make_shared<SwitchState>(switch_id);

    size_t count = 1; // count is 1 since switch_id was inserted to objectHash in SwitchState constructor

    std::string line;
    while (std::getline(dumpFile, line))
    {
        // line format: OBJECT_TYPE OBJECT_ID ATTR_ID ATTR_VALUE
        std::istringstream iss(line);

        std::string str_object_type;
        std::string str_object_id;
        std::string str_attr_id;
        std::string str_attr_value;

        iss >> str_object_type >> str_object_id >> str_attr_id >> str_attr_value;

        sai_object_meta_key_t meta_key;

        sai_deserialize_object_meta_key(str_object_type + ":" + str_object_id, meta_key);

        auto &objectHash = ss->objectHash.at(meta_key.objecttype);

        if (objectHash.find(str_object_id) == objectHash.end())
        {
            count++;

            objectHash[str_object_id] = {};
        }

        if (str_attr_id == "NULL")
        {
            // skip empty attributes
            continue;
        }

        if (meta_key.objecttype == SAI_OBJECT_TYPE_SWITCH)
        {
            if (meta_key.objectkey.key.object_id != switch_id)
            {
                SWSS_LOG_THROW("created switch id is %s but warm boot serialized is %s",
                        sai_serialize_object_id(switch_id).c_str(),
                        str_object_id.c_str());
            }
        }

        auto meta = sai_metadata_get_attr_metadata_by_attr_id_name(str_attr_id.c_str());

        if (meta == NULL)
        {
            SWSS_LOG_THROW("failed to find metadata for %s", str_attr_id.c_str());
        }

        // populate attributes

        sai_attribute_t attr;

        attr.id = meta->attrid;

        sai_deserialize_attr_value(str_attr_value.c_str(), *meta, attr, false);

        auto a = std::make_shared<SaiAttrWrap>(meta_key.objecttype, &attr);

        objectHash[str_object_id][a->getAttrMetadata()->attridname] = a;

        // free possible list attributes
        sai_deserialize_free_attribute_value(meta->attrvaluetype, attr);
    }

    // NOTE notification pointers should be restored by attr_list when creating switch

    dumpFile.close();

    SWSS_LOG_NOTICE("loaded %zu objects from: %s", count, g_warm_boot_read_file);

    return ss;
}

sai_status_t internal_vs_generic_create(
        _In_ sai_object_type_t object_type,
        _In_ const std::string &serialized_object_id,
        _In_ sai_object_id_t switch_id,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    if (object_type == SAI_OBJECT_TYPE_SWITCH)
    {
        std::shared_ptr<SwitchState> warmBootState = nullptr;

        if (g_vs_boot_type == SAI_VS_WARM_BOOT)
        {
            warmBootState = vs_read_switch_database_for_warm_restart(switch_id);
        }

        switch (g_vs_switch_type)
        {
            case SAI_VS_SWITCH_TYPE_BCM56850:
                init_switch_BCM56850(switch_id, warmBootState);
                break;

            case SAI_VS_SWITCH_TYPE_MLNX2700:
                init_switch_MLNX2700(switch_id, warmBootState);
                break;

            default:
                SWSS_LOG_WARN("unknown switch type: %d", g_vs_switch_type);
                return SAI_STATUS_FAILURE;
        }

        if (warmBootState != nullptr)
        {
            vs_update_real_object_ids(warmBootState);
        }
    }

    auto &objectHash = g_switch_state_map.at(switch_id)->objectHash.at(object_type);

    auto it = objectHash.find(serialized_object_id);

    if (object_type != SAI_OBJECT_TYPE_SWITCH)
    {
        /*
         * Switch is special, and object is already created by init.
         *
         * XXX revisit this.
         */

        if (it != objectHash.end())
        {
            SWSS_LOG_ERROR("create failed, object already exists, object type: %s: id: %s",
                    sai_serialize_object_type(object_type).c_str(),
                    serialized_object_id.c_str());

            return SAI_STATUS_ITEM_ALREADY_EXISTS;
        }
    }

    if (objectHash.find(serialized_object_id) == objectHash.end())
    {
        /*
         * Number of attributes may be zero, so see if actual entry was created
         * with empty hash.
         */

        objectHash[serialized_object_id] = {};
    }

    for (uint32_t i = 0; i < attr_count; ++i)
    {
        auto a = std::make_shared<SaiAttrWrap>(object_type, &attr_list[i]);

        objectHash[serialized_object_id][a->getAttrMetadata()->attridname] = a;
    }

    return SAI_STATUS_SUCCESS;
}

sai_status_t vs_generic_create(
        _In_ sai_object_type_t object_type,
        _Out_ sai_object_id_t *object_id,
        _In_ sai_object_id_t switch_id,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    // create new real object ID
    *object_id = vs_create_real_object_id(object_type, switch_id);

    if (object_type == SAI_OBJECT_TYPE_SWITCH)
    {
        switch_id = *object_id;
    }

    std::string str_object_id = sai_serialize_object_id(*object_id);

    return internal_vs_generic_create(
            object_type,
            str_object_id,
            switch_id,
            attr_count,
            attr_list);
}

#define VS_ENTRY_CREATE(OT,ot)                          \
    sai_status_t vs_generic_create_ ## ot(              \
            _In_ const sai_ ## ot ## _t * entry,        \
            _In_ uint32_t attr_count,                   \
            _In_ const sai_attribute_t *attr_list)      \
    {                                                   \
        SWSS_LOG_ENTER();                               \
        std::string str = sai_serialize_ ## ot(*entry); \
        return internal_vs_generic_create(              \
                SAI_OBJECT_TYPE_ ## OT,                 \
                str,                                    \
                entry->switch_id,                       \
                attr_count,                             \
                attr_list);                             \
    }

VS_ENTRY_CREATE(FDB_ENTRY,fdb_entry);
VS_ENTRY_CREATE(INSEG_ENTRY,inseg_entry);
VS_ENTRY_CREATE(IPMC_ENTRY,ipmc_entry);
VS_ENTRY_CREATE(L2MC_ENTRY,l2mc_entry);
VS_ENTRY_CREATE(MCAST_FDB_ENTRY,mcast_fdb_entry);
VS_ENTRY_CREATE(NEIGHBOR_ENTRY,neighbor_entry);
VS_ENTRY_CREATE(ROUTE_ENTRY,route_entry);
