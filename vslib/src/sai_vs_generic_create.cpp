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

void vs_free_real_object_id(
        _In_ sai_object_id_t object_id)
{
    SWSS_LOG_ENTER();

    if (sai_object_type_query(object_id) == SAI_OBJECT_TYPE_SWITCH)
    {
        vs_free_switch_id_index(vs_get_switch_id_index(object_id));
    }
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
        switch (g_vs_switch_type)
        {
            case SAI_VS_SWITCH_TYPE_BCM56850:
                init_switch_BCM56850(switch_id);
                break;

            case SAI_VS_SWITCH_TYPE_MLNX2700:
                init_switch_MLNX2700(switch_id);
                break;

            default:
                SWSS_LOG_WARN("unknown switch type: %d", g_vs_switch_type);
                return SAI_STATUS_FAILURE;
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
            SWSS_LOG_ERROR("create failed, object already exists %s:%, object type: %s: id: %s",
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

sai_status_t vs_generic_create_fdb_entry(
        _In_ const sai_fdb_entry_t *fdb_entry,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    std::string str_fdb_entry = sai_serialize_fdb_entry(*fdb_entry);

    return internal_vs_generic_create(
            SAI_OBJECT_TYPE_FDB_ENTRY,
            str_fdb_entry,
            fdb_entry->switch_id,
            attr_count,
            attr_list);
}

sai_status_t vs_generic_create_neighbor_entry(
        _In_ const sai_neighbor_entry_t *neighbor_entry,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    std::string str_neighbor_entry = sai_serialize_neighbor_entry(*neighbor_entry);

    return internal_vs_generic_create(
            SAI_OBJECT_TYPE_NEIGHBOR_ENTRY,
            str_neighbor_entry,
            neighbor_entry->switch_id,
            attr_count,
            attr_list);
}

sai_status_t vs_generic_create_route_entry(
        _In_ const sai_route_entry_t *route_entry,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    std::string str_route_entry = sai_serialize_route_entry(*route_entry);

    return internal_vs_generic_create(
            SAI_OBJECT_TYPE_ROUTE_ENTRY,
            str_route_entry,
            route_entry->switch_id,
            attr_count,
            attr_list);
}
