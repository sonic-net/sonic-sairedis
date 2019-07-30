#include "sai_vs.h"
#include "sai_vs_internal.h"
#include "sai_vs_state.h"
#include "sai_vs_switch_BCM56850.h"
#include "sai_vs_switch_MLNX2700.h"

#include <algorithm>

#define MAX_OBJLIST_LEN 128

sai_status_t vs_clear_port_all_stats(
        _In_ sai_object_id_t port_id)
{
    MUTEX();

    SWSS_LOG_ENTER();

    return SAI_STATUS_NOT_IMPLEMENTED;
}

sai_status_t vs_create_port(
            _Out_ sai_object_id_t *port_id,
            _In_ sai_object_id_t switch_id,
            _In_ uint32_t attr_count,
            _In_ const sai_attribute_t *attr_list)
{
    MUTEX();
    SWSS_LOG_ENTER();

    /* create port */
    CHECK_STATUS(meta_sai_create_oid(
                (sai_object_type_t)SAI_OBJECT_TYPE_PORT,
                port_id,
                switch_id,
                attr_count,
                attr_list,
                &vs_generic_create));

    // TODO needs to be revisited

    if (g_vs_switch_type == SAI_VS_SWITCH_TYPE_BCM56850)
    {
        vs_create_port_BCM56850(*port_id, switch_id);
    }
    else if (g_vs_switch_type == SAI_VS_SWITCH_TYPE_MLNX2700)
    {
        vs_create_port_MLNX2700(*port_id, switch_id);
    }
    else
    {
        SWSS_LOG_ERROR("unknown switch type: %d", g_vs_switch_type);
        return SAI_STATUS_FAILURE;
    }

    return SAI_STATUS_SUCCESS;
}

static bool vs_get_object_list(
        _In_ sai_object_id_t object_id,
        _In_ sai_attr_id_t attr_id,
        _Out_ std::vector<sai_object_id_t>& objlist)
{
    SWSS_LOG_ENTER();

    objlist.clear();

    sai_object_type_t object_type = sai_object_type_query(object_id);

    auto* meta = sai_metadata_get_attr_metadata(object_type, attr_id);

    if (meta == nullptr)
    {
        SWSS_LOG_THROW("failed to get metadata for OID %s and attrid: %d",
                sai_serialize_object_id(object_id).c_str(),
                attr_id);
    }

    if (meta->attrvaluetype != SAI_ATTR_VALUE_TYPE_OBJECT_LIST)
    {
        SWSS_LOG_THROW("attr %s is not objlist attribute", meta->attridname);
    }

    sai_status_t status;

    sai_attribute_t attr;

    objlist.resize(MAX_OBJLIST_LEN);

    attr.id = attr_id;

    attr.value.objlist.count = MAX_OBJLIST_LEN;
    attr.value.objlist.list = objlist.data();

    status = vs_generic_get(object_type, object_id, 1, &attr);

    if (status != SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_ERROR("failed to obtain %s for %s queues: %s",
                meta->attridname,
                sai_serialize_object_id(object_id).c_str(),
                sai_serialize_status(status).c_str());

        objlist.clear();
        return false;
    }

    objlist.resize(attr.value.objlist.count);

    SWSS_LOG_NOTICE("%s returned %zu objects for %s",
            meta->attridname,
            objlist.size(),
            sai_serialize_object_id(object_id).c_str());

    return true;
}

static bool vs_get_port_queues(
        _In_ sai_object_id_t port_id,
        _Out_ std::vector<sai_object_id_t>& queues)
{
    SWSS_LOG_ENTER();

    return vs_get_object_list(port_id, SAI_PORT_ATTR_QOS_QUEUE_LIST, queues);
}

static bool vs_get_port_ipgs(
        _In_ sai_object_id_t port_id,
        _Out_ std::vector<sai_object_id_t>& ipgs)
{
    SWSS_LOG_ENTER();

    return vs_get_object_list(port_id, SAI_PORT_ATTR_INGRESS_PRIORITY_GROUP_LIST, ipgs);
}

static bool vs_get_port_sg(
        _In_ sai_object_id_t port_id,
        _Out_ std::vector<sai_object_id_t>& sg)
{
    SWSS_LOG_ENTER();

    // scheduler groups are organized in tree, but
    // SAI_PORT_ATTR_INGRESS_PRIORITY_GROUP_LIST should return all scheduler groups in that tree

    return vs_get_object_list(port_id, SAI_PORT_ATTR_QOS_SCHEDULER_GROUP_LIST, sg);
}

