#include "sai_redis.h"
#include "meta/saiserialize.h"
#include "meta/saiattributelist.h"

sai_status_t internal_redis_generic_remove(
        _In_ sai_object_type_t object_type,
        _In_ const std::string &serialized_object_id)
{
    SWSS_LOG_ENTER();

    std::string str_object_type = sai_serialize_object_type(object_type);

    std::string key = str_object_type + ":" + serialized_object_id;

    SWSS_LOG_DEBUG("generic remove key: %s", key.c_str());

    if (g_record)
    {
        recordLine("r|" + key);
    }

    g_asicState->del(key, "remove");

    return SAI_STATUS_SUCCESS;
}

sai_status_t redis_generic_remove(
        _In_ sai_object_type_t object_type,
        _In_ sai_object_id_t object_id)
{
    SWSS_LOG_ENTER();

    std::string str_object_id = sai_serialize_object_id(object_id);

    sai_status_t status = internal_redis_generic_remove(
            object_type,
            str_object_id);

    if (object_type == SAI_OBJECT_TYPE_SWITCH &&
            status == SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_NOTICE("removing switch id %s", sai_serialize_object_id(object_id).c_str());

        redis_free_virtual_object_id(object_id);

        // TODO do we need some more actions here ? to clean all
        // objects that are in the same switch that were snooped
        // inside metadata ? should that be metadata job?
    }

    return status;
}

sai_status_t redis_generic_remove_fdb_entry(
        _In_ const sai_fdb_entry_t* fdb_entry)
{
    SWSS_LOG_ENTER();

    std::string str_fdb_entry = sai_serialize_fdb_entry(*fdb_entry);

    return internal_redis_generic_remove(
            SAI_OBJECT_TYPE_FDB_ENTRY,
            str_fdb_entry);
}

sai_status_t redis_generic_remove_neighbor_entry(
        _In_ const sai_neighbor_entry_t* neighbor_entry)
{
    SWSS_LOG_ENTER();

    std::string str_neighbor_entry = sai_serialize_neighbor_entry(*neighbor_entry);

    return internal_redis_generic_remove(
            SAI_OBJECT_TYPE_NEIGHBOR_ENTRY,
            str_neighbor_entry);
}

sai_status_t redis_generic_remove_route_entry(
        _In_ const sai_route_entry_t* route_entry)
{
    SWSS_LOG_ENTER();

    std::string str_route_entry = sai_serialize_route_entry(*route_entry);

    return internal_redis_generic_remove(
            SAI_OBJECT_TYPE_ROUTE_ENTRY,
            str_route_entry);
}
