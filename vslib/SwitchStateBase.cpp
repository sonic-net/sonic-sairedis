#include "SwitchStateBase.h"

#include "swss/logger.h"
#include "meta/sai_serialize.h"
#include "meta/NotificationTamTelTypeConfigChange.h"
#include "EventPayloadNotification.h"

#include <net/if.h>
#include <unistd.h>

#include <algorithm>

#define SAI_VS_MAX_PORTS 1024

using namespace saivs;

SwitchStateBase::SwitchStateBase(
        _In_ sai_object_id_t switch_id,
        _In_ std::shared_ptr<RealObjectIdManager> manager,
        _In_ std::shared_ptr<SwitchConfig> config):
    SwitchState(switch_id, config),
    m_realObjectIdManager(manager)
{
    SWSS_LOG_ENTER();

    m_macsecManager.cleanup_macsec_device();
}

SwitchStateBase::SwitchStateBase(
        _In_ sai_object_id_t switch_id,
        _In_ std::shared_ptr<RealObjectIdManager> manager,
        _In_ std::shared_ptr<SwitchConfig> config,
        _In_ std::shared_ptr<WarmBootState> warmBootState):
    SwitchState(switch_id, config),
    m_realObjectIdManager(manager)
{
    SWSS_LOG_ENTER();

    m_macsecManager.cleanup_macsec_device();

    if (warmBootState)
    {
        for (auto& kvp: warmBootState->m_objectHash)
        {
            // we write only existing ones, since base constructor
            // created empty entries for non existing object types
            m_objectHash[kvp.first] = kvp.second;
        }

        if (m_switchConfig->m_useTapDevice)
        {
            m_fdb_info_set = warmBootState->m_fdbInfoSet;

            // TODO populate m_hostif_info_map - need to be able to remove port after warm boot
            // should be auto populated vs_recreate_hostif_tap_interfaces on create_switch
        }
    }
}

SwitchStateBase::~SwitchStateBase()
{
    SWSS_LOG_ENTER();

    m_macsecManager.cleanup_macsec_device();
}

sai_status_t SwitchStateBase::create(
        _In_ sai_object_type_t object_type,
        _Out_ sai_object_id_t *object_id,
        _In_ sai_object_id_t switch_id,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    if (object_type == SAI_OBJECT_TYPE_SWITCH)
    {
        SWSS_LOG_THROW("this method can't be used to create switch");
    }

    *object_id = m_realObjectIdManager->allocateNewObjectId(object_type, switch_id);

    auto sid = sai_serialize_object_id(*object_id);

    return create_internal(object_type, sid, switch_id, attr_count, attr_list);
}

uint32_t SwitchStateBase::getNewDebugCounterIndex()
{
    SWSS_LOG_ENTER();

    for (uint32_t i = 0; i < maxDebugCounters; i++)
    {
        if (m_indices.find(i) == m_indices.end())
        {
            m_indices.insert(i);
            return i;
        }
    }

    return UINT32_MAX;
}

sai_status_t SwitchStateBase::createDebugCounter(
        _In_ sai_object_id_t object_id,
        _In_ sai_object_id_t switch_id,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    auto index = getNewDebugCounterIndex();

    if (index > maxDebugCounters)
    {
        SWSS_LOG_ERROR("Cannot create more than %d debug counters", maxDebugCounters);

        return SAI_STATUS_FAILURE;
    }

    auto sid = sai_serialize_object_id(object_id);

    CHECK_STATUS(create_internal(SAI_OBJECT_TYPE_DEBUG_COUNTER, sid, switch_id, attr_count, attr_list));

    sai_attribute_t attr;

    attr.id = SAI_DEBUG_COUNTER_ATTR_INDEX;
    attr.value.u32 = index;

    return set(SAI_OBJECT_TYPE_DEBUG_COUNTER, sid, &attr);
}

sai_status_t SwitchStateBase::create(
        _In_ sai_object_type_t object_type,
        _In_ const std::string &serializedObjectId,
        _In_ sai_object_id_t switch_id,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    if (object_type == SAI_OBJECT_TYPE_DEBUG_COUNTER)
    {
        sai_object_id_t object_id;
        sai_deserialize_object_id(serializedObjectId, object_id);
        return createDebugCounter(object_id, switch_id, attr_count, attr_list);
    }

    if (object_type == SAI_OBJECT_TYPE_PORT)
    {
        sai_object_id_t object_id;
        sai_deserialize_object_id(serializedObjectId, object_id);
        return createPort(object_id, switch_id, attr_count, attr_list);
    }

    if (object_type == SAI_OBJECT_TYPE_HOSTIF)
    {
        sai_object_id_t object_id;
        sai_deserialize_object_id(serializedObjectId, object_id);
        return createHostif(object_id, switch_id, attr_count, attr_list);
    }

    if (object_type == SAI_OBJECT_TYPE_MACSEC_PORT)
    {
        sai_object_id_t object_id;
        sai_deserialize_object_id(serializedObjectId, object_id);
        return createMACsecPort(object_id, switch_id, attr_count, attr_list);
    }

    if (object_type == SAI_OBJECT_TYPE_MACSEC_SC)
    {
        sai_object_id_t object_id;
        sai_deserialize_object_id(serializedObjectId, object_id);
        return createMACsecSC(object_id, switch_id, attr_count, attr_list);
    }

    if (object_type == SAI_OBJECT_TYPE_MACSEC_SA)
    {
        sai_object_id_t object_id;
        sai_deserialize_object_id(serializedObjectId, object_id);
        return createMACsecSA(object_id, switch_id, attr_count, attr_list);
    }

    if (object_type == SAI_OBJECT_TYPE_NEIGHBOR_ENTRY && m_system_port_list.size())
    {
        // Neighbor entry programming for VOQ systems
        return createVoqSystemNeighborEntry(serializedObjectId, switch_id, attr_count, attr_list);
    }

    return create_internal(object_type, serializedObjectId, switch_id, attr_count, attr_list);
}

sai_status_t SwitchStateBase::create_internal(
        _In_ sai_object_type_t object_type,
        _In_ const std::string &serializedObjectId,
        _In_ sai_object_id_t switch_id,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    auto &objectHash = m_objectHash.at(object_type);

    if (m_switchConfig->m_resourceLimiter)
    {
        size_t limit = m_switchConfig->m_resourceLimiter->getObjectTypeLimit(object_type);

        if (objectHash.size() >= limit)
        {
            SWSS_LOG_ERROR("too many %s, created %zu is resource limit",
                    sai_serialize_object_type(object_type).c_str(),
                    limit);

            return SAI_STATUS_INSUFFICIENT_RESOURCES;
        }
    }

    auto it = objectHash.find(serializedObjectId);

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
                    serializedObjectId.c_str());

            return SAI_STATUS_ITEM_ALREADY_EXISTS;
        }
    }

    if (objectHash.find(serializedObjectId) == objectHash.end())
    {
        /*
         * Number of attributes may be zero, so see if actual entry was created
         * with empty hash.
         */

        objectHash[serializedObjectId] = {};
    }

    for (uint32_t i = 0; i < attr_count; ++i)
    {
        auto a = std::make_shared<SaiAttrWrap>(object_type, &attr_list[i]);

        objectHash[serializedObjectId][a->getAttrMetadata()->attridname] = a;
    }

    return SAI_STATUS_SUCCESS;
}

void SwitchStateBase::releaseDebugCounterIndex(
        _In_ uint32_t index)
{
    SWSS_LOG_ENTER();

    m_indices.erase(index);
}

sai_status_t SwitchStateBase::remove(
        _In_ sai_object_type_t object_type,
        _In_ sai_object_id_t objectId)
{
    SWSS_LOG_ENTER();

    auto sid = sai_serialize_object_id(objectId);

    return remove(object_type, sid);
}