bool vs_check_object_default_state(
        _In_ sai_object_id_t object_id)
{
    SWSS_LOG_ENTER();

    sai_object_type_t object_type = sai_object_type_query(object_id);

    if (object_type == SAI_OBJECT_TYPE_NULL)
    {
        SWSS_LOG_ERROR("failed to get object type for oid: %s",
                sai_serialize_object_id(object_id).c_str());

        return false;
    }

    auto* oti = sai_metadata_get_object_type_info(object_type);

    if (oti == nullptr)
    {
        SWSS_LOG_THROW("failed to get object type info for object type: %s",
                sai_serialize_object_type(object_type).c_str());
    }

    // iterate over all attributes

    for (size_t i = 0; i < oti->attrmetadatalength; i++)
    {
        auto* meta = oti->attrmetadata[i];

        // skip readonly, mandatory on create and non oid attributes

        if (meta->isreadonly)
            continue;

        if (!meta->isoidattribute)
            continue;

        // those attributes must be skipped since those dependencies will be automatically broken
        if (meta->objecttype == SAI_OBJECT_TYPE_SCHEDULER_GROUP && meta->attrid == SAI_SCHEDULER_GROUP_ATTR_PORT_ID)
            continue;

        if (meta->objecttype == SAI_OBJECT_TYPE_SCHEDULER_GROUP && meta->attrid == SAI_SCHEDULER_GROUP_ATTR_PARENT_NODE)
            continue;

        if (meta->objecttype == SAI_OBJECT_TYPE_QUEUE && meta->attrid == SAI_QUEUE_ATTR_PORT)
            continue;

        if (meta->objecttype == SAI_OBJECT_TYPE_QUEUE && meta->attrid == SAI_QUEUE_ATTR_PARENT_SCHEDULER_NODE)
            continue;

        if (meta->objecttype == SAI_OBJECT_TYPE_INGRESS_PRIORITY_GROUP && meta->attrid == SAI_INGRESS_PRIORITY_GROUP_ATTR_PORT)
            continue;

        // here we have only oid/object list attrs and we expect each of this
        // attribute will be in default state which for oid is usually NULL,
        // and for object list is empty

        sai_attribute_t attr;

        attr.id = meta->attrid;

        sai_status_t status;

        std::vector<sai_object_id_t> objlist;

        if (meta->attrvaluetype == SAI_ATTR_VALUE_TYPE_OBJECT_ID)
        {
            // ok
        }
        else if (meta->attrvaluetype == SAI_ATTR_VALUE_TYPE_OBJECT_LIST)
        {
            objlist.resize(MAX_OBJLIST_LEN);

            attr.value.objlist.count = MAX_OBJLIST_LEN;
            attr.value.objlist.list = objlist.data();
        }
        else
        {
            // unable to check whether object is in default state, need fix

            SWSS_LOG_ERROR("unsupported oid attribute: %s, FIX ME!", meta->attridname);
            return false;
        }

        status = vs_generic_get(object_type, object_id, 1, &attr);

        switch (status)
        {
            case SAI_STATUS_NOT_IMPLEMENTED:
            case SAI_STATUS_NOT_SUPPORTED:
                continue;

            case SAI_STATUS_SUCCESS:
                break;

            default:

                SWSS_LOG_ERROR("unexpected status %s on %s obj %s",
                        sai_serialize_status(status).c_str(),
                        meta->attridname,
                        sai_serialize_object_id(object_id).c_str());
                return false;

        }


        if (meta->attrvaluetype == SAI_ATTR_VALUE_TYPE_OBJECT_ID)
        {
            if (attr.value.oid != SAI_NULL_OBJECT_ID)
            {
                SWSS_LOG_ERROR("expected null object id on %s on %s, but got: %s",
                        meta->attridname,
                        sai_serialize_object_id(object_id).c_str(),
                        sai_serialize_object_id(attr.value.oid).c_str());

                return false;
            }

        }
        else if (meta->attrvaluetype == SAI_ATTR_VALUE_TYPE_OBJECT_LIST)
        {
            if (objlist.size())
            {
                SWSS_LOG_ERROR("expected empty list on %s on %s, contents:",
                        meta->attridname,
                        sai_serialize_object_id(object_id).c_str());

                for (auto oid: objlist)
                {
                    SWSS_LOG_ERROR(" - oid: %s", sai_serialize_object_id(oid).c_str());
                }

                return false;
            }
        }
        else
        {
            // unable to check whether object is in default state, need fix

            SWSS_LOG_ERROR("unsupported oid attribute: %s, FIX ME!", meta->attridname);
            return false;
        }
    }

    // TODO later there can be issue when we for example add extra queues to
    // the port those new queues should be removed by user first before
    // removing port, and currently we don't have a way to differentiate those

    // object is in default state
    return true;
}

bool vs_check_object_list_default_state(
        _Out_ const std::vector<sai_object_id_t>& objlist)
{
    SWSS_LOG_ENTER();

    return std::all_of(objlist.begin(), objlist.end(),
            [](sai_object_id_t oid) { return vs_check_object_default_state(oid); });
}

