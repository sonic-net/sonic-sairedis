#include "sai_redis.h"
#include "meta/saiserialize.h"
#include "meta/saiattributelist.h"

bool switch_ids[MAX_SWITCHES] = {};

void redis_clear_switch_ids()
{
    SWSS_LOG_ENTER();

    for (int idx = 0; idx < MAX_SWITCHES; ++idx)
    {
        switch_ids[idx] = false;
    }
}

int redis_get_free_switch_id_index()
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

void redis_free_switch_id_index(int index)
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

sai_object_id_t redis_construct_object_id(
        _In_ sai_object_type_t object_type,
        _In_ int switch_index,
        _In_ uint64_t virtual_id)
{
    SWSS_LOG_ENTER();

    return (sai_object_id_t)(((uint64_t)switch_index << 56) | ((uint64_t)object_type << 48) | virtual_id);
}

sai_object_id_t redis_create_switch_virtual_object_id()
{
    SWSS_LOG_ENTER();

    /*
     * NOTE: Switch ids are deterministic.
     */

    int index = redis_get_free_switch_id_index();

    return redis_construct_object_id(SAI_OBJECT_TYPE_SWITCH, index, index);
}

sai_object_type_t sai_object_type_query(
        _In_ sai_object_id_t object_id)
{
    SWSS_LOG_ENTER();

    if (object_id == SAI_NULL_OBJECT_ID)
    {
        return SAI_OBJECT_TYPE_NULL;
    }

    sai_object_type_t ot = (sai_object_type_t)((object_id >> 48) & 0xFF);

    if (ot == SAI_OBJECT_TYPE_NULL || ot >= SAI_OBJECT_TYPE_MAX)
    {
        SWSS_LOG_ERROR("invalid object id 0x%lx", object_id);

        /*
         * We can't throw here, since it would give no meaningfull message.
         * Throwing at one level up is better.
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

    int sw_index = (int)((oid >> 56) & 0xFF);

    sai_object_id_t sw_id = redis_construct_object_id(SAI_OBJECT_TYPE_SWITCH, sw_index, sw_index);

    return sw_id;
}

int redis_get_switch_id_index(
        _In_ sai_object_id_t switch_id)
{
    SWSS_LOG_ENTER();

    sai_object_type_t switch_object_type = sai_object_type_query(switch_id);

    if (switch_object_type == SAI_OBJECT_TYPE_SWITCH)
    {
        return (int)((switch_id >> 56) & 0xFF);
    }

    SWSS_LOG_THROW("object type of switch %s is %s, should be SWITCH",
            sai_serialize_object_id(switch_id).c_str(),
            sai_serialize_object_type(switch_object_type).c_str());
}

sai_object_id_t redis_create_virtual_object_id(
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
        sai_object_id_t object_id = redis_create_switch_virtual_object_id();

        SWSS_LOG_DEBUG("created SWITCH VID 0x%lx", object_id);

        return object_id;
    }

    int index = redis_get_switch_id_index(switch_id);

    uint64_t virtual_id = g_redisClient->incr("VIDCOUNTER");

    sai_object_id_t object_id = redis_construct_object_id(object_type, index, virtual_id);

    SWSS_LOG_DEBUG("created VID 0x%lx", object_id);

    return object_id;
}

void redis_free_virtual_object_id(
        _In_ sai_object_id_t object_id)
{
    SWSS_LOG_ENTER();

    if (sai_object_type_query(object_id) == SAI_OBJECT_TYPE_SWITCH)
    {
        redis_free_switch_id_index(redis_get_switch_id_index(object_id));
    }
}

sai_status_t internal_redis_generic_create(
        _In_ sai_object_type_t object_type,
        _In_ const std::string &serialized_object_id,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    std::vector<swss::FieldValueTuple> entry = SaiAttributeList::serialize_attr_list(
            object_type,
            attr_count,
            attr_list,
            false);

    if (entry.size() == 0)
    {
        // make sure that we put object into db
        // even if there are no attributes set
        swss::FieldValueTuple null("NULL", "NULL");

        entry.push_back(null);
    }

    std::string str_object_type = sai_serialize_object_type(object_type);

    std::string key = str_object_type + ":" + serialized_object_id;

    SWSS_LOG_DEBUG("generic create key: %s, fields: %lu", key.c_str(), entry.size());

    if (g_record)
    {
        recordLine("c|" + key + "|" + joinFieldValues(entry));
    }

    g_asicState->set(key, entry, "create");

    // we assume create will always succeed which may not be true
    // we should make this synchronous call
    return SAI_STATUS_SUCCESS;
}

sai_status_t redis_generic_create(
        _In_ sai_object_type_t object_type,
        _Out_ sai_object_id_t* object_id,
        _In_ sai_object_id_t switch_id,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    // on create vid is put in db by syncd
    *object_id = redis_create_virtual_object_id(object_type, switch_id);

    if (*object_id == SAI_NULL_OBJECT_ID)
    {
        SWSS_LOG_ERROR("failed to create %s, with switch id: %s",
                sai_serialize_object_type(object_type).c_str(),
                sai_serialize_object_id(switch_id).c_str());

        return SAI_STATUS_INSUFFICIENT_RESOURCES;
    }

    std::string str_object_id = sai_serialize_object_id(*object_id);

    return internal_redis_generic_create(
            object_type,
            str_object_id,
            attr_count,
            attr_list);
}

sai_status_t redis_generic_create_fdb_entry(
        _In_ const sai_fdb_entry_t *fdb_entry,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    std::string str_fdb_entry = sai_serialize_fdb_entry(*fdb_entry);

    return internal_redis_generic_create(
            SAI_OBJECT_TYPE_FDB_ENTRY,
            str_fdb_entry,
            attr_count,
            attr_list);
}

sai_status_t redis_generic_create_neighbor_entry(
        _In_ const sai_neighbor_entry_t* neighbor_entry,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    std::string str_neighbor_entry = sai_serialize_neighbor_entry(*neighbor_entry);

    return internal_redis_generic_create(
            SAI_OBJECT_TYPE_NEIGHBOR_ENTRY,
            str_neighbor_entry,
            attr_count,
            attr_list);
}

sai_status_t redis_generic_create_route_entry(
        _In_ const sai_route_entry_t* route_entry,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    std::string str_route_entry = sai_serialize_route_entry(*route_entry);

    return internal_redis_generic_create(
            SAI_OBJECT_TYPE_ROUTE_ENTRY,
            str_route_entry,
            attr_count,
            attr_list);
}