sai_status_t SwitchStateBase::removeDebugCounter(
        _In_ sai_object_id_t objectId)
{
    SWSS_LOG_ENTER();

    sai_attribute_t attr;

    attr.id = SAI_DEBUG_COUNTER_ATTR_INDEX;

    CHECK_STATUS(get(SAI_OBJECT_TYPE_DEBUG_COUNTER, objectId, 1, &attr));

    auto sid = sai_serialize_object_id(objectId);

    CHECK_STATUS(remove_internal(SAI_OBJECT_TYPE_DEBUG_COUNTER, sid));

    releaseDebugCounterIndex(attr.value.u32);

    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchStateBase::createPort(
        _In_ sai_object_id_t object_id,
        _In_ sai_object_id_t switch_id,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    auto sid = sai_serialize_object_id(object_id);

    CHECK_STATUS(create_internal(SAI_OBJECT_TYPE_PORT, sid, switch_id, attr_count, attr_list));

    return create_port_dependencies(object_id);
}

sai_status_t SwitchStateBase::removePort(
        _In_ sai_object_id_t objectId)
{
    SWSS_LOG_ENTER();

    if (!isPortReadyToBeRemove(objectId))
    {
        SWSS_LOG_ERROR("port %s still have some active dependencies, can't remove",
                sai_serialize_object_id(objectId).c_str());

        return SAI_STATUS_OBJECT_IN_USE;
    }

    auto dep = getPortDependencies(objectId);

    auto sid = sai_serialize_object_id(objectId);

    CHECK_STATUS(remove_internal(SAI_OBJECT_TYPE_PORT, sid));

    SWSS_LOG_NOTICE("port %s was successfully removed, removing depending objects now",
            sai_serialize_object_id(objectId).c_str());

    for (auto oid: dep)
    {
        // meta_sai_remove_oid automatically removed related oids internally
        // so we just need to execute remove for virtual switch db

        auto status = remove(objectTypeQuery(oid), oid);

        if (status != SAI_STATUS_SUCCESS)
        {
            // we can't continue, there is a bug somewhere if we can't remove
            // port related objects: queues, ipgs, sg

            SWSS_LOG_THROW("FATAL: failed to removed port related oid: %s: %s, bug!",
                    sai_serialize_object_type(objectTypeQuery(oid)).c_str(),
                    sai_serialize_object_id(oid).c_str());
        }
    }

    SWSS_LOG_NOTICE("successfully removed all %zu port related objects", dep.size());

    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchStateBase::remove(
        _In_ sai_object_type_t object_type,
        _In_ const std::string &serializedObjectId)
{
    SWSS_LOG_ENTER();

    if (object_type == SAI_OBJECT_TYPE_DEBUG_COUNTER)
    {
        sai_object_id_t objectId;
        sai_deserialize_object_id(serializedObjectId, objectId);
        return removeDebugCounter(objectId);
    }

    if (object_type == SAI_OBJECT_TYPE_PORT)
    {
        sai_object_id_t objectId;
        sai_deserialize_object_id(serializedObjectId, objectId);
        return removePort(objectId);
    }

    if (object_type == SAI_OBJECT_TYPE_HOSTIF)
    {
        sai_object_id_t objectId;
        sai_deserialize_object_id(serializedObjectId, objectId);
        return removeHostif(objectId);
    }

    if (object_type == SAI_OBJECT_TYPE_MACSEC_PORT)
    {
        sai_object_id_t objectId;
        sai_deserialize_object_id(serializedObjectId, objectId);
        return removeMACsecPort(objectId);
    }
    else if (object_type == SAI_OBJECT_TYPE_MACSEC_SC)
    {
        sai_object_id_t objectId;
        sai_deserialize_object_id(serializedObjectId, objectId);
        return removeMACsecSC(objectId);
    }
    else if (object_type == SAI_OBJECT_TYPE_MACSEC_SA)
    {
        sai_object_id_t objectId;
        sai_deserialize_object_id(serializedObjectId, objectId);
        return removeMACsecSA(objectId);
    }

    return remove_internal(object_type, serializedObjectId);
}

sai_status_t SwitchStateBase::remove_internal(
        _In_ sai_object_type_t object_type,
        _In_ const std::string &serializedObjectId)
{
    SWSS_LOG_ENTER();

    SWSS_LOG_INFO("removing object: %s", serializedObjectId.c_str());

    auto &objectHash = m_objectHash.at(object_type);

    auto it = objectHash.find(serializedObjectId);

    if (it == objectHash.end())
    {
        SWSS_LOG_ERROR("not found %s:%s",
                sai_serialize_object_type(object_type).c_str(),
                serializedObjectId.c_str());

        return SAI_STATUS_ITEM_NOT_FOUND;
    }

    objectHash.erase(it);

    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchStateBase::setPort(
        _In_ sai_object_id_t portId,
        _In_ const sai_attribute_t* attr)
{
    SWSS_LOG_ENTER();

    if (attr && attr->id == SAI_PORT_ATTR_ADMIN_STATE && m_switchConfig->m_useTapDevice)
    {
        bool up = attr->value.booldata;

        // find corresponding host if interface and bring it down !
        for (auto& kvp: m_hostif_info_map)
        {
            auto tapname = kvp.first;

            if (kvp.second->m_portId == portId)
            {
                std::string vethname = vs_get_veth_name(tapname, portId);

                if (ifup(vethname.c_str(), portId, up, false))
                {
                    SWSS_LOG_ERROR("if admin %s failed on %s failed: %s", (up ? "UP" : "DOWN"), vethname.c_str(),  strerror(errno));

                    return SAI_STATUS_FAILURE;
                }

                SWSS_LOG_NOTICE("if admin %s success on %s", (up ? "UP" : "DOWN"), vethname.c_str());

                break;
            }
        }
    }
    else if (attr && attr->id == SAI_PORT_ATTR_MTU && m_switchConfig->m_useTapDevice)
    {
        uint32_t mtu = attr->value.u32;

        std::string name;

        if (getTapNameFromPortId(portId, name))
        {
            SWSS_LOG_INFO("setting new MTU: %d on %s", mtu, name.c_str());

            std::string vname = vs_get_veth_name(name, portId);

            if (vs_set_dev_mtu(vname.c_str(), mtu) < 0)
            {
                SWSS_LOG_ERROR("failed to set MTU on portId %s",
                        sai_serialize_object_id(portId).c_str());
                return SAI_STATUS_FAILURE;
            }
        }
    }

    auto sid = sai_serialize_object_id(portId);

    return set_internal(SAI_OBJECT_TYPE_PORT, sid, attr);
}

sai_status_t SwitchStateBase::setAclEntry(
        _In_ sai_object_id_t entry_id,
        _In_ const sai_attribute_t* attr)
{
    SWSS_LOG_ENTER();

    if (attr && attr->id == SAI_ACL_ENTRY_ATTR_ACTION_MACSEC_FLOW)
    {
        return setAclEntryMACsecFlowActive(entry_id, attr);
    }

    auto sid = sai_serialize_object_id(entry_id);

    return set_internal(SAI_OBJECT_TYPE_ACL_ENTRY, sid, attr);
}

sai_status_t SwitchStateBase::setTamTelType(
    _In_ sai_object_id_t tam_tel_type_id,
    _In_ const sai_attribute_t *attr)
{
    SWSS_LOG_ENTER();

    if (attr->id == SAI_TAM_TEL_TYPE_ATTR_STATE && attr->value.s32 == SAI_TAM_TEL_TYPE_STATE_CREATE_CONFIG)
    {
        send_tam_tel_type_config_change(tam_tel_type_id);
    }

    auto sid = sai_serialize_object_id(tam_tel_type_id);

    return set_internal(SAI_OBJECT_TYPE_TAM_TEL_TYPE, sid, attr);
}

sai_status_t SwitchStateBase::set(
        _In_ sai_object_type_t objectType,
        _In_ const std::string &serializedObjectId,
        _In_ const sai_attribute_t* attr)
{
    SWSS_LOG_ENTER();

    if (objectType == SAI_OBJECT_TYPE_PORT)
    {
        sai_object_id_t objectId;
        sai_deserialize_object_id(serializedObjectId, objectId);
        return setPort(objectId, attr);
    }

    if (objectType == SAI_OBJECT_TYPE_ACL_ENTRY)
    {
        sai_object_id_t objectId;
        sai_deserialize_object_id(serializedObjectId, objectId);
        return setAclEntry(objectId, attr);
    }

    if (objectType == SAI_OBJECT_TYPE_MACSEC_SA)
    {
        sai_object_id_t objectId;
        sai_deserialize_object_id(serializedObjectId, objectId);
        return setMACsecSA(objectId, attr);
    }

    if (objectType == SAI_OBJECT_TYPE_TAM_TEL_TYPE)
    {
        sai_object_id_t objectId;
        sai_deserialize_object_id(serializedObjectId, objectId);
        return setTamTelType(objectId, attr);
    }

    return set_internal(objectType, serializedObjectId, attr);
}

sai_status_t SwitchStateBase::set_internal(
        _In_ sai_object_type_t objectType,
        _In_ const std::string &serializedObjectId,
        _In_ const sai_attribute_t* attr)
{
    SWSS_LOG_ENTER();

    auto it = m_objectHash.at(objectType).find(serializedObjectId);

    if (it == m_objectHash.at(objectType).end())
    {
        SWSS_LOG_ERROR("not found %s:%s",
                sai_serialize_object_type(objectType).c_str(),
                serializedObjectId.c_str());

        return SAI_STATUS_ITEM_NOT_FOUND;
    }

    auto &attrHash = it->second;

    auto a = std::make_shared<SaiAttrWrap>(objectType, attr);

    // set have only one attribute
    attrHash[a->getAttrMetadata()->attridname] = a;

    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchStateBase::set(
        _In_ sai_object_type_t objectType,
        _In_ sai_object_id_t objectId,
        _In_ const sai_attribute_t* attr)
{
    SWSS_LOG_ENTER();

    auto sid = sai_serialize_object_id(objectId);

    return set(objectType, sid, attr);
}

sai_status_t SwitchStateBase::get(
        _In_ sai_object_type_t object_type,
        _In_ sai_object_id_t object_id,
        _In_ uint32_t attr_count,
        _Out_ sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    auto sid = sai_serialize_object_id(object_id);

    return get(object_type, sid, attr_count, attr_list);
}

sai_status_t SwitchStateBase::get(
        _In_ sai_object_type_t objectType,
        _In_ const std::string &serializedObjectId,
        _In_ uint32_t attr_count,
        _Out_ sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    const auto &objectHash = m_objectHash.at(objectType);

    auto it = objectHash.find(serializedObjectId);

    if (it == objectHash.end())
    {
        SWSS_LOG_ERROR("not found %s:%s",
                sai_serialize_object_type(objectType).c_str(),
                serializedObjectId.c_str());

        return SAI_STATUS_ITEM_NOT_FOUND;
    }

    /*
     * We need reference here since we can potentially update attr hash for RO
     * object.
     */

    auto& attrHash = it->second;

    /*
     * Some of the list query maybe for length, so we can't do
     * normal serialize, maybe with count only.
     */

    sai_status_t final_status = SAI_STATUS_SUCCESS;

    for (uint32_t idx = 0; idx < attr_count; ++idx)
    {
        sai_attr_id_t id = attr_list[idx].id;

        auto meta = sai_metadata_get_attr_metadata(objectType, id);

        if (meta == NULL)
        {
            SWSS_LOG_ERROR("failed to find attribute %d for %s:%s", id,
                    sai_serialize_object_type(objectType).c_str(),
                    serializedObjectId.c_str());

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
            sai_deserialize_object_id(serializedObjectId, oid);

            status = refresh_read_only(meta, oid);

            if (status != SAI_STATUS_SUCCESS)
            {
                SWSS_LOG_INFO("%s read only not implemented on %s",
                        meta->attridname,
                        serializedObjectId.c_str());

                return status;
            }
        }

        auto ait = attrHash.find(meta->attridname);

        if (ait == attrHash.end())
        {
            SWSS_LOG_WARN("%s not implemented on %s",
                    meta->attridname,
                    serializedObjectId.c_str());

            return SAI_STATUS_NOT_IMPLEMENTED;
        }

        auto attr = ait->second->getAttr();

        status = transfer_attributes(objectType, 1, attr, &attr_list[idx], false);

        if (status == SAI_STATUS_BUFFER_OVERFLOW)
        {
            /*
             * This is considered partial success, since we get correct list
             * length.  Note that other items ARE processes on the list.
             */

            SWSS_LOG_NOTICE("BUFFER_OVERFLOW %s: %s",
                    serializedObjectId.c_str(),
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
                    serializedObjectId.c_str(),
                    meta->attridname,
                    sai_serialize_status(status).c_str());

            return status;
        }
    }

    return final_status;
}

sai_status_t SwitchStateBase::bulkCreate(
        _In_ sai_object_id_t switch_id,
        _In_ sai_object_type_t object_type,
        _In_ const std::vector<std::string> &serialized_object_ids,
        _In_ const uint32_t *attr_count,
        _In_ const sai_attribute_t **attr_list,
        _In_ sai_bulk_op_error_mode_t mode,
        _Out_ sai_status_t *object_statuses)
{
    SWSS_LOG_ENTER();

    uint32_t object_count = (uint32_t) serialized_object_ids.size();

    if (!object_count || !attr_count || !attr_list || !object_statuses)
    {
        SWSS_LOG_ERROR("Invalid arguments");
        return SAI_STATUS_FAILURE;
    }

    sai_status_t status = SAI_STATUS_SUCCESS;
    uint32_t it;

    for (it = 0; it < object_count; it++)
    {
        object_statuses[it] = create(object_type, serialized_object_ids[it], switch_id, attr_count[it], attr_list[it]);

        if (object_statuses[it] != SAI_STATUS_SUCCESS)
        {
            SWSS_LOG_ERROR("Failed to create object with type = %u", object_type);

            status = SAI_STATUS_FAILURE;

            if (mode == SAI_BULK_OP_ERROR_MODE_STOP_ON_ERROR)
            {
                break;
            }
        }
    }

    while (++it < object_count)
    {
        object_statuses[it] = SAI_STATUS_NOT_EXECUTED;
    }

    return status;
}

sai_status_t SwitchStateBase::bulkRemove(
        _In_ sai_object_type_t object_type,
        _In_ const std::vector<std::string> &serialized_object_ids,
        _In_ sai_bulk_op_error_mode_t mode,
        _Out_ sai_status_t *object_statuses)
{
    SWSS_LOG_ENTER();

    uint32_t object_count = (uint32_t) serialized_object_ids.size();

    if (!object_count || !object_statuses)
    {
        SWSS_LOG_ERROR("Invalid arguments");
        return SAI_STATUS_FAILURE;
    }

    sai_status_t status = SAI_STATUS_SUCCESS;
    uint32_t it;

    for (it = 0; it < object_count; it++)
    {
        object_statuses[it] = remove(object_type, serialized_object_ids[it]);

        if (object_statuses[it] != SAI_STATUS_SUCCESS)
        {
            SWSS_LOG_ERROR("Failed to remove object with type = %u", object_type);

            status = SAI_STATUS_FAILURE;

            if (mode == SAI_BULK_OP_ERROR_MODE_STOP_ON_ERROR)
            {
                break;
            }
        }
    }

    while (++it < object_count)
    {
        object_statuses[it] = SAI_STATUS_NOT_EXECUTED;
    }

    return status;
}

sai_status_t SwitchStateBase::bulkSet(
        _In_ sai_object_type_t object_type,
        _In_ const std::vector<std::string> &serialized_object_ids,
        _In_ const sai_attribute_t *attr_list,
        _In_ sai_bulk_op_error_mode_t mode,
        _Out_ sai_status_t *object_statuses)
{
    SWSS_LOG_ENTER();

    uint32_t object_count = (uint32_t) serialized_object_ids.size();

    if (!object_count || !attr_list || !object_statuses)
    {
        SWSS_LOG_ERROR("Invalid arguments");
        return SAI_STATUS_FAILURE;
    }

    sai_status_t status = SAI_STATUS_SUCCESS;
    uint32_t it;

    for (it = 0; it < object_count; it++)
    {
        object_statuses[it] = set_internal(object_type, serialized_object_ids[it], &attr_list[it]);

        if (object_statuses[it] != SAI_STATUS_SUCCESS)
        {
            SWSS_LOG_ERROR("Failed to set attribute for object with type = %u", object_type);

            status = SAI_STATUS_FAILURE;

            if (mode == SAI_BULK_OP_ERROR_MODE_STOP_ON_ERROR)
            {
                break;
            }
        }
    }

    while (++it < object_count)
    {
        object_statuses[it] = SAI_STATUS_NOT_EXECUTED;
    }

    return status;
}

sai_status_t SwitchStateBase::bulkGet(
        _In_ sai_object_type_t object_type,
        _In_ const std::vector<std::string> &serialized_object_ids,
        _In_ const uint32_t *attr_count,
        _Inout_ sai_attribute_t **attr_list,
        _In_ sai_bulk_op_error_mode_t mode,
        _Out_ sai_status_t *object_statuses)
{
    SWSS_LOG_ENTER();

    uint32_t it;
    uint32_t object_count = (uint32_t) serialized_object_ids.size();
    sai_status_t status = SAI_STATUS_SUCCESS;

    if (!object_count || !attr_list || !attr_count || !object_statuses)
    {
        SWSS_LOG_ERROR("Invalid arguments");
        return SAI_STATUS_FAILURE;
    }

    for (it = 0; it < object_count; it++)
    {
        if (!attr_list[it] || !attr_count[it])
        {
            SWSS_LOG_ERROR("Invalid arguments");
            return SAI_STATUS_FAILURE;
        }
    }

    for (it = 0; it < object_count; it++)
    {
        object_statuses[it] = get(object_type, serialized_object_ids[it], attr_count[it], attr_list[it]);

        if (object_statuses[it] != SAI_STATUS_SUCCESS)
        {
            SWSS_LOG_ERROR("Failed to get attribute for object with type = %u", object_type);

            status = SAI_STATUS_FAILURE;

            if (mode == SAI_BULK_OP_ERROR_MODE_STOP_ON_ERROR)
            {
                break;
            }
        }
    }

    while (++it < object_count)
    {
        object_statuses[it] = SAI_STATUS_NOT_EXECUTED;
    }

    return status;
}

int SwitchStateBase::get_default_gw_mac_address(
        _Out_ sai_mac_t& mac)
{
    SWSS_LOG_ENTER();

    auto file = std::ifstream("/proc/net/route");

    if (!file)
    {
        return -1;
    }

    std::string buf;

    while (std::getline(file, buf))
    {
        char iface[IF_NAMESIZE];

        long destination, gateway;

        if (std::sscanf(buf.c_str(), "%s %lx %lx", iface, &destination, &gateway) == 3)
        {
            if (destination == 0)
            {
                file = std::ifstream("/sys/class/net/" + std::string(iface) + "/address");

                if ( !file )
                {
                    return -1;
                }

                file >> buf;

                return std::sscanf(buf.c_str(), "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx", &mac[0], &mac[1], &mac[2], &mac[3], &mac[4], &mac[5]) == 6 ? 0 : -1;
            }
        }
    }

    return -1;
}

sai_status_t SwitchStateBase::set_switch_mac_address()
{
    SWSS_LOG_ENTER();

    SWSS_LOG_INFO("create switch src mac address");

    sai_attribute_t attr;

    attr.id = SAI_SWITCH_ATTR_SRC_MAC_ADDRESS;

    if (get_default_gw_mac_address(attr.value.mac) < 0)
    {
        attr.value.mac[0] = 0x22;
        attr.value.mac[1] = 0x33;
        attr.value.mac[2] = 0x44;
        attr.value.mac[3] = 0x55;
        attr.value.mac[4] = 0x66;
        attr.value.mac[5] = 0x77;
    }

    return set(SAI_OBJECT_TYPE_SWITCH, m_switch_id, &attr);
}

sai_status_t SwitchStateBase::set_vxlan_default_router_mac()
{
    SWSS_LOG_ENTER();

    SWSS_LOG_INFO("create switch vxlan default router mac address");

    sai_attribute_t attr;

    attr.id = SAI_SWITCH_ATTR_VXLAN_DEFAULT_ROUTER_MAC;

    // NOTE if there is default vxlan mac present like in case of get_default_gw_mac_address
    // then that mac should be used
    {
        attr.value.mac[0] = 0x12;
        attr.value.mac[1] = 0x23;
        attr.value.mac[2] = 0x34;
        attr.value.mac[3] = 0x45;
        attr.value.mac[4] = 0x56;
        attr.value.mac[5] = 0x67;
    }

    return set(SAI_OBJECT_TYPE_SWITCH, m_switch_id, &attr);
}

sai_status_t SwitchStateBase::set_switch_supported_object_types()
{
    SWSS_LOG_ENTER();

    sai_attribute_t attr;

    // Fill this with supported SAI_OBJECT_TYPEs
    sai_object_type_t supported_obj_list[] = {
        SAI_OBJECT_TYPE_PORT,
        SAI_OBJECT_TYPE_LAG,
        SAI_OBJECT_TYPE_TAM,
        SAI_OBJECT_TYPE_TAM_COLLECTOR,
        SAI_OBJECT_TYPE_TAM_REPORT,
        SAI_OBJECT_TYPE_TAM_TRANSPORT,
        SAI_OBJECT_TYPE_TAM_TELEMETRY,
        SAI_OBJECT_TYPE_TAM_EVENT_THRESHOLD
    };

    attr.id = SAI_SWITCH_ATTR_SUPPORTED_OBJECT_TYPE_LIST;
    attr.value.s32list.count = sizeof(supported_obj_list)/sizeof(sai_object_type_t);
    attr.value.s32list.list = (int32_t *) supported_obj_list;

    return set(SAI_OBJECT_TYPE_SWITCH, m_switch_id, &attr);
}

sai_status_t SwitchStateBase::set_switch_default_attributes()
{
    SWSS_LOG_ENTER();

    SWSS_LOG_INFO("create switch default attributes");

    sai_attribute_t attr;

    attr.id = SAI_SWITCH_ATTR_PORT_STATE_CHANGE_NOTIFY;
    attr.value.ptr = NULL;

    CHECK_STATUS(set(SAI_OBJECT_TYPE_SWITCH, m_switch_id, &attr));

    attr.id = SAI_SWITCH_ATTR_FDB_EVENT_NOTIFY;

    CHECK_STATUS(set(SAI_OBJECT_TYPE_SWITCH, m_switch_id, &attr));

    attr.id = SAI_SWITCH_ATTR_BFD_SESSION_STATE_CHANGE_NOTIFY;

    CHECK_STATUS(set(SAI_OBJECT_TYPE_SWITCH, m_switch_id, &attr));

    attr.id = SAI_SWITCH_ATTR_TAM_TEL_TYPE_CONFIG_CHANGE_NOTIFY;

    CHECK_STATUS(set(SAI_OBJECT_TYPE_SWITCH, m_switch_id, &attr));

    attr.id = SAI_SWITCH_ATTR_FDB_AGING_TIME;
    attr.value.u32 = 0;

    CHECK_STATUS(set(SAI_OBJECT_TYPE_SWITCH, m_switch_id, &attr));

    attr.id = SAI_SWITCH_ATTR_RESTART_WARM;
    attr.value.booldata = false;

    CHECK_STATUS(set(SAI_OBJECT_TYPE_SWITCH, m_switch_id, &attr));

    attr.id = SAI_SWITCH_ATTR_TYPE;
    attr.value.s32 = SAI_SWITCH_TYPE_NPU;

    CHECK_STATUS(set(SAI_OBJECT_TYPE_SWITCH, m_switch_id, &attr));

    attr.id = SAI_SWITCH_ATTR_WARM_RECOVER;
    attr.value.booldata = false;

    CHECK_STATUS(set(SAI_OBJECT_TYPE_SWITCH, m_switch_id, &attr));

    attr.id = SAI_SWITCH_ATTR_SUPPORTED_IPV4_BFD_SESSION_OFFLOAD_TYPE;
    uint32_t list[1] = { SAI_BFD_SESSION_OFFLOAD_TYPE_FULL };

    if(!m_switchConfig->m_bfdOffload) {
        list[0] = SAI_BFD_SESSION_OFFLOAD_TYPE_NONE;
    }

    attr.value.u32list.count = 1;
    attr.value.u32list.list = list;

    CHECK_STATUS(set(SAI_OBJECT_TYPE_SWITCH, m_switch_id, &attr));

    attr.id = SAI_SWITCH_ATTR_SUPPORTED_IPV6_BFD_SESSION_OFFLOAD_TYPE;
    attr.value.u32list.count = 1;
    attr.value.u32list.list = list;

    CHECK_STATUS(set(SAI_OBJECT_TYPE_SWITCH, m_switch_id, &attr));

    return set_switch_supported_object_types();
}

sai_status_t SwitchStateBase::create_default_hash()
{
    SWSS_LOG_ENTER();

    SWSS_LOG_INFO("create default hash");

    // Hash defaults according to SAI headers
    std::vector<sai_native_hash_field_t> hfList = {
        SAI_NATIVE_HASH_FIELD_DST_MAC,
        SAI_NATIVE_HASH_FIELD_SRC_MAC,
        SAI_NATIVE_HASH_FIELD_ETHERTYPE,
        SAI_NATIVE_HASH_FIELD_IN_PORT
    };

    // create and populate default ecmp hash object
    sai_attribute_t attr;
    attr.id = SAI_HASH_ATTR_NATIVE_HASH_FIELD_LIST;
    attr.value.s32list.list = reinterpret_cast<sai_int32_t*>(hfList.data());
    attr.value.s32list.count = static_cast<sai_uint32_t>(hfList.size());

    CHECK_STATUS(create(SAI_OBJECT_TYPE_HASH, &m_ecmp_hash_id, m_switch_id, 1, &attr));

    // set default ecmp hash on switch
    attr.id = SAI_SWITCH_ATTR_ECMP_HASH;
    attr.value.oid = m_ecmp_hash_id;

    CHECK_STATUS(set(SAI_OBJECT_TYPE_SWITCH, m_switch_id, &attr));

    // create and populate default lag hash object
    attr.id = SAI_HASH_ATTR_NATIVE_HASH_FIELD_LIST;
    attr.value.s32list.list = reinterpret_cast<sai_int32_t*>(hfList.data());
    attr.value.s32list.count = static_cast<sai_uint32_t>(hfList.size());

    CHECK_STATUS(create(SAI_OBJECT_TYPE_HASH, &m_lag_hash_id, m_switch_id, 1, &attr));

    // set default lag hash on switch
    attr.id = SAI_SWITCH_ATTR_LAG_HASH;
    attr.value.oid = m_lag_hash_id;

    return set(SAI_OBJECT_TYPE_SWITCH, m_switch_id, &attr);
}

sai_status_t SwitchStateBase::set_static_crm_values()
{
    SWSS_LOG_ENTER();

    std::map<sai_switch_attr_t, int> crm_resource_lookup = {
        { SAI_SWITCH_ATTR_AVAILABLE_IPV4_ROUTE_ENTRY, m_maxIPv4RouteEntries },
        { SAI_SWITCH_ATTR_AVAILABLE_IPV6_ROUTE_ENTRY, m_maxIPv6RouteEntries },
        { SAI_SWITCH_ATTR_AVAILABLE_IPV4_NEXTHOP_ENTRY, m_maxIPv4NextHopEntries },
        { SAI_SWITCH_ATTR_AVAILABLE_IPV6_NEXTHOP_ENTRY, m_maxIPv6NextHopEntries },
        { SAI_SWITCH_ATTR_AVAILABLE_IPV4_NEIGHBOR_ENTRY, m_maxIPv4NeighborEntries },
        { SAI_SWITCH_ATTR_AVAILABLE_IPV6_NEIGHBOR_ENTRY, m_maxIPv6NeighborEntries },
        { SAI_SWITCH_ATTR_AVAILABLE_NEXT_HOP_GROUP_MEMBER_ENTRY, m_maxNextHopGroupMemberEntries },
        { SAI_SWITCH_ATTR_AVAILABLE_NEXT_HOP_GROUP_ENTRY, m_maxNextHopGroupEntries },
        { SAI_SWITCH_ATTR_AVAILABLE_FDB_ENTRY, m_maxFdbEntries },
        { SAI_SWITCH_ATTR_AVAILABLE_SNAT_ENTRY, m_maxSNATEntries },
        { SAI_SWITCH_ATTR_AVAILABLE_DNAT_ENTRY, m_maxDNATEntries },
        { SAI_SWITCH_ATTR_AVAILABLE_IPMC_ENTRY, m_maxIPMCEntries },
        { SAI_SWITCH_ATTR_AVAILABLE_DOUBLE_NAT_ENTRY, m_maxDoubleNATEntries }
    };

    for (auto const &resource: crm_resource_lookup)
    {
        sai_attribute_t attr;
        attr.id = resource.first;
        attr.value.u32 = resource.second;
        CHECK_STATUS(set(SAI_OBJECT_TYPE_SWITCH, m_switch_id, &attr));
    }

    CHECK_STATUS(set_static_acl_resource_list(SAI_SWITCH_ATTR_AVAILABLE_ACL_TABLE, m_maxAclTables));

    return set_static_acl_resource_list(SAI_SWITCH_ATTR_AVAILABLE_ACL_TABLE_GROUP, m_maxAclTableGroups);
}

sai_status_t SwitchStateBase::set_static_acl_resource_list(
        _In_ sai_switch_attr_t acl_resource,
        _In_ int max_count)
{
    SWSS_LOG_ENTER();

    auto acl_stages = {SAI_ACL_STAGE_INGRESS, SAI_ACL_STAGE_EGRESS};
    auto acl_bind_points = {
        SAI_ACL_BIND_POINT_TYPE_PORT,
        SAI_ACL_BIND_POINT_TYPE_LAG,
        SAI_ACL_BIND_POINT_TYPE_VLAN,
        SAI_ACL_BIND_POINT_TYPE_ROUTER_INTERFACE,
        SAI_ACL_BIND_POINT_TYPE_SWITCH
    };

    std::vector<sai_acl_resource_t> acl_resource_list;
    for (auto stage: acl_stages)
    {
        for (auto bp: acl_bind_points)
        {
            sai_acl_resource_t acl_resource_count;
            acl_resource_count.stage = stage;
            acl_resource_count.bind_point = bp;
            acl_resource_count.avail_num = max_count;

            acl_resource_list.push_back(acl_resource_count);
        }
    }

    sai_acl_resource_list_t available_acl_resources;
    available_acl_resources.count = static_cast<uint32_t>(acl_resource_list.size());
    available_acl_resources.list = acl_resource_list.data();

    sai_attribute_t attr;
    attr.id = acl_resource;
    attr.value.aclresource = available_acl_resources;

    return set(SAI_OBJECT_TYPE_SWITCH, m_switch_id, &attr);
}

sai_status_t SwitchStateBase::create_default_vlan()
{
    SWSS_LOG_ENTER();

    SWSS_LOG_INFO("create default vlan");

    sai_attribute_t attr;

    attr.id = SAI_VLAN_ATTR_VLAN_ID;
    attr.value.u16 = DEFAULT_VLAN_NUMBER;

    CHECK_STATUS(create(SAI_OBJECT_TYPE_VLAN, &m_default_vlan_id, m_switch_id, 1, &attr));

    /* set default vlan on switch */

    attr.id = SAI_SWITCH_ATTR_DEFAULT_VLAN_ID;
    attr.value.oid = m_default_vlan_id;

    return set(SAI_OBJECT_TYPE_SWITCH, m_switch_id, &attr);
}

sai_status_t SwitchStateBase::create_cpu_port()
{
    SWSS_LOG_ENTER();

    SWSS_LOG_INFO("create cpu port");

    sai_attribute_t attr;

    CHECK_STATUS(create(SAI_OBJECT_TYPE_PORT, &m_cpu_port_id, m_switch_id, 0, &attr));

    // populate cpu port object on switch
    attr.id = SAI_SWITCH_ATTR_CPU_PORT;
    attr.value.oid = m_cpu_port_id;

    CHECK_STATUS(set(SAI_OBJECT_TYPE_SWITCH, m_switch_id, &attr));

    // set type on cpu
    attr.id = SAI_PORT_ATTR_TYPE;
    attr.value.s32 = SAI_PORT_TYPE_CPU;

    return set(SAI_OBJECT_TYPE_PORT, m_cpu_port_id, &attr);
}

sai_status_t SwitchStateBase::create_default_1q_bridge()
{
    SWSS_LOG_ENTER();

    SWSS_LOG_INFO("create default 1q bridge");

    sai_attribute_t attr;

    attr.id = SAI_BRIDGE_ATTR_TYPE;
    attr.value.s32 = SAI_BRIDGE_TYPE_1Q;

    CHECK_STATUS(create(SAI_OBJECT_TYPE_BRIDGE, &m_default_1q_bridge, m_switch_id, 1, &attr));

    attr.id = SAI_SWITCH_ATTR_DEFAULT_1Q_BRIDGE_ID;
    attr.value.oid = m_default_1q_bridge;

    return set(SAI_OBJECT_TYPE_SWITCH, m_switch_id, &attr);
}

sai_status_t SwitchStateBase::create_ports()
{
    SWSS_LOG_ENTER();

    SWSS_LOG_INFO("create ports");

    auto map = m_switchConfig->m_laneMap;

    if (!map)
    {
        SWSS_LOG_ERROR("lane map for switch %s is NULL",
                sai_serialize_object_id(m_switch_id).c_str());

        return SAI_STATUS_FAILURE;
    }

    auto lanesVector = map->getLaneVector();

    if (m_switchConfig->m_useTapDevice)
    {
        SWSS_LOG_DEBUG("Check available lane");

        CHECK_STATUS(filter_available_lanes(lanesVector));
    }

    uint32_t port_count = (uint32_t)lanesVector.size();

    m_port_list.clear();

    for (uint32_t i = 0; i < port_count; i++)
    {
        SWSS_LOG_DEBUG("create port index %u", i);

        sai_object_id_t port_id;

        CHECK_STATUS(create(SAI_OBJECT_TYPE_PORT, &port_id, m_switch_id, 0, NULL));
        m_port_list.push_back(port_id);

        sai_attribute_t attr;

        attr.id = SAI_PORT_ATTR_ADMIN_STATE;
        attr.value.booldata = false;     /* default admin state is down as defined in SAI */

        CHECK_STATUS(set(SAI_OBJECT_TYPE_PORT, port_id, &attr));

        attr.id = SAI_PORT_ATTR_MTU;
        attr.value.u32 = 1514;     /* default MTU is 1514 as defined in SAI */

        CHECK_STATUS(set(SAI_OBJECT_TYPE_PORT, port_id, &attr));

        attr.id = SAI_PORT_ATTR_SPEED;
        attr.value.u32 = 40 * 1000;     // TODO from config

        CHECK_STATUS(set(SAI_OBJECT_TYPE_PORT, port_id, &attr));

        std::vector<uint32_t> lanes = lanesVector.at(i);

        attr.id = SAI_PORT_ATTR_HW_LANE_LIST;
        attr.value.u32list.count = (uint32_t)lanes.size();
        attr.value.u32list.list = lanes.data();

        CHECK_STATUS(set(SAI_OBJECT_TYPE_PORT, port_id, &attr));

        attr.id = SAI_PORT_ATTR_TYPE;
        attr.value.s32 = SAI_PORT_TYPE_LOGICAL;

        CHECK_STATUS(set(SAI_OBJECT_TYPE_PORT, port_id, &attr));

        attr.id = SAI_PORT_ATTR_OPER_STATUS;
        attr.value.s32 = SAI_PORT_OPER_STATUS_DOWN;

        CHECK_STATUS(set(SAI_OBJECT_TYPE_PORT, port_id, &attr));

        attr.id = SAI_PORT_ATTR_PORT_VLAN_ID;
        attr.value.u32 = DEFAULT_VLAN_NUMBER;

        CHECK_STATUS(set(SAI_OBJECT_TYPE_PORT, port_id, &attr));

        attr.id = SAI_PORT_ATTR_HOST_TX_READY_STATUS;
        attr.value.u32 = SAI_PORT_HOST_TX_READY_STATUS_READY;

        CHECK_STATUS(set(SAI_OBJECT_TYPE_PORT, port_id, &attr));

        attr.id = SAI_PORT_ATTR_AUTO_NEG_MODE;
        attr.value.booldata = true;

        CHECK_STATUS(set(SAI_OBJECT_TYPE_PORT, port_id, &attr));
    }

    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchStateBase::set_port_list()
{
    SWSS_LOG_ENTER();

    SWSS_LOG_INFO("set port list");

    // NOTE: this is static, but will be refreshed on read only get

    sai_attribute_t attr;

    uint32_t port_count = (uint32_t)m_port_list.size();

    attr.id = SAI_SWITCH_ATTR_PORT_LIST;
    attr.value.objlist.count = port_count;
    attr.value.objlist.list = m_port_list.data();

    CHECK_STATUS(set(SAI_OBJECT_TYPE_SWITCH, m_switch_id, &attr));

    attr.id = SAI_SWITCH_ATTR_PORT_NUMBER;
    attr.value.u32 = port_count;

    return set(SAI_OBJECT_TYPE_SWITCH, m_switch_id, &attr);
}

sai_status_t SwitchStateBase::set_port_capabilities()
{
    SWSS_LOG_ENTER();

    SWSS_LOG_NOTICE("set port capabilities");

    sai_attribute_t attr;

    for (auto &port_id: m_port_list)
    {
        attr.id = SAI_PORT_ATTR_SUPPORTED_AUTO_NEG_MODE;
        attr.value.booldata = true;
        CHECK_STATUS(set(SAI_OBJECT_TYPE_PORT, port_id, &attr));
    }
    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchStateBase::create_default_virtual_router()
{
    SWSS_LOG_ENTER();

    SWSS_LOG_INFO("create default virtual router");

    sai_object_id_t virtual_router_id;

    CHECK_STATUS(create(SAI_OBJECT_TYPE_VIRTUAL_ROUTER, &virtual_router_id, m_switch_id, 0, NULL));

    sai_attribute_t attr;

    attr.id = SAI_SWITCH_ATTR_DEFAULT_VIRTUAL_ROUTER_ID;
    attr.value.oid = virtual_router_id;

    return set(SAI_OBJECT_TYPE_SWITCH, m_switch_id, &attr);
}

sai_status_t SwitchStateBase::create_default_stp_instance()
{
    SWSS_LOG_ENTER();

    SWSS_LOG_INFO("create default stp instance");

    sai_object_id_t stp_instance_id;

    CHECK_STATUS(create(SAI_OBJECT_TYPE_STP, &stp_instance_id, m_switch_id, 0, NULL));

    sai_attribute_t attr;

    attr.id = SAI_SWITCH_ATTR_DEFAULT_STP_INST_ID;
    attr.value.oid = stp_instance_id;

    return set(SAI_OBJECT_TYPE_SWITCH, m_switch_id, &attr);
}

sai_status_t SwitchStateBase::create_default_trap_group()
{
    SWSS_LOG_ENTER();

    SWSS_LOG_INFO("create default trap group");

    sai_object_id_t trap_group_id;

    CHECK_STATUS(create(SAI_OBJECT_TYPE_HOSTIF_TRAP_GROUP, &trap_group_id, m_switch_id, 0, NULL));

    sai_attribute_t attr;

    attr.id = SAI_SWITCH_ATTR_DEFAULT_TRAP_GROUP;
    attr.value.oid = trap_group_id;

    return set(SAI_OBJECT_TYPE_SWITCH, m_switch_id, &attr);
}

sai_status_t SwitchStateBase::create_ingress_priority_groups_per_port(
        _In_ sai_object_id_t port_id)
{
    SWSS_LOG_ENTER();

    const uint32_t port_pgs_count = 8; // NOTE: must be per switch (mlnx and brcm is 8)

    std::vector<sai_object_id_t> pgs;

    for (uint32_t i = 0; i < port_pgs_count; ++i)
    {
        sai_object_id_t pg_id;

        sai_attribute_t attr[3];

        // NOTE: on brcm this attribute is not added

        attr[0].id = SAI_INGRESS_PRIORITY_GROUP_ATTR_BUFFER_PROFILE;
        attr[0].value.oid = SAI_NULL_OBJECT_ID;

        attr[1].id = SAI_INGRESS_PRIORITY_GROUP_ATTR_PORT;
        attr[1].value.oid = port_id;

        attr[2].id = SAI_INGRESS_PRIORITY_GROUP_ATTR_INDEX;
        attr[2].value.oid = i;

        CHECK_STATUS(create(SAI_OBJECT_TYPE_INGRESS_PRIORITY_GROUP, &pg_id, m_switch_id, 3, attr));

        pgs.push_back(pg_id);
    }

    sai_attribute_t attr;

    attr.id = SAI_PORT_ATTR_NUMBER_OF_INGRESS_PRIORITY_GROUPS;
    attr.value.u32 = port_pgs_count;

    CHECK_STATUS(set(SAI_OBJECT_TYPE_PORT, port_id, &attr));

    attr.id = SAI_PORT_ATTR_INGRESS_PRIORITY_GROUP_LIST;
    attr.value.objlist.count = port_pgs_count;
    attr.value.objlist.list = pgs.data();

    CHECK_STATUS(set(SAI_OBJECT_TYPE_PORT, port_id, &attr));

    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchStateBase::create_ingress_priority_groups()
{
    SWSS_LOG_ENTER();

    // XXX priority groups size may change when we will modify pg or ports

    SWSS_LOG_INFO("create ingress priority groups");

    for (auto &port_id: m_port_list)
    {
        create_ingress_priority_groups_per_port(port_id);
    }

    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchStateBase::create_vlan_members()
{
    SWSS_LOG_ENTER();

    // Crete vlan members for bridge ports.

    for (auto bridge_port_id: m_bridge_port_list_port_based)
    {
        SWSS_LOG_DEBUG("create vlan member for bridge port %s",
                sai_serialize_object_id(bridge_port_id).c_str());

        sai_attribute_t attrs[3];

        attrs[0].id = SAI_VLAN_MEMBER_ATTR_BRIDGE_PORT_ID;
        attrs[0].value.oid = bridge_port_id;

        attrs[1].id = SAI_VLAN_MEMBER_ATTR_VLAN_ID;
        attrs[1].value.oid = m_default_vlan_id;

        attrs[2].id = SAI_VLAN_MEMBER_ATTR_VLAN_TAGGING_MODE;
        attrs[2].value.s32 = SAI_VLAN_TAGGING_MODE_UNTAGGED;

        sai_object_id_t vlan_member_id;

        CHECK_STATUS(create(SAI_OBJECT_TYPE_VLAN_MEMBER, &vlan_member_id, m_switch_id, 3, attrs));
    }

    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchStateBase::create_bridge_ports()
{
    SWSS_LOG_ENTER();

    // Create bridge port for 1q router.

    sai_attribute_t attr;

    attr.id = SAI_BRIDGE_PORT_ATTR_TYPE;
    attr.value.s32 = SAI_BRIDGE_PORT_TYPE_1Q_ROUTER;

    CHECK_STATUS(create(SAI_OBJECT_TYPE_BRIDGE_PORT, &m_default_bridge_port_1q_router, m_switch_id, 1, &attr));

    attr.id = SAI_BRIDGE_PORT_ATTR_PORT_ID;
    attr.value.oid = SAI_NULL_OBJECT_ID;

    CHECK_STATUS(set(SAI_OBJECT_TYPE_BRIDGE_PORT, m_default_bridge_port_1q_router, &attr));

    // Create bridge ports for regular ports.

    m_bridge_port_list_port_based.clear();

    for (const auto &port_id: m_port_list)
    {
        SWSS_LOG_DEBUG("create bridge port for port %s", sai_serialize_object_id(port_id).c_str());

        sai_attribute_t attrs[4];

        attrs[0].id = SAI_BRIDGE_PORT_ATTR_BRIDGE_ID;
        attrs[0].value.oid = m_default_1q_bridge;

        attrs[1].id = SAI_BRIDGE_PORT_ATTR_FDB_LEARNING_MODE;
        attrs[1].value.s32 = SAI_BRIDGE_PORT_FDB_LEARNING_MODE_HW;

        attrs[2].id = SAI_BRIDGE_PORT_ATTR_PORT_ID;
        attrs[2].value.oid = port_id;

        attrs[3].id = SAI_BRIDGE_PORT_ATTR_TYPE;
        attrs[3].value.s32 = SAI_BRIDGE_PORT_TYPE_PORT;

        sai_object_id_t bridge_port_id;

        CHECK_STATUS(create(SAI_OBJECT_TYPE_BRIDGE_PORT, &bridge_port_id, m_switch_id, 4, attrs));

        m_bridge_port_list_port_based.push_back(bridge_port_id);
    }

    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchStateBase::set_acl_entry_min_prio()
{
    SWSS_LOG_ENTER();

    SWSS_LOG_INFO("set acl entry min prio");

    sai_attribute_t attr;

    attr.id = SAI_SWITCH_ATTR_ACL_ENTRY_MINIMUM_PRIORITY;
    attr.value.u32 = 1;

    CHECK_STATUS(set(SAI_OBJECT_TYPE_SWITCH, m_switch_id, &attr));

    attr.id = SAI_SWITCH_ATTR_ACL_ENTRY_MAXIMUM_PRIORITY;
    attr.value.u32 = 16000;

    return set(SAI_OBJECT_TYPE_SWITCH, m_switch_id, &attr);
}

sai_status_t SwitchStateBase::set_acl_capabilities()
{
    SWSS_LOG_ENTER();

    SWSS_LOG_INFO("set acl capabilities");

    sai_attribute_t attr;

    m_ingress_acl_action_list.clear();
    m_egress_acl_action_list.clear();

    for (int action_type = SAI_ACL_ENTRY_ATTR_ACTION_START; action_type <= SAI_ACL_ENTRY_ATTR_ACTION_END; action_type++)
    {
        m_ingress_acl_action_list.push_back(static_cast<sai_acl_action_type_t>(action_type - SAI_ACL_ENTRY_ATTR_ACTION_START));
        m_egress_acl_action_list.push_back(static_cast<sai_acl_action_type_t>(action_type - SAI_ACL_ENTRY_ATTR_ACTION_START));
    }

    attr.id = SAI_SWITCH_ATTR_MAX_ACL_ACTION_COUNT;
    attr.value.u32 = static_cast<uint32_t>(std::max(m_ingress_acl_action_list.size(), m_egress_acl_action_list.size()));

    CHECK_STATUS(set(SAI_OBJECT_TYPE_SWITCH, m_switch_id, &attr));

    attr.id = SAI_SWITCH_ATTR_ACL_STAGE_INGRESS;
    attr.value.aclcapability.is_action_list_mandatory = false;
    attr.value.aclcapability.action_list.list = reinterpret_cast<int32_t*>(m_ingress_acl_action_list.data());
    attr.value.aclcapability.action_list.count = static_cast<uint32_t>(m_ingress_acl_action_list.size());

    CHECK_STATUS(set(SAI_OBJECT_TYPE_SWITCH, m_switch_id, &attr));

    attr.id = SAI_SWITCH_ATTR_ACL_STAGE_EGRESS;
    attr.value.aclcapability.is_action_list_mandatory = false;
    attr.value.aclcapability.action_list.list = reinterpret_cast<int32_t*>(m_egress_acl_action_list.data());
    attr.value.aclcapability.action_list.count = static_cast<uint32_t>(m_egress_acl_action_list.size());

    return set(SAI_OBJECT_TYPE_SWITCH, m_switch_id, &attr);
}

sai_status_t SwitchStateBase::create_cpu_qos_queues(
        _In_ sai_object_id_t port_id)
{
    SWSS_LOG_ENTER();

    SWSS_LOG_ERROR("implement in child class");

    return SAI_STATUS_NOT_IMPLEMENTED;
}

sai_status_t SwitchStateBase::create_qos_queues_per_port(
        _In_ sai_object_id_t port_id)
{
    SWSS_LOG_ENTER();

    SWSS_LOG_ERROR("implement in child class");

    return SAI_STATUS_NOT_IMPLEMENTED;
}

sai_status_t SwitchStateBase::create_qos_queues()
{
    SWSS_LOG_ENTER();

    SWSS_LOG_ERROR("implement in child class");

    return SAI_STATUS_NOT_IMPLEMENTED;
}

sai_status_t SwitchStateBase::create_scheduler_group_tree(
        _In_ const std::vector<sai_object_id_t>& sgs,
        _In_ sai_object_id_t port_id)
{
    SWSS_LOG_ENTER();

    SWSS_LOG_ERROR("implement in child class");

    return SAI_STATUS_NOT_IMPLEMENTED;
}

sai_status_t SwitchStateBase::create_scheduler_groups_per_port(
        _In_ sai_object_id_t port_id)
{
    SWSS_LOG_ENTER();

    SWSS_LOG_ERROR("implement in child class");

    return SAI_STATUS_NOT_IMPLEMENTED;
}

sai_status_t SwitchStateBase::create_scheduler_groups()
{
    SWSS_LOG_ENTER();

    SWSS_LOG_INFO("create scheduler groups");

    // XXX scheduler groups size may change when we will modify sg or ports

    for (auto &port_id: m_port_list)
    {
        CHECK_STATUS(create_scheduler_groups_per_port(port_id));
    }

    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchStateBase::set_maximum_number_of_childs_per_scheduler_group()
{
    SWSS_LOG_ENTER();

    SWSS_LOG_ERROR("implement in child class");

    return SAI_STATUS_NOT_IMPLEMENTED;
}

sai_status_t SwitchStateBase::set_number_of_ecmp_groups()
{
    SWSS_LOG_ENTER();

    SWSS_LOG_INFO("set number of ecmp groups");

    sai_attribute_t attr;

    attr.id = SAI_SWITCH_ATTR_NUMBER_OF_ECMP_GROUPS;
    attr.value.u32 = 512;

    return set(SAI_OBJECT_TYPE_SWITCH, m_switch_id, &attr);
}

sai_status_t SwitchStateBase::create_port_serdes()
{
    SWSS_LOG_ENTER();

    SWSS_LOG_ERROR("implement in child class");

    return SAI_STATUS_NOT_IMPLEMENTED;
}

sai_status_t SwitchStateBase::create_port_serdes_per_port(
        _In_ sai_object_id_t port_id)
{
    SWSS_LOG_ENTER();

    SWSS_LOG_ERROR("implement in child class");

    return SAI_STATUS_NOT_IMPLEMENTED;
}

sai_status_t SwitchStateBase::initialize_default_objects(
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    CHECK_STATUS(set_switch_mac_address());
    CHECK_STATUS(set_vxlan_default_router_mac());
    CHECK_STATUS(create_cpu_port());
    CHECK_STATUS(create_default_hash());
    CHECK_STATUS(create_default_vlan());
    CHECK_STATUS(create_default_virtual_router());
    CHECK_STATUS(create_default_stp_instance());
    CHECK_STATUS(create_default_1q_bridge());
    CHECK_STATUS(create_default_trap_group());
    CHECK_STATUS(create_ports());
    CHECK_STATUS(create_port_serdes());
    CHECK_STATUS(set_port_list());
    CHECK_STATUS(set_port_capabilities());
    CHECK_STATUS(create_bridge_ports());
    CHECK_STATUS(create_vlan_members());
    CHECK_STATUS(set_acl_entry_min_prio());
    CHECK_STATUS(set_acl_capabilities());
    CHECK_STATUS(create_ingress_priority_groups());
    CHECK_STATUS(create_qos_queues());
    CHECK_STATUS(set_maximum_number_of_childs_per_scheduler_group());
    CHECK_STATUS(set_number_of_ecmp_groups());
    CHECK_STATUS(set_switch_default_attributes());
    CHECK_STATUS(create_scheduler_groups());
    CHECK_STATUS(set_static_crm_values());

    // Initialize switch for VOQ attributes

    CHECK_STATUS(initialize_voq_switch_objects(attr_count, attr_list));

    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchStateBase::create_port_dependencies(
        _In_ sai_object_id_t port_id)
{
    SWSS_LOG_ENTER();

    // params are not needed since they were already set

    SWSS_LOG_WARN("check attributes and set, FIXME");

    // this method is post create action on generic create object

    sai_attribute_t attr;

    // default admin state is down as defined in SAI

    attr.id = SAI_PORT_ATTR_ADMIN_STATE;
    attr.value.booldata = false;

    CHECK_STATUS(set(SAI_OBJECT_TYPE_PORT, port_id, &attr));

    attr.id = SAI_PORT_ATTR_HOST_TX_READY_STATUS;
    attr.value.u32 = SAI_PORT_HOST_TX_READY_STATUS_READY;

    CHECK_STATUS(set(SAI_OBJECT_TYPE_PORT, port_id, &attr));

    attr.id = SAI_PORT_ATTR_AUTO_NEG_MODE;
    attr.value.booldata = true;

    CHECK_STATUS(set(SAI_OBJECT_TYPE_PORT, port_id, &attr));

    // attributes are not required since they will be set outside this function

    CHECK_STATUS(create_ingress_priority_groups_per_port(port_id));
    CHECK_STATUS(create_qos_queues_per_port(port_id));
    CHECK_STATUS(create_scheduler_groups_per_port(port_id));
    CHECK_STATUS(create_port_serdes_per_port(port_id));

    // XXX should bridge ports should also be created when new port is created?
    // this needs to be checked on real ASIC and updated here

    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchStateBase::refresh_ingress_priority_group(
        _In_ const sai_attr_metadata_t *meta,
        _In_ sai_object_id_t port_id)
{
    SWSS_LOG_ENTER();

    /*
     * TODO Currently we don't have index in groups, so we don't know how to
     * sort.  Returning success, since assuming that we will not create more
     * ingress priority groups.
     */

    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchStateBase::refresh_qos_queues(
        _In_ const sai_attr_metadata_t *meta,
        _In_ sai_object_id_t port_id)
{
    SWSS_LOG_ENTER();

    /*
     * TODO Currently we don't have index in groups, so we don't know how to
     * sort.  Returning success, since assuming that we will not create more
     * ingress priority groups.
     */

    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchStateBase::refresh_scheduler_groups(
        _In_ const sai_attr_metadata_t *meta,
        _In_ sai_object_id_t port_id)
{
    SWSS_LOG_ENTER();

    /*
     * TODO Currently we don't have index in groups, so we don't know how to
     * sort.  Returning success, since assuming that we will not create more
     * ingress priority groups.
     */

    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchStateBase::warm_boot_initialize_objects()
{
    SWSS_LOG_ENTER();

    SWSS_LOG_INFO("warm boot init objects");

    /*
     * We need to bring back previous state in case user will get some read
     * only attributes and recalculation will need to be done.
     *
     * We need to refresh:
     * - ports
     * - default bridge port 1q router
     */

    m_port_list.resize(SAI_VS_MAX_PORTS);

    sai_attribute_t attr;

    attr.id = SAI_SWITCH_ATTR_PORT_LIST;

    attr.value.objlist.count = SAI_VS_MAX_PORTS;
    attr.value.objlist.list = m_port_list.data();

    CHECK_STATUS(get(SAI_OBJECT_TYPE_SWITCH, m_switch_id, 1, &attr));

    m_port_list.resize(attr.value.objlist.count);

    SWSS_LOG_NOTICE("port list size: %zu", m_port_list.size());

    attr.id = SAI_SWITCH_ATTR_DEFAULT_1Q_BRIDGE_ID;

    CHECK_STATUS(get(SAI_OBJECT_TYPE_SWITCH, m_switch_id, 1, &attr));

    m_default_bridge_port_1q_router = attr.value.oid;

    SWSS_LOG_NOTICE("default bridge port 1q router: %s",
            sai_serialize_object_id(m_default_bridge_port_1q_router).c_str());

    attr.id = SAI_SWITCH_ATTR_CPU_PORT;

    CHECK_STATUS(get(SAI_OBJECT_TYPE_SWITCH, m_switch_id, 1, &attr));

    m_cpu_port_id = attr.value.oid;

    SWSS_LOG_NOTICE("cpu port id: %s",
            sai_serialize_object_id(m_cpu_port_id).c_str());

    // TODO refresh
    //
    // m_bridge_port_list_port_based;
    // m_default_1q_bridge;
    // m_default_vlan_id;
    //
    // other objects/numbers if added (per switch also)

    // TODO update
    // std::unordered_set<uint32_t> m_indices;

    // When booting from older virtual switch (eg. 201811) some attributes
    // could not be populated at that time, but they can be populated on master
    // (mostly default values of those attributes) so they will require manual
    // update after warm boot, for example: SAI_OBJECT_TYPE_QUEUE has no
    // attributes on 201811 branch but on master there are 3 attributes
    // populated SAI_QUEUE_ATTR_INDEX, PORT and TYPE. This could be per asic,
    // so those functions are virtual and could be overridden in child class.

    CHECK_STATUS(warm_update_queues());
    CHECK_STATUS(warm_update_scheduler_groups());
    CHECK_STATUS(warm_update_ingress_priority_groups());
    CHECK_STATUS(warm_update_switch());
    CHECK_STATUS(warm_update_cpu_queues());

    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchStateBase::warm_update_queues()
{
    SWSS_LOG_ENTER();

    SWSS_LOG_ERROR("implement in child class");

    return SAI_STATUS_NOT_IMPLEMENTED;
}

sai_status_t SwitchStateBase::warm_update_scheduler_groups()
{
    SWSS_LOG_ENTER();

    for (auto port: m_port_list)
    {
        sai_attribute_t attr;

        std::vector<sai_object_id_t> list(MAX_OBJLIST_LEN);

        // get all scheduler list on current port

        attr.id = SAI_PORT_ATTR_QOS_SCHEDULER_GROUP_LIST;

        attr.value.objlist.count = MAX_OBJLIST_LEN;
        attr.value.objlist.list = list.data();

        CHECK_STATUS(get(SAI_OBJECT_TYPE_PORT, port , 1, &attr));

        list.resize(attr.value.objlist.count);

        for (auto sg: list)
        {
            attr.id = SAI_SCHEDULER_GROUP_ATTR_PORT_ID;

            if (get(SAI_OBJECT_TYPE_SCHEDULER_GROUP, sg, 1, &attr) != SAI_STATUS_SUCCESS)
            {
                attr.value.oid = port;

                CHECK_STATUS(set(SAI_OBJECT_TYPE_SCHEDULER_GROUP, sg, &attr));
            }
        }
    }

    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchStateBase::warm_update_ingress_priority_groups()
{
    SWSS_LOG_ENTER();

    for (auto port: m_port_list)
    {
        sai_attribute_t attr;

        std::vector<sai_object_id_t> list(MAX_OBJLIST_LEN);

        // get all ingress priority groups list on current port

        attr.id = SAI_PORT_ATTR_INGRESS_PRIORITY_GROUP_LIST;

        attr.value.objlist.count = MAX_OBJLIST_LEN;
        attr.value.objlist.list = list.data();

        CHECK_STATUS(get(SAI_OBJECT_TYPE_PORT, port , 1, &attr));

        list.resize(attr.value.objlist.count);

        uint8_t index = 0;

        for (auto ipg: list)
        {
            attr.id = SAI_INGRESS_PRIORITY_GROUP_ATTR_PORT;

            if (get(SAI_OBJECT_TYPE_INGRESS_PRIORITY_GROUP, ipg, 1, &attr) != SAI_STATUS_SUCCESS)
            {
                attr.value.oid = port;

                CHECK_STATUS(set(SAI_OBJECT_TYPE_INGRESS_PRIORITY_GROUP, ipg, &attr));
            }

            attr.id = SAI_INGRESS_PRIORITY_GROUP_ATTR_BUFFER_PROFILE;

            if (get(SAI_OBJECT_TYPE_INGRESS_PRIORITY_GROUP, ipg, 1, &attr) != SAI_STATUS_SUCCESS)
            {
                attr.value.oid = SAI_NULL_OBJECT_ID;

                CHECK_STATUS(set(SAI_OBJECT_TYPE_INGRESS_PRIORITY_GROUP, ipg, &attr));
            }

            attr.id = SAI_INGRESS_PRIORITY_GROUP_ATTR_INDEX;

            if (get(SAI_OBJECT_TYPE_INGRESS_PRIORITY_GROUP, ipg, 1, &attr) != SAI_STATUS_SUCCESS)
            {
                attr.value.u8 = index; // warn, we are guessing index here if it was not defined

                CHECK_STATUS(set(SAI_OBJECT_TYPE_INGRESS_PRIORITY_GROUP, ipg, &attr));
            }

            index++;
        }
    }

    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchStateBase::warm_update_switch()
{
    SWSS_LOG_ENTER();

    sai_attribute_t attr;

    // check for default available attributes

    attr.id = SAI_SWITCH_ATTR_AVAILABLE_IPV4_ROUTE_ENTRY;

    if (get(SAI_OBJECT_TYPE_SWITCH, m_switch_id, 1, &attr) != SAI_STATUS_SUCCESS)
    {
        CHECK_STATUS(set_static_crm_values())
    }

    // check for default switch type

    attr.id = SAI_SWITCH_ATTR_TYPE;

    if (get(SAI_OBJECT_TYPE_SWITCH, m_switch_id, 1, &attr) != SAI_STATUS_SUCCESS)
    {
        attr.value.s32 = SAI_SWITCH_TYPE_NPU;

        CHECK_STATUS(set(SAI_OBJECT_TYPE_SWITCH, m_switch_id, &attr));
    }

    // check for default acl stages

    attr.id = SAI_SWITCH_ATTR_MAX_ACL_ACTION_COUNT;

    if (get(SAI_OBJECT_TYPE_SWITCH, m_switch_id, 1, &attr) != SAI_STATUS_SUCCESS)
    {
        CHECK_STATUS(set_acl_capabilities());
    }

    // check for default supported object types

    attr.id = SAI_SWITCH_ATTR_SUPPORTED_OBJECT_TYPE_LIST;

    std::vector<sai_object_type_t> objs(MAX_OBJLIST_LEN);

    attr.value.s32list.count = MAX_OBJLIST_LEN;
    attr.value.s32list.list = (int32_t*)objs.data();

    if (get(SAI_OBJECT_TYPE_SWITCH, m_switch_id, 1, &attr) != SAI_STATUS_SUCCESS)
    {
        CHECK_STATUS(set_switch_supported_object_types());
    }

    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchStateBase::warm_update_cpu_queues()
{
    SWSS_LOG_ENTER();

    sai_attribute_t attr;

    // check for queues

    attr.id = SAI_PORT_ATTR_QOS_NUMBER_OF_QUEUES;

    if (get(SAI_OBJECT_TYPE_PORT, m_cpu_port_id, 1, &attr) != SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_WARN("creating cpu queues");

        CHECK_STATUS(create_cpu_qos_queues(m_cpu_port_id));
    }

    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchStateBase::refresh_bridge_port_list(
        _In_ const sai_attr_metadata_t *meta,
        _In_ sai_object_id_t bridge_id)
{
    SWSS_LOG_ENTER();

    SWSS_LOG_ERROR("implement in child class");

    return SAI_STATUS_NOT_IMPLEMENTED;
}

sai_status_t SwitchStateBase::refresh_vlan_member_list(
        _In_ const sai_attr_metadata_t *meta,
        _In_ sai_object_id_t vlan_id)
{
    SWSS_LOG_ENTER();

    auto &all_vlan_members = m_objectHash.at(SAI_OBJECT_TYPE_VLAN_MEMBER);

    auto m_member_list = sai_metadata_get_attr_metadata(SAI_OBJECT_TYPE_VLAN, SAI_VLAN_ATTR_MEMBER_LIST);
    auto md_vlan_id = sai_metadata_get_attr_metadata(SAI_OBJECT_TYPE_VLAN_MEMBER, SAI_VLAN_MEMBER_ATTR_VLAN_ID);
    //auto md_brportid = sai_metadata_get_attr_metadata(SAI_OBJECT_TYPE_VLAN_MEMBER, SAI_VLAN_MEMBER_ATTR_BRIDGE_PORT_ID);

    std::vector<sai_object_id_t> vlan_member_list;

    /*
     * We want order as bridge port order (so port order)
     */

    sai_attribute_t attr;

    auto me = m_objectHash.at(SAI_OBJECT_TYPE_VLAN).at(sai_serialize_object_id(vlan_id));

    for (auto vm: all_vlan_members)
    {
        if (vm.second.at(md_vlan_id->attridname)->getAttr()->value.oid != vlan_id)
        {
            /*
             * Only interested in our vlan
             */

            continue;
        }

        // TODO we need order as bridge ports, but we need bridge id!

        {
            sai_object_id_t vlan_member_id;

            sai_deserialize_object_id(vm.first, vlan_member_id);

            vlan_member_list.push_back(vlan_member_id);
        }
    }

    uint32_t vlan_member_list_count = (uint32_t)vlan_member_list.size();

    SWSS_LOG_NOTICE("recalculated %s: %u", m_member_list->attridname, vlan_member_list_count);

    attr.id = SAI_VLAN_ATTR_MEMBER_LIST;
    attr.value.objlist.count = vlan_member_list_count;
    attr.value.objlist.list = vlan_member_list.data();

    return set(SAI_OBJECT_TYPE_VLAN, vlan_id, &attr);
}

sai_status_t SwitchStateBase::refresh_port_list(
        _In_ const sai_attr_metadata_t *meta)
{
    SWSS_LOG_ENTER();

    // since now port can be added or removed, we need to update port list
    // dynamically

    sai_attribute_t attr;

    attr.id = SAI_SWITCH_ATTR_CPU_PORT;

    CHECK_STATUS(get(SAI_OBJECT_TYPE_SWITCH, m_switch_id, 1, &attr));

    const sai_object_id_t cpu_port_id = attr.value.oid;

    std::unordered_set<sai_object_id_t> fabric_port_set(m_fabric_port_list.begin(),
            m_fabric_port_list.end());

    m_port_list.clear();

    // iterate via ASIC state to find all the ports

    for (const auto& it: m_objectHash.at(SAI_OBJECT_TYPE_PORT))
    {
        sai_object_id_t port_id;
        sai_deserialize_object_id(it.first, port_id);

        // don't put CPU port id on the list

        if (port_id == cpu_port_id)
            continue;

        // don't put fabric ports on the list
        if (fabric_port_set.find(port_id) != fabric_port_set.end())
            continue;

        m_port_list.push_back(port_id);
    }

    /*
     * XXX Currently we don't know what's happen on brcm SAI implementation
     * when port is removed and then added, will new port could get the same
     * vendor OID or always different, and what is order of those new oids on
     * the PORT_LIST attribute.
     *
     * This needs to be investigated, and to reflect exact behaviour here.
     * Currently we just sort all the port oids.
     */

    std::sort(m_port_list.begin(), m_port_list.end());

    uint32_t port_count = (uint32_t)m_port_list.size();

    attr.id = SAI_SWITCH_ATTR_PORT_LIST;
    attr.value.objlist.count = port_count;
    attr.value.objlist.list = m_port_list.data();

    CHECK_STATUS(set(SAI_OBJECT_TYPE_SWITCH, m_switch_id, &attr));

    attr.id = SAI_SWITCH_ATTR_PORT_NUMBER;
    attr.value.u32 = port_count;

    CHECK_STATUS(set(SAI_OBJECT_TYPE_SWITCH, m_switch_id, &attr));

    SWSS_LOG_NOTICE("refreshed port list, current port number: %zu, not counting cpu port", m_port_list.size());

    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchStateBase::refresh_macsec_sci_in_ingress_macsec_acl(
        _In_ sai_object_id_t object_id)
{
    SWSS_LOG_ENTER();

    /*
     * SAI_MACSEC_ATTR_SCI_IN_INGRESS_MACSEC_ACL indicates the MACsec ASIC capability
     * of whether SCI can only be used as ACL field.
     * To set SAI_MACSEC_ATTR_SCI_IN_INGRESS_MACSEC_ACL is always true,
     * which indicates that here is emulating a kind of MACsec ASIC that use SCI as ACL field.
     */
    sai_attribute_t attr;
    attr.id = SAI_MACSEC_ATTR_SCI_IN_INGRESS_MACSEC_ACL;
    attr.value.booldata = true;

    CHECK_STATUS(set(SAI_OBJECT_TYPE_MACSEC, object_id, &attr));

    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchStateBase::refresh_queue_pause_status(
        _In_ sai_object_id_t object_id)
{
    SWSS_LOG_ENTER();

    // To trigger fake PFC storm on fake Broadcom platform, PFC storm detection
    // lua requires SAI_QUEUE_ATTR_PAUSE_STATUS field to be present in COUNTERS_DB.
    // However, the actual value of the attribute does not matter in this regard,
    // so a dummy one is assigned here.
    sai_attribute_t attr;
    attr.id = SAI_QUEUE_ATTR_PAUSE_STATUS;
    attr.value.booldata = false;

    CHECK_STATUS(set(SAI_OBJECT_TYPE_QUEUE, object_id, &attr));

    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchStateBase::refresh_macsec_sa_stat(
        _In_ sai_object_id_t object_id)
{
    SWSS_LOG_ENTER();

    sai_attribute_t attr;

    attr.id = SAI_MACSEC_SA_ATTR_CURRENT_XPN;
    CHECK_STATUS(getMACsecSAPacketNumber(object_id, attr));
    CHECK_STATUS(set(SAI_OBJECT_TYPE_MACSEC_SA, object_id, &attr));

    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchStateBase::refresh_port_serdes_id(
        _In_ sai_object_id_t port_id)
{
    SWSS_LOG_ENTER();

    // port serdes could be removed by user explicitly, so try to find port
    // serdes object with this specific port

    sai_attribute_t attr;

    attr.id = SAI_PORT_ATTR_PORT_SERDES_ID;
    attr.value.oid = SAI_NULL_OBJECT_ID;

    auto* meta = sai_metadata_get_attr_metadata(SAI_OBJECT_TYPE_PORT_SERDES, SAI_PORT_SERDES_ATTR_PORT_ID);

    // loop via all port serdes objects

    auto &serdeses = m_objectHash.at(SAI_OBJECT_TYPE_PORT_SERDES);

    auto strPortId = sai_serialize_object_id(port_id);

    for (auto& kvp: serdeses)
    {
        if (kvp.second.find(meta->attridname) != kvp.second.end())
        {
            if (kvp.second.at(meta->attridname)->getAttrStrValue() != strPortId)
            {
                // this is not the port we are looking for
                continue;
            }

            SWSS_LOG_INFO("found corresponding port serdes %s for port %s",
                    kvp.first.c_str(),
                    sai_serialize_object_id(port_id).c_str());

            sai_deserialize_object_id(kvp.first, attr.value.oid);
            break;
        }
    }

    CHECK_STATUS(set(SAI_OBJECT_TYPE_PORT, port_id, &attr));

    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchStateBase::refresh_port_oper_speed(
                    _In_ sai_object_id_t port_id)
{
    SWSS_LOG_ENTER();

    sai_attribute_t attr;

    attr.id = SAI_PORT_ATTR_OPER_STATUS;

    CHECK_STATUS(get(SAI_OBJECT_TYPE_PORT, port_id, 1, &attr));

    if (attr.value.s32 == SAI_PORT_OPER_STATUS_DOWN)
    {
        attr.value.u32 = 0;
    }
    else
    {
        if (m_switchConfig->m_useConfiguredSpeedAsOperSpeed)
        {
            attr.id = SAI_PORT_ATTR_SPEED;
            CHECK_STATUS(get(SAI_OBJECT_TYPE_PORT, port_id, 1, &attr));
        }
        else if (!vs_get_oper_speed(port_id, attr.value.u32))
        {
            return SAI_STATUS_FAILURE;
        }
    }

    attr.id = SAI_PORT_ATTR_OPER_SPEED;

    CHECK_STATUS(set(SAI_OBJECT_TYPE_PORT, port_id, &attr));

    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchStateBase::refresh_acl_table_entries(
                    _In_ sai_object_id_t acl_table_id)
{
    SWSS_LOG_ENTER();

    std::vector<sai_object_id_t> acl_entries;

    sai_attribute_t attr;
    attr.id = SAI_ACL_ENTRY_ATTR_TABLE_ID;
    attr.value.oid = acl_table_id;
    findObjects(SAI_OBJECT_TYPE_ACL_ENTRY, attr, acl_entries);

    attr.id = SAI_ACL_TABLE_ATTR_AVAILABLE_ACL_ENTRY;
    attr.value.u32 = m_maxAclTableEntries - (uint32_t) acl_entries.size();

    CHECK_STATUS(set(SAI_OBJECT_TYPE_ACL_TABLE, acl_table_id, &attr));

    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchStateBase::refresh_acl_table_counters(
                    _In_ sai_object_id_t acl_table_id)
{
    SWSS_LOG_ENTER();

    std::vector<sai_object_id_t> acl_counters;

    sai_attribute_t attr;
    attr.id = SAI_ACL_COUNTER_ATTR_TABLE_ID;
    attr.value.oid = acl_table_id;
    findObjects(SAI_OBJECT_TYPE_ACL_COUNTER, attr, acl_counters);

    attr.id = SAI_ACL_TABLE_ATTR_AVAILABLE_ACL_COUNTER;
    attr.value.u32 = m_maxAclTableCounters - (uint32_t) acl_counters.size();

    CHECK_STATUS(set(SAI_OBJECT_TYPE_ACL_TABLE, acl_table_id, &attr));

    return SAI_STATUS_SUCCESS;
}


// XXX extra work may be needed on GET api if N on list will be > then actual

/*
 * We can use local variable here for initialization (init should be in class
 * constructor anyway, we can move it there later) because each switch init is
 * done under global lock.
 */

/*
 * NOTE For recalculation we can add flag on create/remove specific object type
 * so we can deduce whether actually need to perform recalculation, as
 * optimization.
 */

sai_status_t SwitchStateBase::refresh_read_only(
        _In_ const sai_attr_metadata_t *meta,
        _In_ sai_object_id_t object_id)
{
    SWSS_LOG_ENTER();

    if (meta->objecttype == SAI_OBJECT_TYPE_SWITCH)
    {
        switch (meta->attrid)
        {
            case SAI_SWITCH_ATTR_CPU_PORT:
            case SAI_SWITCH_ATTR_DEFAULT_VIRTUAL_ROUTER_ID:
            case SAI_SWITCH_ATTR_DEFAULT_TRAP_GROUP:
            case SAI_SWITCH_ATTR_DEFAULT_VLAN_ID:
            case SAI_SWITCH_ATTR_DEFAULT_STP_INST_ID:
            case SAI_SWITCH_ATTR_DEFAULT_1Q_BRIDGE_ID:
                return SAI_STATUS_SUCCESS;

            case SAI_SWITCH_ATTR_ECMP_HASH:
            case SAI_SWITCH_ATTR_LAG_HASH:
                return SAI_STATUS_SUCCESS;

            case SAI_SWITCH_ATTR_ACL_ENTRY_MINIMUM_PRIORITY:
            case SAI_SWITCH_ATTR_ACL_ENTRY_MAXIMUM_PRIORITY:
                return SAI_STATUS_SUCCESS;

            case SAI_SWITCH_ATTR_MAX_ACL_ACTION_COUNT:
            case SAI_SWITCH_ATTR_ACL_STAGE_INGRESS:
            case SAI_SWITCH_ATTR_ACL_STAGE_EGRESS:
                return SAI_STATUS_SUCCESS;

            case SAI_SWITCH_ATTR_NUMBER_OF_ECMP_GROUPS:
                return SAI_STATUS_SUCCESS;

            case SAI_SWITCH_ATTR_PORT_NUMBER:
            case SAI_SWITCH_ATTR_PORT_LIST:
                return refresh_port_list(meta);

            case SAI_SWITCH_ATTR_QOS_MAX_NUMBER_OF_CHILDS_PER_SCHEDULER_GROUP:
                return SAI_STATUS_SUCCESS;

            case SAI_SWITCH_ATTR_AVAILABLE_IPV4_ROUTE_ENTRY:
            case SAI_SWITCH_ATTR_AVAILABLE_IPV6_ROUTE_ENTRY:
            case SAI_SWITCH_ATTR_AVAILABLE_IPV4_NEXTHOP_ENTRY:
            case SAI_SWITCH_ATTR_AVAILABLE_IPV6_NEXTHOP_ENTRY:
            case SAI_SWITCH_ATTR_AVAILABLE_IPV4_NEIGHBOR_ENTRY:
            case SAI_SWITCH_ATTR_AVAILABLE_IPV6_NEIGHBOR_ENTRY:
            case SAI_SWITCH_ATTR_AVAILABLE_NEXT_HOP_GROUP_MEMBER_ENTRY:
            case SAI_SWITCH_ATTR_AVAILABLE_NEXT_HOP_GROUP_ENTRY:
            case SAI_SWITCH_ATTR_AVAILABLE_FDB_ENTRY:
            case SAI_SWITCH_ATTR_AVAILABLE_ACL_TABLE:
            case SAI_SWITCH_ATTR_AVAILABLE_ACL_TABLE_GROUP:
            case SAI_SWITCH_ATTR_AVAILABLE_SNAT_ENTRY:
            case SAI_SWITCH_ATTR_AVAILABLE_DNAT_ENTRY:
            case SAI_SWITCH_ATTR_AVAILABLE_IPMC_ENTRY:
            case SAI_SWITCH_ATTR_AVAILABLE_DOUBLE_NAT_ENTRY:
                return SAI_STATUS_SUCCESS;

            case SAI_SWITCH_ATTR_SUPPORTED_IPV4_BFD_SESSION_OFFLOAD_TYPE:
            case SAI_SWITCH_ATTR_SUPPORTED_IPV6_BFD_SESSION_OFFLOAD_TYPE:
                return SAI_STATUS_SUCCESS;

            case SAI_SWITCH_ATTR_NUMBER_OF_SYSTEM_PORTS:
            case SAI_SWITCH_ATTR_SYSTEM_PORT_LIST:
                return refresh_system_port_list(meta);

            case SAI_SWITCH_ATTR_NUMBER_OF_FABRIC_PORTS:
            case SAI_SWITCH_ATTR_FABRIC_PORT_LIST:
                return SAI_STATUS_SUCCESS;

            case SAI_SWITCH_ATTR_SUPPORTED_OBJECT_TYPE_LIST:
                return SAI_STATUS_SUCCESS;
        }
    }

    if (meta->objecttype == SAI_OBJECT_TYPE_PORT)
    {
        switch (meta->attrid)
        {
            case SAI_PORT_ATTR_QOS_NUMBER_OF_QUEUES:
            case SAI_PORT_ATTR_QOS_QUEUE_LIST:
                return refresh_qos_queues(meta, object_id);

            case SAI_PORT_ATTR_NUMBER_OF_INGRESS_PRIORITY_GROUPS:
            case SAI_PORT_ATTR_INGRESS_PRIORITY_GROUP_LIST:
                return refresh_ingress_priority_group(meta, object_id);

            case SAI_PORT_ATTR_QOS_NUMBER_OF_SCHEDULER_GROUPS:
            case SAI_PORT_ATTR_QOS_SCHEDULER_GROUP_LIST:
                return refresh_scheduler_groups(meta, object_id);

                /*
                 * This status is based on hostif vEthernetX status.
                 */

            case SAI_PORT_ATTR_OPER_STATUS:
            case SAI_PORT_ATTR_HOST_TX_READY_STATUS:
                return SAI_STATUS_SUCCESS;

            case SAI_PORT_ATTR_FABRIC_ATTACHED:
            case SAI_PORT_ATTR_FABRIC_ATTACHED_SWITCH_ID:
            case SAI_PORT_ATTR_FABRIC_ATTACHED_PORT_INDEX:
            case SAI_PORT_ATTR_HW_LANE_LIST:
                return SAI_STATUS_SUCCESS;

            case SAI_PORT_ATTR_PORT_SERDES_ID:
                return refresh_port_serdes_id(object_id);

            case SAI_PORT_ATTR_SUPPORTED_AUTO_NEG_MODE:
                return SAI_STATUS_SUCCESS;

            case SAI_PORT_ATTR_OPER_SPEED:
                return refresh_port_oper_speed(object_id);
        }
    }

    if (meta->objecttype == SAI_OBJECT_TYPE_SYSTEM_PORT)
    {
        switch (meta->attrid)
        {
            case SAI_SYSTEM_PORT_ATTR_TYPE:
            case SAI_SYSTEM_PORT_ATTR_PORT:
            case SAI_SYSTEM_PORT_ATTR_QOS_NUMBER_OF_VOQS:
            case SAI_SYSTEM_PORT_ATTR_QOS_VOQ_LIST:
                return SAI_STATUS_SUCCESS;
        }
    }

    if (meta->objecttype == SAI_OBJECT_TYPE_SCHEDULER_GROUP)
    {
        switch (meta->attrid)
        {
            case SAI_SCHEDULER_GROUP_ATTR_CHILD_COUNT:
            case SAI_SCHEDULER_GROUP_ATTR_CHILD_LIST:
                return refresh_scheduler_groups(meta, object_id);
        }
    }

    if (meta->objecttype == SAI_OBJECT_TYPE_BRIDGE && meta->attrid == SAI_BRIDGE_ATTR_PORT_LIST)
    {
        return refresh_bridge_port_list(meta, object_id);
    }

    if (meta->objecttype == SAI_OBJECT_TYPE_VLAN && meta->attrid == SAI_VLAN_ATTR_MEMBER_LIST)
    {
        return refresh_vlan_member_list(meta, object_id);
    }

    if (meta->objecttype == SAI_OBJECT_TYPE_DEBUG_COUNTER && meta->attrid == SAI_DEBUG_COUNTER_ATTR_INDEX)
    {
        return SAI_STATUS_SUCCESS;
    }

    if (meta->objecttype == SAI_OBJECT_TYPE_MACSEC && meta->attrid == SAI_MACSEC_ATTR_SCI_IN_INGRESS_MACSEC_ACL)
    {
        return refresh_macsec_sci_in_ingress_macsec_acl(object_id);
    }

    if (meta->objecttype == SAI_OBJECT_TYPE_QUEUE && meta->attrid == SAI_QUEUE_ATTR_PAUSE_STATUS)
    {
        return refresh_queue_pause_status(object_id);
    }

    if (meta->objecttype == SAI_OBJECT_TYPE_MACSEC_SA)
    {
        return refresh_macsec_sa_stat(object_id);
    }

    if (meta->objecttype == SAI_OBJECT_TYPE_ACL_TABLE && meta->attrid == SAI_ACL_TABLE_ATTR_AVAILABLE_ACL_ENTRY)
    {
        return refresh_acl_table_entries(object_id);
    }

    if (meta->objecttype == SAI_OBJECT_TYPE_ACL_TABLE && meta->attrid == SAI_ACL_TABLE_ATTR_AVAILABLE_ACL_COUNTER)
    {
        return refresh_acl_table_counters(object_id);
    }

    auto mmeta = m_meta.lock();

    if (mmeta)
    {
        if (mmeta->meta_unittests_enabled())
        {
            SWSS_LOG_NOTICE("unittests enabled, SET could be performed on %s, not recalculating", meta->attridname);

            return SAI_STATUS_SUCCESS;
        }
    }
    else
    {
        SWSS_LOG_WARN("meta pointer expired");
    }

    SWSS_LOG_WARN("need to recalculate RO: %s", meta->attridname);

    return SAI_STATUS_NOT_IMPLEMENTED;
}

void SwitchStateBase::processFdbEntriesForAging()
{
    SWSS_LOG_ENTER();

    SWSS_LOG_DEBUG("fdb infos to process: %zu", m_fdb_info_set.size());

    uint32_t current = (uint32_t)time(NULL);

    sai_attribute_t attr;

    attr.id = SAI_SWITCH_ATTR_FDB_AGING_TIME;

    sai_status_t status = get(SAI_OBJECT_TYPE_SWITCH, m_switch_id, 1, &attr);

    if (status != SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_WARN("failed to get FDB aging time for switch %s",
                sai_serialize_object_id(m_switch_id).c_str());

        return;
    }

    uint32_t aging_time = attr.value.u32;

    if (aging_time == 0)
    {
        SWSS_LOG_DEBUG("aging is disabled");
        return;
    }

    // find aged fdb entries

    for (auto it = m_fdb_info_set.begin(); it != m_fdb_info_set.end();)
    {
        if ((current - it->getTimestamp()) >= aging_time)
        {
            FdbInfo fi = *it;

            processFdbInfo(fi, SAI_FDB_EVENT_AGED);

            it = m_fdb_info_set.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

bool SwitchStateBase::isPortReadyToBeRemove(
        _In_ sai_object_id_t portId)
{
    SWSS_LOG_ENTER();

    std::vector<sai_object_id_t> dep;

    auto status = check_port_dependencies(portId, dep);

    return status == SAI_STATUS_SUCCESS;
}

std::vector<sai_object_id_t> SwitchStateBase::getPortDependencies(
        _In_ sai_object_id_t portId)
{
    SWSS_LOG_ENTER();

    std::vector<sai_object_id_t> dep;

    check_port_dependencies(portId, dep);

    return dep;
}

bool SwitchStateBase::check_port_reference_count(
        _In_ sai_object_id_t port_id)
{
    SWSS_LOG_ENTER();

    // TODO make generic

    // TODO currently when switch is initialized, there is no metadata yet
    // and objects are created without reference count, this needs to be
    // addressed in refactoring metadata and meta_create_oid to correct
    // count references, but now we need to check if port is used in any
    // bridge port (and bridge port in any vlan member), after metadata
    // refactor this function can be removed.

    // check if port is used on any bridge port object (only switch init one
    // matters, user created bridge ports will have correct reference count

    auto& bridgePorts = m_objectHash.at(SAI_OBJECT_TYPE_BRIDGE_PORT);

    auto* meta = sai_metadata_get_attr_metadata(SAI_OBJECT_TYPE_BRIDGE_PORT, SAI_BRIDGE_PORT_ATTR_PORT_ID);

    for (auto& bp: bridgePorts)
    {
        for (auto&attr: bp.second)
        {
            if (attr.first != meta->attridname)
                continue; // not this attribute

            if (attr.second->getAttr()->value.oid == port_id)
            {
                SWSS_LOG_ERROR("port id %s is in use on bridge port %s",
                        sai_serialize_object_id(port_id).c_str(),
                        bp.first.c_str());

                return false;
            }
        }
    }

    auto& serdeses = m_objectHash.at(SAI_OBJECT_TYPE_PORT_SERDES);

    meta = sai_metadata_get_attr_metadata(SAI_OBJECT_TYPE_PORT_SERDES, SAI_PORT_SERDES_ATTR_PORT_ID);

    for (auto& ps: serdeses)
    {
        for (auto&attr: ps.second)
        {
            if (attr.first != meta->attridname)
                continue; // not this attribute

            if (attr.second->getAttr()->value.oid == port_id)
            {
                SWSS_LOG_ERROR("port id %s is in use on port serdes %s",
                        sai_serialize_object_id(port_id).c_str(),
                        ps.first.c_str());

                return false;
            }
        }
    }

    return true;
}

sai_status_t SwitchStateBase::check_port_dependencies(
        _In_ sai_object_id_t port_id,
        _Out_ std::vector<sai_object_id_t>& dep)
{
    SWSS_LOG_ENTER();

    // check if port exists's

    sai_object_id_t switch_id = switchIdQuery(port_id);

    if (switch_id == SAI_NULL_OBJECT_ID)
    {
        SWSS_LOG_ERROR("failed to obtain switch_id from object %s",
                sai_serialize_object_id(port_id).c_str());

        return SAI_STATUS_FAILURE;
    }

    sai_object_type_t ot = objectTypeQuery(port_id);

    if (ot != SAI_OBJECT_TYPE_PORT)
    {
        SWSS_LOG_ERROR("expected object type PORT but object %s has type %s",
                sai_serialize_object_id(port_id).c_str(),
                sai_serialize_object_type(ot).c_str());

        return SAI_STATUS_FAILURE;
    }

    std::string str_port_id = sai_serialize_object_id(port_id);

    auto &objectHash = m_objectHash.at(ot);

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

    // check port reference count on bridge port

    if (!check_port_reference_count(port_id))
    {
        SWSS_LOG_ERROR("port %s reference count IS NOT ZERO, can't remove, remove dependencies first",
                sai_serialize_object_id(port_id).c_str());

        return SAI_STATUS_FAILURE;
    }

    // obtain objects to examine

    std::vector<sai_object_id_t> queues;
    std::vector<sai_object_id_t> ipgs;
    std::vector<sai_object_id_t> sg;

    bool result = true;

    result &= get_port_queues(port_id, queues);
    result &= get_port_ipgs(port_id, ipgs);
    result &= get_port_sg(port_id, sg);

    if (!result)
    {
        SWSS_LOG_ERROR("failed to obtain required objects on port %s",
                sai_serialize_object_id(port_id).c_str());

        return SAI_STATUS_FAILURE;
    }

    // check if all objects are in default state

    result &= check_object_default_state(port_id);
    result &= check_object_list_default_state(queues);
    result &= check_object_list_default_state(ipgs);
    result &= check_object_list_default_state(sg);

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

    // BRIDGE PORT (and also VLAN MEMBER using that bridge port) must be
    // removed before removing port itself, since bridge port holds reference
    // to port being removed.

    // bridge ports and vlan members must be removed before port can be removed

    return SAI_STATUS_SUCCESS;
}


bool SwitchStateBase::get_object_list(
        _In_ sai_object_id_t object_id,
        _In_ sai_attr_id_t attr_id,
        _Out_ std::vector<sai_object_id_t>& objlist)
{
    SWSS_LOG_ENTER();

    objlist.clear();

    sai_object_type_t object_type = objectTypeQuery(object_id);

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

    status = get(object_type, object_id, 1, &attr);

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

bool SwitchStateBase::get_port_queues(
        _In_ sai_object_id_t port_id,
        _Out_ std::vector<sai_object_id_t>& queues)
{
    SWSS_LOG_ENTER();

    return get_object_list(port_id, SAI_PORT_ATTR_QOS_QUEUE_LIST, queues);
}

bool SwitchStateBase::get_port_ipgs(
        _In_ sai_object_id_t port_id,
        _Out_ std::vector<sai_object_id_t>& ipgs)
{
    SWSS_LOG_ENTER();

    return get_object_list(port_id, SAI_PORT_ATTR_INGRESS_PRIORITY_GROUP_LIST, ipgs);
}

bool SwitchStateBase::get_port_sg(
        _In_ sai_object_id_t port_id,
        _Out_ std::vector<sai_object_id_t>& sg)
{
    SWSS_LOG_ENTER();

    // scheduler groups are organized in tree, but
    // SAI_PORT_ATTR_INGRESS_PRIORITY_GROUP_LIST should return all scheduler groups in that tree

    return get_object_list(port_id, SAI_PORT_ATTR_QOS_SCHEDULER_GROUP_LIST, sg);
}

bool SwitchStateBase::check_object_default_state(
        _In_ sai_object_id_t object_id)
{
    SWSS_LOG_ENTER();

    sai_object_type_t object_type = objectTypeQuery(object_id);

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

        if (meta->ismandatoryoncreate && meta->iscreateonly)
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

        status = get(object_type, object_id, 1, &attr);

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

    // XXX later there can be issue when we for example add extra queues to
    // the port those new queues should be removed by user first before
    // removing port, and currently we don't have a way to differentiate those

    // object is in default state
    return true;
}

bool SwitchStateBase::check_object_list_default_state(
        _Out_ const std::vector<sai_object_id_t>& objlist)
{
    SWSS_LOG_ENTER();

    return std::all_of(objlist.begin(), objlist.end(),
            [&](sai_object_id_t oid) { return check_object_default_state(oid); });
}

std::string SwitchStateBase::dump_switch_database_for_warm_restart() const
{
    SWSS_LOG_ENTER();

    std::stringstream ss;

    const auto& objectHash = m_objectHash;

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
                ss << sai_serialize_object_type(kvp.first) << " " << o.first << " NULL NULL" << std::endl;
                continue;
            }

            for (auto a: o.second)
            {
                ss << sai_serialize_object_type(kvp.first) << " ";
                ss << o.first.c_str();
                ss << " ";
                ss << a.first.c_str();
                ss << " ";
                ss << a.second->getAttrStrValue();
                ss << std::endl;
            }
        }
    }

    if (m_switchConfig->m_useTapDevice)
    {
        /*
         * If user is using tap devices we also need to dump local fdb info
         * data and restore it on warm start.
         */

        for (auto fi: m_fdb_info_set)
        {
            ss << SAI_VS_FDB_INFO << " " << fi.serialize() << std::endl;
        }

        SWSS_LOG_NOTICE("dumped %zu fdb infos for switch %s",
                m_fdb_info_set.size(),
                sai_serialize_object_id(m_switch_id).c_str());
    }

    SWSS_LOG_NOTICE("dumped %zu objects from switch %s",
            count,
            sai_serialize_object_id(m_switch_id).c_str());

    return ss.str();
}

sai_object_type_t SwitchStateBase::objectTypeQuery(
        _In_ sai_object_id_t objectId)
{
    SWSS_LOG_ENTER();

    return RealObjectIdManager::objectTypeQuery(objectId);
}

sai_object_id_t SwitchStateBase::switchIdQuery(
        _In_ sai_object_id_t objectId)
{
    SWSS_LOG_ENTER();

    return RealObjectIdManager::switchIdQuery(objectId);
}

void SwitchStateBase::debugSetStats(
        _In_ sai_object_id_t oid,
        _In_ const std::map<sai_stat_id_t, uint64_t>& stats)
{
    SWSS_LOG_ENTER();

    auto key = sai_serialize_object_id(oid);

    for (auto& kvp: stats)
    {
        m_countersMap[key][kvp.first] = kvp.second;
    }
}

sai_status_t SwitchStateBase::initialize_voq_switch_objects(
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    bool voq_switch = false;
    int32_t voq_switch_id = -1;
    uint32_t voq_max_cores = 0;
    uint32_t sys_port_count = 0;
    sai_system_port_config_t *sys_port_cfg_list = NULL;

    for (uint32_t i = 0; i < attr_count; i++)
    {
        switch (attr_list[i].id)
        {
            case SAI_SWITCH_ATTR_TYPE:

                if (attr_list[i].value.u32 != SAI_SWITCH_TYPE_VOQ)
                {
                    // Switch is not being set as VOQ type.
                    SWSS_LOG_NOTICE("initialize_voq_switch_objects the value is %d", attr_list[i].value.u32);
                    if (attr_list[i].value.u32 == SAI_SWITCH_TYPE_FABRIC)
                    {
                       SWSS_LOG_NOTICE("about to config fabric ports");
                       if (m_switchConfig->m_fabricLaneMap)
                       {
                          CHECK_STATUS(create_fabric_ports());
                          CHECK_STATUS(set_fabric_port_list());
                       }
                    }
                    return SAI_STATUS_SUCCESS;
                }
                else
                {
                    voq_switch = true;
                }

                break;

            case SAI_SWITCH_ATTR_SWITCH_ID:

                voq_switch_id = (int32_t) attr_list[i].value.u32;

                if (voq_switch_id < 0)
                {
                    SWSS_LOG_ERROR("Invalid VOQ switch id %d", voq_switch_id);

                    return (int32_t)SAI_STATUS_INVALID_ATTR_VALUE_0 + (int32_t)i;
                }

                break;

            case SAI_SWITCH_ATTR_MAX_SYSTEM_CORES:

                voq_max_cores = attr_list[i].value.u32;

                if (voq_max_cores < 1)
                {
                    SWSS_LOG_ERROR("Invalid VOQ max system cores %d", voq_max_cores);

                    return (int32_t)SAI_STATUS_INVALID_ATTR_VALUE_0 + (int32_t)i;
                }

                break;

            case SAI_SWITCH_ATTR_SYSTEM_PORT_CONFIG_LIST:

                sys_port_count = attr_list[i].value.sysportconfiglist.count;
                sys_port_cfg_list = attr_list[i].value.sysportconfiglist.list;

                if (sys_port_count < 1 || !sys_port_cfg_list)
                {
                    SWSS_LOG_ERROR("Invalid voq system port config info! sys port count %d, sys port list %p", sys_port_count, sys_port_cfg_list);

                    return (int32_t)SAI_STATUS_INVALID_ATTR_VALUE_0 + (int32_t)i;
                }

                break;

            default:
                // Ignore other attributes. They are processed elsewhere.
                break;
        }
    }

    if (!voq_switch)
    {
        // No switch type attribute in the attribute list.
        return SAI_STATUS_SUCCESS;
    }

    CHECK_STATUS(create_system_ports(voq_switch_id, sys_port_count, sys_port_cfg_list));
    CHECK_STATUS(create_voqs());

    CHECK_STATUS(set_system_port_list());

    if (m_switchConfig->m_fabricLaneMap)
    {
        CHECK_STATUS(create_fabric_ports());

        CHECK_STATUS(set_fabric_port_list());
    }

    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchStateBase::filter_available_lanes(
                    _Inout_ std::vector<std::vector<uint32_t>> &lanes_vector)
{
    SWSS_LOG_ENTER();

    auto lanes = lanes_vector.begin();

    while (lanes != lanes_vector.end())
    {
        bool available_lane = false;

        for (auto lane: *lanes)
        {
            std::string ifname = m_switchConfig->m_laneMap->getInterfaceFromLaneNumber(lane);
            std::string path = std::string("/sys/class/net/") + ifname + "/operstate";

            if (access(path.c_str(), F_OK) != 0)
            {
                SWSS_LOG_WARN("Port %s isn't available", ifname.c_str());

                available_lane &= false;

                break;
            }
            else
            {
                available_lane = true;
            }
        }

        if (!available_lane)
        {
            lanes = lanes_vector.erase(lanes);
        }
        else
        {
            lanes++;
        }
    }

    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchStateBase::create_voq_per_sysport(
        _In_ sai_object_id_t sys_port_id)
{
    SWSS_LOG_ENTER();

    sai_attribute_t attr;

    const uint32_t voq_count = 8;

    std::vector<sai_object_id_t> voqs;

    for (uint32_t i = 0; i < voq_count; ++i)
    {
        sai_object_id_t voq_id;

        CHECK_STATUS(create(SAI_OBJECT_TYPE_QUEUE, &voq_id, m_switch_id, 0, NULL));
        voqs.push_back(voq_id);

        attr.id = SAI_QUEUE_ATTR_TYPE;
        attr.value.s32 = SAI_QUEUE_TYPE_UNICAST_VOQ;

        CHECK_STATUS(set(SAI_OBJECT_TYPE_QUEUE, voq_id, &attr));

        attr.id = SAI_QUEUE_ATTR_INDEX;
        attr.value.u8 = (uint8_t)i;

        CHECK_STATUS(set(SAI_OBJECT_TYPE_QUEUE, voq_id, &attr));
    }

    attr.id = SAI_SYSTEM_PORT_ATTR_QOS_NUMBER_OF_VOQS;
    attr.value.u32 = voq_count;

    CHECK_STATUS(set(SAI_OBJECT_TYPE_SYSTEM_PORT, sys_port_id, &attr));

    attr.id = SAI_SYSTEM_PORT_ATTR_QOS_VOQ_LIST;
    attr.value.objlist.count = voq_count;
    attr.value.objlist.list = voqs.data();

    CHECK_STATUS(set(SAI_OBJECT_TYPE_SYSTEM_PORT, sys_port_id, &attr));

    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchStateBase::create_voqs()
{
    SWSS_LOG_ENTER();

    for (auto &sys_port_id: m_system_port_list)
    {
        CHECK_STATUS(create_voq_per_sysport(sys_port_id));
    }

    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchStateBase::create_system_ports(
        _In_ int32_t voq_switch_id,
        _In_ uint32_t sys_port_count,
        _In_ const sai_system_port_config_t *sys_port_cfg_list)
{
    SWSS_LOG_ENTER();

    SWSS_LOG_INFO("create system ports");

    if (!sys_port_cfg_list)
    {
        return SAI_STATUS_FAILURE;
    }

    m_system_port_list.clear();

    for (uint32_t i = 0; i < sys_port_count; i++)
    {
        SWSS_LOG_DEBUG("create system port index %u", i);

        sai_object_id_t system_port_id;

        CHECK_STATUS(create(SAI_OBJECT_TYPE_SYSTEM_PORT, &system_port_id, m_switch_id, 0, NULL));

        m_system_port_list.push_back(system_port_id);

        sai_attribute_t attr;

        attr.id = SAI_SYSTEM_PORT_ATTR_CONFIG_INFO;
        attr.value.sysportconfig = sys_port_cfg_list[i];

        CHECK_STATUS(set(SAI_OBJECT_TYPE_SYSTEM_PORT, system_port_id, &attr));

        attr.id = SAI_SYSTEM_PORT_ATTR_TYPE;
        attr.value.s32 = SAI_SYSTEM_PORT_TYPE_REMOTE;

        if ((int32_t)sys_port_cfg_list[i].attached_switch_id == voq_switch_id)
        {
            attr.value.s32 = SAI_SYSTEM_PORT_TYPE_LOCAL;

            // This is system port of local port. Set the oid of the local port corresponding to this system port

            auto map = m_switchConfig->m_corePortIndexMap;

            if (map)
            {
                auto& corePortIndexVector = map->getCorePortIndexVector();
                size_t n_map = corePortIndexVector.size();
                size_t idx;

                for (idx = 0; idx < n_map; idx++)
                {
                    if (corePortIndexVector[idx][0] == sys_port_cfg_list[i].attached_core_index &&
                            corePortIndexVector[idx][1] == sys_port_cfg_list[i].attached_core_port_index &&
                            idx < m_port_list.size())
                    {
                        // m_port_list entries are in the same order as lane maps. The core port index maps are in the
                        // same order as the lane maps. So m_port_list at the index corresponding to the core port index map
                        // will be the port corresponding to the system port with core port index matching core port index map

                        sai_attribute_t lp_attr;

                        lp_attr.id = SAI_SYSTEM_PORT_ATTR_PORT;
                        lp_attr.value.oid = m_port_list.at(idx);

                        CHECK_STATUS(set(SAI_OBJECT_TYPE_SYSTEM_PORT, system_port_id, &lp_attr));

                        break;
                    }
                }

                if (idx >= n_map)
                {
                    SWSS_LOG_ERROR("Core port index not found for system port %d for switch %s. Local port oid is not set!",
                            sys_port_cfg_list[i].port_id,
                            sai_serialize_object_id(m_switch_id).c_str());
                }
            }
            else
            {
                SWSS_LOG_ERROR("Core port index map for switch %s is NULL. Local port oid is not set for system port %d!",
                        sai_serialize_object_id(m_switch_id).c_str(),
                        sys_port_cfg_list[i].port_id);
            }
        }

        CHECK_STATUS(set(SAI_OBJECT_TYPE_SYSTEM_PORT, system_port_id, &attr));
    }

    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchStateBase::set_system_port_list()
{
    SWSS_LOG_ENTER();

    SWSS_LOG_INFO("set system port list");

    // NOTE: this is static, but will be refreshed on read only get

    sai_attribute_t attr;

    uint32_t sys_port_count = (uint32_t)m_system_port_list.size();

    attr.id = SAI_SWITCH_ATTR_SYSTEM_PORT_LIST;
    attr.value.objlist.count = sys_port_count;
    attr.value.objlist.list = m_system_port_list.data();

    CHECK_STATUS(set(SAI_OBJECT_TYPE_SWITCH, m_switch_id, &attr));

    attr.id = SAI_SWITCH_ATTR_NUMBER_OF_SYSTEM_PORTS;
    attr.value.u32 = sys_port_count;

    return set(SAI_OBJECT_TYPE_SWITCH, m_switch_id, &attr);
}

sai_status_t SwitchStateBase::refresh_system_port_list(
        _In_ const sai_attr_metadata_t *meta)
{
    SWSS_LOG_ENTER();

    // Currently, system ports info are not changing. So no refresh is done

    // TODO In the future, when dynamic system port config (add/delete) is implemented, re-visit

    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchStateBase::create_fabric_ports()
{
    SWSS_LOG_ENTER();

    SWSS_LOG_INFO("create fabric ports");

    auto map = m_switchConfig->m_fabricLaneMap;

    if (!map)
    {
        SWSS_LOG_ERROR("fabric lane map for switch %s is NULL",
                sai_serialize_object_id(m_switch_id).c_str());

        return SAI_STATUS_FAILURE;
    }

    auto& lanesVector = map->getLaneVector();

    uint32_t fabric_port_count = (uint32_t)lanesVector.size();

    m_fabric_port_list.clear();

    for (uint32_t i = 0; i < fabric_port_count; i++)
    {
        SWSS_LOG_DEBUG("create fabric port index %u", i);

        sai_object_id_t fabric_port_id;

        CHECK_STATUS(create(SAI_OBJECT_TYPE_PORT, &fabric_port_id, m_switch_id, 0, NULL));
        m_fabric_port_list.push_back(fabric_port_id);

        sai_attribute_t attr;

        attr.id = SAI_PORT_ATTR_FABRIC_ATTACHED;
        attr.value.booldata = true;

        CHECK_STATUS(set(SAI_OBJECT_TYPE_PORT, fabric_port_id, &attr));

        std::vector<uint32_t> lanes = lanesVector.at(i);

        attr.id = SAI_PORT_ATTR_HW_LANE_LIST;
        attr.value.u32list.count = (uint32_t)lanes.size();
        attr.value.u32list.list = lanes.data();

        CHECK_STATUS(set(SAI_OBJECT_TYPE_PORT, fabric_port_id, &attr));

        attr.id = SAI_PORT_ATTR_TYPE;
        attr.value.s32 = SAI_PORT_TYPE_FABRIC;

        CHECK_STATUS(set(SAI_OBJECT_TYPE_PORT, fabric_port_id, &attr));

        attr.id = SAI_PORT_ATTR_FABRIC_ATTACHED_SWITCH_ID;
        attr.value.s32 = i;
        CHECK_STATUS(set(SAI_OBJECT_TYPE_PORT, fabric_port_id, &attr));

        attr.id = SAI_PORT_ATTR_FABRIC_ATTACHED_PORT_INDEX;
        attr.value.s32 = i;
        CHECK_STATUS(set(SAI_OBJECT_TYPE_PORT, fabric_port_id, &attr));
    }

    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchStateBase::set_fabric_port_list()
{
    SWSS_LOG_ENTER();

    SWSS_LOG_INFO("set fabric port list");

    sai_attribute_t attr;

    uint32_t fabric_port_count = (uint32_t)m_fabric_port_list.size();

    attr.id = SAI_SWITCH_ATTR_FABRIC_PORT_LIST;
    attr.value.objlist.count = fabric_port_count;
    attr.value.objlist.list = m_fabric_port_list.data();

    CHECK_STATUS(set(SAI_OBJECT_TYPE_SWITCH, m_switch_id, &attr));

    attr.id = SAI_SWITCH_ATTR_NUMBER_OF_FABRIC_PORTS;
    attr.value.u32 = fabric_port_count;

    return set(SAI_OBJECT_TYPE_SWITCH, m_switch_id, &attr);
}

sai_status_t SwitchStateBase::createVoqSystemNeighborEntry(
        _In_ const std::string &serializedObjectId,
        _In_ sai_object_id_t switch_id,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    // For VOQ switch, encap index attribute should be set for local neighbors. For
    // remote neighbors check for presence of encap index supplied from upper layer.
    // And also validate isLocal and impose encap index flags in the attribute list

    // Determine whether this neighbor is local neighbor or remote neighbor by checking the
    // RIF id provided. If the port of the RIF is system port, the neighbor is remote neighbor

    bool is_system_neigh = false;
    sai_attribute_t attr;
    sai_neighbor_entry_t nbr_entry;

    sai_deserialize_neighbor_entry(serializedObjectId, nbr_entry);

    attr.id = SAI_ROUTER_INTERFACE_ATTR_PORT_ID;

    CHECK_STATUS(get(SAI_OBJECT_TYPE_ROUTER_INTERFACE, nbr_entry.rif_id, 1, &attr));

    if (objectTypeQuery(attr.value.oid) == SAI_OBJECT_TYPE_SYSTEM_PORT)
    {
        is_system_neigh = true;
    }

    uint32_t encap_index = 0;
    bool impose_encap_index = false;
    bool is_local = true;

    for (uint32_t i = 0; i < attr_count; i++)
    {
        switch (attr_list[i].id)
        {
            case SAI_NEIGHBOR_ENTRY_ATTR_ENCAP_INDEX:

                encap_index = attr_list[i].value.u32;

                break;

            case SAI_NEIGHBOR_ENTRY_ATTR_ENCAP_IMPOSE_INDEX:

                impose_encap_index = attr_list[i].value.booldata;

                break;

            case SAI_NEIGHBOR_ENTRY_ATTR_IS_LOCAL:

                is_local = attr_list[i].value.booldata;

                break;

            default:
                break;
        }
    }

    if (impose_encap_index && encap_index == 0)
    {
        SWSS_LOG_ERROR("Impose invalid encap index %d for %s neigh %s",
                encap_index,
                (is_system_neigh ? "VOQ" : "local"),
                serializedObjectId.c_str());

        return SAI_STATUS_FAILURE;
    }

    if (is_system_neigh && is_local)
    {
        SWSS_LOG_ERROR("VOQ neigh info mismatch for %s. RIF is remote. attr is_local is true", serializedObjectId.c_str());

        return SAI_STATUS_FAILURE;
    }

    if (!is_local && encap_index == 0)
    {
        SWSS_LOG_ERROR("VOQ neigh info mismatch for %s. attr is_local is false. encap_index is 0", serializedObjectId.c_str());

        return SAI_STATUS_FAILURE;
    }

    CHECK_STATUS(create_internal(SAI_OBJECT_TYPE_NEIGHBOR_ENTRY, serializedObjectId, switch_id, attr_count, attr_list));

    if (impose_encap_index == false)
    {
        // Encap index is not imposed. Assign encap index.
        // The only requirement for the encap index is it must be locally
        // unique in asic. Lower 32 bits of the ip address is used as encap index

        encap_index = nbr_entry.ip_address.addr.ip4;

        attr.id = SAI_NEIGHBOR_ENTRY_ATTR_ENCAP_INDEX;
        attr.value.u32 = encap_index;

        CHECK_STATUS(set_internal(SAI_OBJECT_TYPE_NEIGHBOR_ENTRY, serializedObjectId, &attr));
    }

    return SAI_STATUS_SUCCESS;
}

void SwitchStateBase::findObjects(
        _In_ sai_object_type_t object_type,
        _In_ const sai_attribute_t &expect,
        _Out_ std::vector<sai_object_id_t> &objects)
{
    SWSS_LOG_ENTER();

    objects.clear();

    SaiAttrWrap expect_wrap(object_type, &expect);

    for (auto &obj : m_objectHash.at(object_type))
    {
        auto attr_itr = obj.second.find(expect_wrap.getAttrMetadata()->attridname);

        if (attr_itr != obj.second.end()
                && attr_itr->second->getAttrStrValue() == expect_wrap.getAttrStrValue())
        {
            sai_object_id_t object_id;
            sai_deserialize_object_id(obj.first, object_id);
            objects.push_back(object_id);
        }
    }
}

bool SwitchStateBase::dumpObject(
        _In_ const sai_object_id_t object_id,
        _Out_ std::vector<sai_attribute_t> &attrs)
{
    SWSS_LOG_ENTER();

    attrs.clear();

    auto &objs = m_objectHash.at(objectTypeQuery(object_id));
    auto obj = objs.find(sai_serialize_object_id(object_id));

    if (obj == objs.end())
    {
        return false;
    }

    for (auto &attr : obj->second)
    {
        attrs.push_back(*attr.second->getAttr());
    }

    return true;
}

sai_status_t SwitchStateBase::queryTunnelPeerModeCapability(
                   _Inout_ sai_s32_list_t *enum_values_capability)
{
    SWSS_LOG_ENTER();

    if (enum_values_capability->count < 2)
    {
        return SAI_STATUS_BUFFER_OVERFLOW;
    }

    enum_values_capability->count = 2;
    enum_values_capability->list[0] = SAI_TUNNEL_PEER_MODE_P2MP;
    enum_values_capability->list[1] = SAI_TUNNEL_PEER_MODE_P2P;
    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchStateBase::queryPortAutonegFecOverrideSupportCapability(
                   _Out_ sai_attr_capability_t *attr_capability)
{
    SWSS_LOG_ENTER();

    attr_capability->create_implemented = false;
    attr_capability->set_implemented    = false;
    attr_capability->get_implemented    = false;

    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchStateBase::queryVlanfloodTypeCapability(
                   _Inout_ sai_s32_list_t *enum_values_capability)
{
    SWSS_LOG_ENTER();

    if (enum_values_capability->count < 3)
    {
        return SAI_STATUS_BUFFER_OVERFLOW;
    }

    enum_values_capability->count = 3;
    enum_values_capability->list[0] = SAI_VLAN_FLOOD_CONTROL_TYPE_ALL;
    enum_values_capability->list[1] = SAI_VLAN_FLOOD_CONTROL_TYPE_NONE;
    enum_values_capability->list[2] = SAI_VLAN_FLOOD_CONTROL_TYPE_L2MC_GROUP;

    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchStateBase::queryNextHopGroupTypeCapability(
                   _Inout_ sai_s32_list_t *enum_values_capability)
{
    SWSS_LOG_ENTER();

    if (enum_values_capability->count < 5)
    {
        return SAI_STATUS_BUFFER_OVERFLOW;
    }

    enum_values_capability->count = 5;
    enum_values_capability->list[0] = SAI_NEXT_HOP_GROUP_TYPE_DYNAMIC_UNORDERED_ECMP;
    enum_values_capability->list[1] = SAI_NEXT_HOP_GROUP_TYPE_DYNAMIC_ORDERED_ECMP;
    enum_values_capability->list[2] = SAI_NEXT_HOP_GROUP_TYPE_FINE_GRAIN_ECMP;
    enum_values_capability->list[3] = SAI_NEXT_HOP_GROUP_TYPE_PROTECTION;
    enum_values_capability->list[4] = SAI_NEXT_HOP_GROUP_TYPE_CLASS_BASED;

    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchStateBase::queryHashNativeHashFieldListCapability(
                   _Inout_ sai_s32_list_t *enum_values_capability)
{
    SWSS_LOG_ENTER();

    if (enum_values_capability->count < 19)
    {
        enum_values_capability->count = 19;
        return SAI_STATUS_BUFFER_OVERFLOW;
    }

    enum_values_capability->count = 19;
    enum_values_capability->list[0] = SAI_NATIVE_HASH_FIELD_IN_PORT;
    enum_values_capability->list[1] = SAI_NATIVE_HASH_FIELD_DST_MAC;
    enum_values_capability->list[2] = SAI_NATIVE_HASH_FIELD_SRC_MAC;
    enum_values_capability->list[3] = SAI_NATIVE_HASH_FIELD_ETHERTYPE;
    enum_values_capability->list[4] = SAI_NATIVE_HASH_FIELD_VLAN_ID;
    enum_values_capability->list[5] = SAI_NATIVE_HASH_FIELD_IP_PROTOCOL;
    enum_values_capability->list[6] = SAI_NATIVE_HASH_FIELD_DST_IP;
    enum_values_capability->list[7] = SAI_NATIVE_HASH_FIELD_SRC_IP;
    enum_values_capability->list[8] = SAI_NATIVE_HASH_FIELD_L4_DST_PORT;
    enum_values_capability->list[9] = SAI_NATIVE_HASH_FIELD_L4_SRC_PORT;
    enum_values_capability->list[10] = SAI_NATIVE_HASH_FIELD_INNER_DST_MAC;
    enum_values_capability->list[11] = SAI_NATIVE_HASH_FIELD_INNER_SRC_MAC;
    enum_values_capability->list[12] = SAI_NATIVE_HASH_FIELD_INNER_ETHERTYPE;
    enum_values_capability->list[13] = SAI_NATIVE_HASH_FIELD_INNER_IP_PROTOCOL;
    enum_values_capability->list[14] = SAI_NATIVE_HASH_FIELD_INNER_DST_IP;
    enum_values_capability->list[15] = SAI_NATIVE_HASH_FIELD_INNER_SRC_IP;
    enum_values_capability->list[16] = SAI_NATIVE_HASH_FIELD_INNER_L4_DST_PORT;
    enum_values_capability->list[17] = SAI_NATIVE_HASH_FIELD_INNER_L4_SRC_PORT;
    enum_values_capability->list[18] = SAI_NATIVE_HASH_FIELD_IPV6_FLOW_LABEL;

    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchStateBase::querySwitchHashAlgorithmCapability(
                   _Inout_ sai_s32_list_t *enum_values_capability)
{
    SWSS_LOG_ENTER();

    if (enum_values_capability->count < 7)
    {
        enum_values_capability->count = 7;
        return SAI_STATUS_BUFFER_OVERFLOW;
    }

    enum_values_capability->count = 7;
    enum_values_capability->list[0] = SAI_HASH_ALGORITHM_CRC;
    enum_values_capability->list[1] = SAI_HASH_ALGORITHM_XOR;
    enum_values_capability->list[2] = SAI_HASH_ALGORITHM_RANDOM;
    enum_values_capability->list[3] = SAI_HASH_ALGORITHM_CRC_32LO;
    enum_values_capability->list[4] = SAI_HASH_ALGORITHM_CRC_32HI;
    enum_values_capability->list[5] = SAI_HASH_ALGORITHM_CRC_CCITT;
    enum_values_capability->list[6] = SAI_HASH_ALGORITHM_CRC_XOR;

    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchStateBase::queryAttrEnumValuesCapability(
                              _In_ sai_object_id_t switch_id,
                              _In_ sai_object_type_t object_type,
                              _In_ sai_attr_id_t attr_id,
                              _Inout_ sai_s32_list_t *enum_values_capability)
{
    SWSS_LOG_ENTER();

    if (object_type == SAI_OBJECT_TYPE_TUNNEL && attr_id == SAI_TUNNEL_ATTR_PEER_MODE)
    {
        return queryTunnelPeerModeCapability(enum_values_capability);
    }
    else if (object_type == SAI_OBJECT_TYPE_VLAN && (attr_id == SAI_VLAN_ATTR_UNKNOWN_UNICAST_FLOOD_CONTROL_TYPE ||
                                                     attr_id == SAI_VLAN_ATTR_UNKNOWN_MULTICAST_FLOOD_CONTROL_TYPE ||
                                                     attr_id == SAI_VLAN_ATTR_BROADCAST_FLOOD_CONTROL_TYPE))
    {
        return queryVlanfloodTypeCapability(enum_values_capability);
    }
    else if (object_type == SAI_OBJECT_TYPE_NEXT_HOP_GROUP && attr_id == SAI_NEXT_HOP_GROUP_ATTR_TYPE)
    {
        return queryNextHopGroupTypeCapability(enum_values_capability);
    }
    else if (object_type == SAI_OBJECT_TYPE_HASH && attr_id == SAI_HASH_ATTR_NATIVE_HASH_FIELD_LIST)
    {
        return queryHashNativeHashFieldListCapability(enum_values_capability);
    }
    else if (object_type == SAI_OBJECT_TYPE_SWITCH && (attr_id == SAI_SWITCH_ATTR_ECMP_DEFAULT_HASH_ALGORITHM ||
                                                       attr_id == SAI_SWITCH_ATTR_LAG_DEFAULT_HASH_ALGORITHM))
    {
        return querySwitchHashAlgorithmCapability(enum_values_capability);
    }

    return SAI_STATUS_NOT_SUPPORTED;
}
sai_status_t SwitchStateBase::queryAttributeCapability(
                              _In_ sai_object_id_t switch_id,
                              _In_ sai_object_type_t object_type,
                              _In_ sai_attr_id_t attr_id,
                              _Out_ sai_attr_capability_t *attr_capability)
{
    SWSS_LOG_ENTER();

    if (object_type == SAI_OBJECT_TYPE_PORT && attr_id == SAI_PORT_ATTR_AUTO_NEG_FEC_MODE_OVERRIDE)
    {
        return queryPortAutonegFecOverrideSupportCapability(attr_capability);
    }

    attr_capability->create_implemented = true;
    attr_capability->set_implemented    = true;
    attr_capability->get_implemented    = true;

    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchStateBase::queryStatsCapability(
                              _In_ sai_object_id_t switchId,
                              _In_ sai_object_type_t objectType,
                              _Inout_ sai_stat_capability_list_t *stats_capability)
{
    SWSS_LOG_ENTER();
    uint32_t i = 0;
    uint32_t stats_count = 0;

    if (objectType == SAI_OBJECT_TYPE_QUEUE)
    {
        stats_count = SAI_QUEUE_STAT_DELAY_WATERMARK_NS;
        if (stats_capability->count < stats_count )
        {
            stats_capability->count = stats_count;
            return SAI_STATUS_BUFFER_OVERFLOW;
        }

        stats_capability->count = stats_count;

        for(i = 0; i < stats_capability->count; i++)
        {
            stats_capability->list[i].stat_modes = SAI_STATS_MODE_READ_AND_CLEAR | SAI_STATS_MODE_READ ;
            stats_capability->list[i].stat_enum = i;
        }

        return SAI_STATUS_SUCCESS;
    }
    else if (objectType == SAI_OBJECT_TYPE_PORT)
    {
        if (stats_capability->count < 91)
        {
            stats_capability->count = 91;
            return SAI_STATUS_BUFFER_OVERFLOW;
        }

        stats_capability->count = 91;
        stats_capability->list[0].stat_enum = SAI_PORT_STAT_IF_IN_OCTETS;
        stats_capability->list[1].stat_enum = SAI_PORT_STAT_IF_IN_UCAST_PKTS;
        stats_capability->list[2].stat_enum = SAI_PORT_STAT_IF_IN_NON_UCAST_PKTS;
        stats_capability->list[3].stat_enum = SAI_PORT_STAT_IF_IN_DISCARDS;
        stats_capability->list[4].stat_enum = SAI_PORT_STAT_IF_IN_ERRORS;
        stats_capability->list[5].stat_enum = SAI_PORT_STAT_IF_IN_UNKNOWN_PROTOS;
        stats_capability->list[6].stat_enum = SAI_PORT_STAT_IF_IN_BROADCAST_PKTS;
        stats_capability->list[7].stat_enum = SAI_PORT_STAT_IF_IN_MULTICAST_PKTS;
        stats_capability->list[8].stat_enum = SAI_PORT_STAT_IF_IN_VLAN_DISCARDS;
        stats_capability->list[9].stat_enum = SAI_PORT_STAT_IF_OUT_OCTETS;
        stats_capability->list[10].stat_enum = SAI_PORT_STAT_IF_OUT_UCAST_PKTS;
        stats_capability->list[11].stat_enum = SAI_PORT_STAT_IF_OUT_NON_UCAST_PKTS;
        stats_capability->list[12].stat_enum = SAI_PORT_STAT_IF_OUT_DISCARDS;
        stats_capability->list[13].stat_enum = SAI_PORT_STAT_IF_OUT_ERRORS;
        stats_capability->list[14].stat_enum = SAI_PORT_STAT_IF_OUT_QLEN;
        stats_capability->list[15].stat_enum = SAI_PORT_STAT_IF_OUT_BROADCAST_PKTS;
        stats_capability->list[16].stat_enum = SAI_PORT_STAT_IF_OUT_MULTICAST_PKTS;
        stats_capability->list[17].stat_enum = SAI_PORT_STAT_ETHER_STATS_DROP_EVENTS;
        stats_capability->list[18].stat_enum = SAI_PORT_STAT_ETHER_STATS_MULTICAST_PKTS;
        stats_capability->list[19].stat_enum = SAI_PORT_STAT_ETHER_STATS_BROADCAST_PKTS;
        stats_capability->list[20].stat_enum = SAI_PORT_STAT_ETHER_STATS_UNDERSIZE_PKTS;
        stats_capability->list[21].stat_enum = SAI_PORT_STAT_ETHER_STATS_FRAGMENTS;
        stats_capability->list[22].stat_enum = SAI_PORT_STAT_ETHER_STATS_PKTS_64_OCTETS;
        stats_capability->list[23].stat_enum = SAI_PORT_STAT_ETHER_STATS_PKTS_65_TO_127_OCTETS;
        stats_capability->list[24].stat_enum = SAI_PORT_STAT_ETHER_STATS_PKTS_128_TO_255_OCTETS;
        stats_capability->list[25].stat_enum = SAI_PORT_STAT_ETHER_STATS_PKTS_256_TO_511_OCTETS;
        stats_capability->list[26].stat_enum = SAI_PORT_STAT_ETHER_STATS_PKTS_512_TO_1023_OCTETS;
        stats_capability->list[27].stat_enum = SAI_PORT_STAT_ETHER_STATS_PKTS_1024_TO_1518_OCTETS;
        stats_capability->list[28].stat_enum = SAI_PORT_STAT_ETHER_STATS_PKTS_1519_TO_2047_OCTETS;
        stats_capability->list[29].stat_enum = SAI_PORT_STAT_ETHER_STATS_PKTS_2048_TO_4095_OCTETS;
        stats_capability->list[30].stat_enum = SAI_PORT_STAT_ETHER_STATS_PKTS_4096_TO_9216_OCTETS;
        stats_capability->list[31].stat_enum = SAI_PORT_STAT_ETHER_STATS_PKTS_9217_TO_16383_OCTETS;
        stats_capability->list[32].stat_enum = SAI_PORT_STAT_ETHER_STATS_OVERSIZE_PKTS;
        stats_capability->list[33].stat_enum = SAI_PORT_STAT_ETHER_RX_OVERSIZE_PKTS;
        stats_capability->list[34].stat_enum = SAI_PORT_STAT_ETHER_TX_OVERSIZE_PKTS;
        stats_capability->list[35].stat_enum = SAI_PORT_STAT_ETHER_STATS_JABBERS;
        stats_capability->list[36].stat_enum = SAI_PORT_STAT_ETHER_STATS_OCTETS;
        stats_capability->list[37].stat_enum = SAI_PORT_STAT_ETHER_STATS_PKTS;
        stats_capability->list[38].stat_enum = SAI_PORT_STAT_ETHER_STATS_COLLISIONS;
        stats_capability->list[39].stat_enum = SAI_PORT_STAT_ETHER_STATS_CRC_ALIGN_ERRORS;
        stats_capability->list[40].stat_enum = SAI_PORT_STAT_ETHER_STATS_TX_NO_ERRORS;
        stats_capability->list[41].stat_enum = SAI_PORT_STAT_ETHER_STATS_RX_NO_ERRORS;
        stats_capability->list[42].stat_enum = SAI_PORT_STAT_GREEN_WRED_DROPPED_PACKETS;
        stats_capability->list[43].stat_enum = SAI_PORT_STAT_GREEN_WRED_DROPPED_BYTES;
        stats_capability->list[44].stat_enum = SAI_PORT_STAT_YELLOW_WRED_DROPPED_PACKETS;
        stats_capability->list[45].stat_enum = SAI_PORT_STAT_YELLOW_WRED_DROPPED_BYTES;
        stats_capability->list[46].stat_enum = SAI_PORT_STAT_RED_WRED_DROPPED_PACKETS;
        stats_capability->list[47].stat_enum = SAI_PORT_STAT_RED_WRED_DROPPED_BYTES;
        stats_capability->list[48].stat_enum = SAI_PORT_STAT_WRED_DROPPED_PACKETS;
        stats_capability->list[49].stat_enum = SAI_PORT_STAT_WRED_DROPPED_BYTES;
        stats_capability->list[50].stat_enum = SAI_PORT_STAT_ECN_MARKED_PACKETS;
        stats_capability->list[51].stat_enum = SAI_PORT_STAT_PFC_0_RX_PKTS;
        stats_capability->list[52].stat_enum = SAI_PORT_STAT_PFC_0_TX_PKTS;
        stats_capability->list[53].stat_enum = SAI_PORT_STAT_PFC_1_RX_PKTS;
        stats_capability->list[54].stat_enum = SAI_PORT_STAT_PFC_1_TX_PKTS;
        stats_capability->list[55].stat_enum = SAI_PORT_STAT_PFC_2_RX_PKTS;
        stats_capability->list[56].stat_enum = SAI_PORT_STAT_PFC_2_TX_PKTS;
        stats_capability->list[57].stat_enum = SAI_PORT_STAT_PFC_3_RX_PKTS;
        stats_capability->list[58].stat_enum = SAI_PORT_STAT_PFC_3_TX_PKTS;
        stats_capability->list[59].stat_enum = SAI_PORT_STAT_PFC_4_RX_PKTS;
        stats_capability->list[60].stat_enum = SAI_PORT_STAT_PFC_4_TX_PKTS;
        stats_capability->list[61].stat_enum = SAI_PORT_STAT_PFC_5_RX_PKTS;
        stats_capability->list[62].stat_enum = SAI_PORT_STAT_PFC_5_TX_PKTS;
        stats_capability->list[63].stat_enum = SAI_PORT_STAT_PFC_6_RX_PKTS;
        stats_capability->list[64].stat_enum = SAI_PORT_STAT_PFC_6_TX_PKTS;
        stats_capability->list[65].stat_enum = SAI_PORT_STAT_PFC_7_RX_PKTS;
        stats_capability->list[66].stat_enum = SAI_PORT_STAT_PFC_7_TX_PKTS;
        stats_capability->list[67].stat_enum = SAI_PORT_STAT_PFC_0_RX_PAUSE_DURATION_US;
        stats_capability->list[68].stat_enum = SAI_PORT_STAT_PFC_0_TX_PAUSE_DURATION_US;
        stats_capability->list[69].stat_enum = SAI_PORT_STAT_PFC_1_RX_PAUSE_DURATION_US;
        stats_capability->list[70].stat_enum = SAI_PORT_STAT_PFC_1_TX_PAUSE_DURATION_US;
        stats_capability->list[71].stat_enum = SAI_PORT_STAT_PFC_2_RX_PAUSE_DURATION_US;
        stats_capability->list[72].stat_enum = SAI_PORT_STAT_PFC_2_TX_PAUSE_DURATION_US;
        stats_capability->list[73].stat_enum = SAI_PORT_STAT_PFC_3_RX_PAUSE_DURATION_US;
        stats_capability->list[74].stat_enum = SAI_PORT_STAT_PFC_3_TX_PAUSE_DURATION_US;
        stats_capability->list[75].stat_enum = SAI_PORT_STAT_PFC_4_RX_PAUSE_DURATION_US;
        stats_capability->list[76].stat_enum = SAI_PORT_STAT_PFC_4_TX_PAUSE_DURATION_US;
        stats_capability->list[77].stat_enum = SAI_PORT_STAT_PFC_5_RX_PAUSE_DURATION_US;
        stats_capability->list[78].stat_enum = SAI_PORT_STAT_PFC_5_TX_PAUSE_DURATION_US;
        stats_capability->list[79].stat_enum = SAI_PORT_STAT_PFC_6_RX_PAUSE_DURATION_US;
        stats_capability->list[80].stat_enum = SAI_PORT_STAT_PFC_6_TX_PAUSE_DURATION_US;
        stats_capability->list[81].stat_enum = SAI_PORT_STAT_PFC_7_RX_PAUSE_DURATION_US;
        stats_capability->list[82].stat_enum = SAI_PORT_STAT_PFC_7_TX_PAUSE_DURATION_US;
        stats_capability->list[83].stat_enum = SAI_PORT_STAT_PFC_0_ON2OFF_RX_PKTS;
        stats_capability->list[84].stat_enum = SAI_PORT_STAT_PFC_1_ON2OFF_RX_PKTS;
        stats_capability->list[85].stat_enum = SAI_PORT_STAT_PFC_2_ON2OFF_RX_PKTS;
        stats_capability->list[86].stat_enum = SAI_PORT_STAT_PFC_3_ON2OFF_RX_PKTS;
        stats_capability->list[87].stat_enum = SAI_PORT_STAT_PFC_4_ON2OFF_RX_PKTS;
        stats_capability->list[88].stat_enum = SAI_PORT_STAT_PFC_5_ON2OFF_RX_PKTS;
        stats_capability->list[89].stat_enum = SAI_PORT_STAT_PFC_6_ON2OFF_RX_PKTS;
        stats_capability->list[90].stat_enum = SAI_PORT_STAT_PFC_7_ON2OFF_RX_PKTS;

        for(i = 0; i < stats_capability->count; i++)
        {
            stats_capability->list[i].stat_modes = SAI_STATS_MODE_READ_AND_CLEAR | SAI_STATS_MODE_READ ;
        }

        return SAI_STATUS_SUCCESS;
    }

    return SAI_STATUS_NOT_SUPPORTED;
}

sai_status_t SwitchStateBase::queryStatsStCapability(
    _In_ sai_object_id_t switchId,
    _In_ sai_object_type_t objectType,
    _Inout_ sai_stat_st_capability_list_t *stats_st_capability)
{
    SWSS_LOG_ENTER();

    sai_stat_capability_list_t stats_capability;
    std::vector<sai_stat_capability_t> stats_list(stats_st_capability->count);
    stats_capability.count = stats_st_capability->count;
    stats_capability.list = stats_list.data();

    sai_status_t status = queryStatsCapability(
        switchId,
        objectType,
        &stats_capability);

    if (status == SAI_STATUS_SUCCESS)
    {
        for (uint32_t i = 0; i < stats_capability.count; i++)
        {
            stats_st_capability->list[i].capability.stat_enum = stats_capability.list[i].stat_enum;
            stats_st_capability->list[i].capability.stat_modes = stats_capability.list[i].stat_modes;
            stats_st_capability->list[i].minimal_polling_interval = 1e6 * 100; // 100ms
        }
    }
    else
    {
        SWSS_LOG_WARN("Failed to query stats capability for object type %s, status: %s",
            sai_serialize_object_type(objectType).c_str(),
            sai_serialize_status(status).c_str());
    }

    return status;
}

void SwitchStateBase::send_tam_tel_type_config_change(
    _In_ sai_object_id_t tam_tel_type_id)
{
    SWSS_LOG_ENTER();

    auto meta = getMeta();

    if (meta)
    {
        meta->meta_sai_on_tam_tel_type_config_change(tam_tel_type_id);
    }

    sai_attribute_t attr;

    attr.id = SAI_SWITCH_ATTR_TAM_TEL_TYPE_CONFIG_CHANGE_NOTIFY;

    sai_status_t status = get(SAI_OBJECT_TYPE_SWITCH, m_switch_id, 1, &attr);

    if (status != SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_ERROR("unable to get SAI_SWITCH_ATTR_TAM_TEL_TYPE_CONFIG_CHANGE_NOTIFY attribute for %s",
                       sai_serialize_object_id(m_switch_id).c_str());

        return;
    }

    auto str = sai_serialize_object_id(tam_tel_type_id);

    sai_switch_notifications_t sn = {};

    sn.on_tam_tel_type_config_change = (sai_tam_tel_type_config_change_notification_fn)attr.value.ptr;

    SWSS_LOG_INFO("send event SAI_SWITCH_ATTR_TAM_TEL_TYPE_CONFIG_CHANGE_NOTIFY %s", str.c_str());

    auto ntf = std::make_shared<sairedis::NotificationTamTelTypeConfigChange>(str);

    auto payload = std::make_shared<EventPayloadNotification>(ntf, sn);

    m_switchConfig->m_eventQueue->enqueue(std::make_shared<Event>(EVENT_TYPE_NOTIFICATION, payload));
}