sai_status_t vs_check_port_dependencies(
        _In_ sai_object_id_t port_id,
        _Out_ std::vector<sai_object_id_t>& dep)
{
    SWSS_LOG_ENTER();

    // check if port exists's

    sai_object_id_t switch_id = sai_switch_id_query(port_id);

    if (switch_id == SAI_NULL_OBJECT_ID)
    {
        SWSS_LOG_ERROR("failed to obtain switch_id from object %s",
                sai_serialize_object_id(port_id).c_str());

        return SAI_STATUS_FAILURE;
    }

    sai_object_type_t ot = sai_object_type_query(port_id);

    if (ot != SAI_OBJECT_TYPE_PORT)
    {
        SWSS_LOG_ERROR("expected object type PORT but object %s has type %s",
                sai_serialize_object_id(port_id).c_str(),
                sai_serialize_object_type(ot).c_str());

        return SAI_STATUS_FAILURE;
    }

    std::string str_port_id = sai_serialize_object_id(port_id);

    auto &objectHash = g_switch_state_map.at(switch_id)->objectHash.at(ot);

    auto it = objectHash.find(str_port_id);

    if (it == objectHash.end())
    {
        SWSS_LOG_ERROR("port not found %s:%s",
                sai_serialize_object_type(ot).c_str(),
                str_port_id.c_str());

        return SAI_STATUS_ITEM_NOT_FOUND;
    }

    // port was found
    SWSS_LOG_NOTICE("port %s found, for removal",
                sai_serialize_object_id(port_id).c_str());

    // obtain objects to examine

    std::vector<sai_object_id_t> queues;
    std::vector<sai_object_id_t> ipgs;
    std::vector<sai_object_id_t> sg;

    bool result = true;

    result &= vs_get_port_queues(port_id, queues);
    result &= vs_get_port_ipgs(port_id, ipgs);
    result &= vs_get_port_sg(port_id, sg);

    if (!result)
    {
        SWSS_LOG_ERROR("failed to obtain required objects on port %s",
                sai_serialize_object_id(port_id).c_str());

        return SAI_STATUS_FAILURE;
    }

    // check if all objects are in default state

    result &= vs_check_object_default_state(port_id);
    result &= vs_check_object_list_default_state(queues);
    result &= vs_check_object_list_default_state(ipgs);
    result &= vs_check_object_list_default_state(sg);

    if (!result)
    {
        SWSS_LOG_ERROR("one of objects is not in default state, can't remove port %s",
                sai_serialize_object_id(port_id).c_str());

        return SAI_STATUS_FAILURE;
    }

    SWSS_LOG_NOTICE("all depending objects on port %s are in default state",
                sai_serialize_object_id(port_id).c_str());

    dep.insert(dep.end(), queues.begin(), queues.end());
    dep.insert(dep.end(), ipgs.begin(), ipgs.end());
    dep.insert(dep.end(), sg.begin(), sg.end());

    // TODO there may be issues with bridge ports created on that port if they
    // are not removed before port remove, this needs to be addressed for warm
    // boot support

    return SAI_STATUS_SUCCESS;
}

sai_status_t vs_remove_port(
            _In_ sai_object_id_t port_id)
{
    MUTEX();
    SWSS_LOG_ENTER();

    std::vector<sai_object_id_t> dep;

    sai_status_t status = vs_check_port_dependencies(port_id, dep);

    if (status != SAI_STATUS_SUCCESS)
    {
        return status;
    }

    // NOTE: we should check references on depending objects to see if it's safe
    // to remove every object but we count on metadata references count to do
    // that for us

    status = meta_sai_remove_oid(
            (sai_object_type_t)SAI_OBJECT_TYPE_PORT,
            port_id,
            &vs_generic_remove);

    if (status != SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_ERROR("failed to remove port: %s",
                sai_serialize_object_id(port_id).c_str());

        return status;
    }

    SWSS_LOG_NOTICE("port %s was successfully removed, removing depending objects now",
            sai_serialize_object_id(port_id).c_str());

    for (auto oid: dep)
    {
        // meta_sai_remove_oid automatically removed related oids internally
        // so we just need to execute remove for virtual switch db

        status = vs_generic_remove(
                sai_object_type_query(oid),
                oid);

        if (status != SAI_STATUS_SUCCESS)
        {
            // we can't continue, there is a bug somewhere if we can't remove
            // port related objects: queues, ipgs, sg

            SWSS_LOG_THROW("FATAL: failed to removed port related oid: %s: %s, bug!",
                    sai_serialize_object_type(sai_object_type_query(oid)).c_str(),
                    sai_serialize_object_id(oid).c_str());
        }
    }

    SWSS_LOG_NOTICE("successfully removed all %zu port related objects", dep.size());

    return SAI_STATUS_SUCCESS;
}

VS_SET(PORT,port);
VS_GET(PORT,port);
VS_GENERIC_QUAD(PORT_POOL,port_pool);
VS_GENERIC_STATS(PORT,port);
VS_GENERIC_STATS(PORT_POOL,port_pool);

const sai_port_api_t vs_port_api = {

    VS_GENERIC_QUAD_API(port)
    VS_GENERIC_STATS_API(port)

    vs_clear_port_all_stats,

    VS_GENERIC_QUAD_API(port_pool)
    VS_GENERIC_STATS_API(port_pool)
};
