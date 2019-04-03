#include "sai_redis.h"
#include "meta/sai_serialize.h"
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

    if (ot == SAI_OBJECT_TYPE_NULL || ot >= SAI_OBJECT_TYPE_EXTENSIONS_MAX)
    {
        SWSS_LOG_ERROR("invalid object id 0x%lx", object_id);

        /*
         * We can't throw here, since it would give no meaningful message.
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
            (object_type >= SAI_OBJECT_TYPE_EXTENSIONS_MAX))
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

sai_status_t redis_bulk_generic_create(
        _In_ sai_object_type_t object_type,
        _In_ uint32_t object_count,
        _Out_ sai_object_id_t *object_id, /* array */
        _In_ sai_object_id_t switch_id,
        _In_ const uint32_t *attr_count, /* array */
        _In_ const sai_attribute_t *const *attr_list, /* array */
        _Inout_ sai_status_t *object_statuses) /* array */
{
    SWSS_LOG_ENTER();

    std::vector<std::string> serialized_object_ids;

    // on create vid is put in db by syncd
    for (uint32_t idx = 0; idx < object_count; idx++)
    {
        object_id[idx] = redis_create_virtual_object_id(object_type, switch_id);
        if (object_id[idx] == SAI_NULL_OBJECT_ID)
        {
            SWSS_LOG_ERROR("failed to create %s, with switch id: %s",
                    sai_serialize_object_type(object_type).c_str(),
                    sai_serialize_object_id(switch_id).c_str());

            return SAI_STATUS_INSUFFICIENT_RESOURCES;
        }
        std::string str_object_id = sai_serialize_object_id(object_id[idx]);
        serialized_object_ids.push_back(str_object_id);
    }

    return internal_redis_bulk_generic_create(
            object_type,
            serialized_object_ids,
            attr_count,
            attr_list,
            object_statuses);
}

sai_status_t internal_redis_bulk_generic_create(
        _In_ sai_object_type_t object_type,
        _In_ const std::vector<std::string> &serialized_object_ids,
        _In_ const uint32_t *attr_count,
        _In_ const sai_attribute_t *const *attr_list,
        _Inout_ sai_status_t *object_statuses)
{
    SWSS_LOG_ENTER();

    std::string str_object_type = sai_serialize_object_type(object_type);

    std::vector<swss::FieldValueTuple> entries;
    std::vector<swss::FieldValueTuple> entriesWithStatus;

    /*
     * We are recording all entries and their statuses, but we send to sairedis
     * only those that succeeded metadata check, since only those will be
     * executed on syncd, so there is no need with bothering decoding statuses
     * on syncd side.
     */

    for (size_t idx = 0; idx < serialized_object_ids.size(); ++idx)
    {
        std::vector<swss::FieldValueTuple> entry =
            SaiAttributeList::serialize_attr_list(object_type, attr_count[idx], &attr_list[idx][0], false);

        std::string str_attr = joinFieldValues(entry);

        std::string str_status = sai_serialize_status(object_statuses[idx]);

        std::string joined = str_attr + "|" + str_status;

        swss::FieldValueTuple fvt(serialized_object_ids[idx] , joined);

        entriesWithStatus.push_back(fvt);

        if (object_statuses[idx] != SAI_STATUS_SUCCESS)
        {
            SWSS_LOG_WARN("skipping %s since status is %s",
                    serialized_object_ids[idx].c_str(),
                    str_status.c_str());

            continue;
        }

        swss::FieldValueTuple fvtNoStatus(serialized_object_ids[idx] , str_attr);

        entries.push_back(fvtNoStatus);
    }

    /*
     * We are adding number of entries to actually add ':' to be compatible
     * with previous
     */

    if (g_record)
    {
        std::string joined;

        for (const auto &e: entriesWithStatus)
        {
            // ||obj_id|attr=val|attr=val|status||obj_id|attr=val|attr=val|status

            joined += "||" + fvField(e) + "|" + fvValue(e);
        }

        /*
         * Capital 'C' stands for bulk CREATE operation.
         */

        recordLine("C|" + str_object_type + joined);
    }

    // key:         object_type:count
    // field:       object_id
    // value:       object_attrs
    std::string key = str_object_type + ":" + std::to_string(entries.size());

    if (entries.size())
    {
        g_asicState->set(key, entries, "bulkcreate");
    }

    return SAI_STATUS_SUCCESS;
}

#define REDIS_ENTRY_CREATE(OT,ot)                       \
    sai_status_t redis_generic_create_ ## ot(           \
            _In_ const sai_ ## ot ## _t * entry,        \
            _In_ uint32_t attr_count,                   \
            _In_ const sai_attribute_t *attr_list)      \
    {                                                   \
        SWSS_LOG_ENTER();                               \
        std::string str = sai_serialize_ ## ot(*entry); \
        return internal_redis_generic_create(           \
                SAI_OBJECT_TYPE_ ## OT,                 \
                str,                                    \
                attr_count,                             \
                attr_list);                             \
    }

REDIS_ENTRY_CREATE(FDB_ENTRY,fdb_entry);
REDIS_ENTRY_CREATE(INSEG_ENTRY,inseg_entry);
REDIS_ENTRY_CREATE(IPMC_ENTRY,ipmc_entry);
REDIS_ENTRY_CREATE(L2MC_ENTRY,l2mc_entry);
REDIS_ENTRY_CREATE(MCAST_FDB_ENTRY,mcast_fdb_entry);
REDIS_ENTRY_CREATE(NEIGHBOR_ENTRY,neighbor_entry);
REDIS_ENTRY_CREATE(ROUTE_ENTRY,route_entry);
