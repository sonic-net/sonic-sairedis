#include "SwitchVpp.h"

#include "meta/sai_serialize.h"

#include "swss/logger.h"

#include "vppxlate/SaiIntfStats.h"

using namespace saivs;

// TODO init vpp

SwitchVpp::SwitchVpp(
        _In_ sai_object_id_t switch_id,
        _In_ std::shared_ptr<RealObjectIdManager> manager,
        _In_ std::shared_ptr<SwitchConfig> config):
    SwitchStateBase(switch_id, manager, config),
    m_object_db(this),
    m_tunnel_mgr(this)
{
    SWSS_LOG_ENTER();

    vpp_dp_initialize();
}

SwitchVpp::SwitchVpp(
        _In_ sai_object_id_t switch_id,
        _In_ std::shared_ptr<RealObjectIdManager> manager,
        _In_ std::shared_ptr<SwitchConfig> config,
        _In_ std::shared_ptr<WarmBootState> warmBootState):
    SwitchStateBase(switch_id, manager, config, warmBootState),
    m_object_db(this),
    m_tunnel_mgr(this)
{
    SWSS_LOG_ENTER();

    vpp_dp_initialize();
}

bool SwitchVpp::port_to_hostif_list(
        _In_ sai_object_id_t port_id,
        _Inout_ std::string& if_name)
{
    SWSS_LOG_ENTER();

    // TODO to be removed and inlined

    //sai_object_id_t switch_id = switchIdQuery(port_id);
    //if (switch_id == SAI_NULL_OBJECT_ID) {
    //return false;
    //}
    //auto it = m_switchStateMap.find(switch_id);
    //if (it == m_switchStateMap.end()) {
    //return false;
    //}
    //auto sw = it->second;
    //if (sw == nullptr) {
    //return false;
    //}
    //return(
    return getTapNameFromPortId(port_id, if_name);
}

bool SwitchVpp::port_to_hwifname(
        _In_ sai_object_id_t port_id,
        _Inout_ std::string& if_name)
{
    SWSS_LOG_ENTER();

    // TODO to be removed and inlined

    // sai_object_id_t switch_id = switchIdQuery(port_id);
    // if (switch_id == SAI_NULL_OBJECT_ID) {
    // return false;
    // }
    // auto it = m_switchStateMap.find(switch_id);
    // if (it == m_switchStateMap.end()) {
    // return false;
    // }
    // auto sw = it->second;
    // if (sw == nullptr) {
    // return false;
    // }

    return vpp_get_hwif_name(port_id, 0, if_name);
}

void SwitchVpp::setPortStats(
        _In_ sai_object_id_t oid)
{
    std::map<sai_stat_id_t, uint64_t> stats;

    std::string if_name;

    if (!port_to_hwifname(oid, if_name))
    {
        return;
    }

    vpp_interface_stats_t port_stats;

    if (vpp_intf_stats_query(if_name.c_str(), &port_stats) == 0)
    {
        stats[SAI_PORT_STAT_IF_IN_OCTETS] = port_stats.rx_bytes;
        stats[SAI_PORT_STAT_IF_IN_UCAST_PKTS] = port_stats.rx;
        stats[SAI_PORT_STAT_IF_IN_BROADCAST_PKTS] = port_stats.rx_broadcast;
        stats[SAI_PORT_STAT_IF_IN_MULTICAST_PKTS] = port_stats.rx_multicast;
        stats[SAI_PORT_STAT_IF_IN_DISCARDS] = port_stats.drops;
        stats[SAI_PORT_STAT_IF_OUT_OCTETS] = port_stats.tx_bytes;
        stats[SAI_PORT_STAT_IF_OUT_UCAST_PKTS] = port_stats.tx;
        stats[SAI_PORT_STAT_IF_OUT_BROADCAST_PKTS] = port_stats.tx_broadcast;
        stats[SAI_PORT_STAT_IF_OUT_MULTICAST_PKTS] = port_stats.tx_multicast;

        stats[SAI_PORT_STAT_IN_DROPPED_PKTS] = port_stats.rx_no_buf;
        stats[SAI_PORT_STAT_IF_IN_ERRORS] = port_stats.rx_error;
        stats[SAI_PORT_STAT_IF_OUT_ERRORS] = port_stats.tx_error;
        stats[SAI_PORT_STAT_IP_IN_RECEIVES] = port_stats.ip4;
        stats[SAI_PORT_STAT_IPV6_IN_RECEIVES] = port_stats.ip6;
    }

    debugSetStats(oid, stats);
}

sai_status_t SwitchVpp::queryAttributeCapability(
        _In_ sai_object_id_t switch_id,
        _In_ sai_object_type_t object_type,
        _In_ sai_attr_id_t attr_id,
        _Out_ sai_attr_capability_t *capability)
{
    SWSS_LOG_ENTER();

    // TODO: We should generate this metadata for the virtual switch rather
    // than hard-coding it here.

    // in virtual switch by default all apis are implemented for all objects. SUCCESS for all attributes

    capability->create_implemented = true;
    capability->set_implemented    = true;
    capability->get_implemented    = true;

    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchVpp::getStatsExt(
        _In_ sai_object_type_t object_type,
        _In_ sai_object_id_t object_id,
        _In_ uint32_t number_of_counters,
        _In_ const sai_stat_id_t *counter_ids,
        _In_ sai_stats_mode_t mode,
        _Out_ uint64_t *counters)
{
    SWSS_LOG_ENTER();

    if (object_type == SAI_OBJECT_TYPE_PORT)
    {
        setPortStats(object_id);
    }

    return SwitchStateBase::getStatsExt(
            object_type,
            object_id,
            number_of_counters,
            counter_ids,
            mode,
            counters);
}

void SwitchVpp::processFdbEntriesForAging()
{
    SWSS_LOG_ENTER();

    return;
}

sai_status_t SwitchVpp::create(
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

    if (object_type == SAI_OBJECT_TYPE_ROUTER_INTERFACE)
    {
        sai_object_id_t object_id;
        sai_deserialize_object_id(serializedObjectId, object_id);
        return createRouterif(object_id, switch_id, attr_count, attr_list);
    }

    if (object_type == SAI_OBJECT_TYPE_ROUTE_ENTRY)
    {
        return addIpRoute(serializedObjectId, switch_id, attr_count, attr_list);
    }

    if (object_type == SAI_OBJECT_TYPE_NEXT_HOP)
    {
        return createNexthop(serializedObjectId, switch_id, attr_count, attr_list);
    }

    if (object_type == SAI_OBJECT_TYPE_NEXT_HOP_GROUP_MEMBER)
    {
        return createNexthopGroupMember(serializedObjectId, switch_id, attr_count, attr_list);
    }

    if (object_type == SAI_OBJECT_TYPE_NEIGHBOR_ENTRY)
    {
        return addIpNbr(serializedObjectId, switch_id, attr_count, attr_list);
    }

    if (object_type == SAI_OBJECT_TYPE_ACL_ENTRY)
    {
        sai_object_id_t object_id;
        sai_deserialize_object_id(serializedObjectId, object_id);
        return createAclEntry(object_id, switch_id, attr_count, attr_list);
    }

    if (object_type == SAI_OBJECT_TYPE_ACL_TABLE)
    {
        sai_object_id_t object_id;
        sai_deserialize_object_id(serializedObjectId, object_id);
        return aclTableCreate(object_id, switch_id, attr_count, attr_list);
    }

    if (object_type == SAI_OBJECT_TYPE_ACL_TABLE_GROUP_MEMBER)
    {
        sai_object_id_t object_id;
        sai_deserialize_object_id(serializedObjectId, object_id);
        return createAclGrpMbr(object_id, switch_id, attr_count, attr_list);
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

    if (object_type == SAI_OBJECT_TYPE_VLAN_MEMBER)
    {
        sai_object_id_t object_id;
        sai_deserialize_object_id(serializedObjectId, object_id);
        return createVlanMember(object_id, switch_id, attr_count, attr_list);
    }

    if (object_type == SAI_OBJECT_TYPE_FDB_ENTRY)
    {
        return FdbEntryadd(serializedObjectId, switch_id, attr_count, attr_list);
    }

    if (object_type == SAI_OBJECT_TYPE_BFD_SESSION)
    {
        return bfd_session_add(serializedObjectId, switch_id, attr_count, attr_list);
    }

    return create_internal(object_type, serializedObjectId, switch_id, attr_count, attr_list);
}

sai_status_t SwitchVpp::create_internal(
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

    m_object_db.create_or_update(object_type, serializedObjectId, attr_count, attr_list, true /*is_create*/);

    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchVpp::createPort(
        _In_ sai_object_id_t object_id,
        _In_ sai_object_id_t switch_id,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    UpdatePort(object_id, attr_count, attr_list);

    auto sid = sai_serialize_object_id(object_id);

    CHECK_STATUS(create_internal(SAI_OBJECT_TYPE_PORT, sid, switch_id, attr_count, attr_list));

    return create_port_dependencies(object_id);
}


sai_status_t SwitchVpp::remove(
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

    if (object_type == SAI_OBJECT_TYPE_ROUTER_INTERFACE)
    {
        sai_object_id_t objectId;
        sai_deserialize_object_id(serializedObjectId, objectId);
        return removeRouterif(objectId);
    }

    if (object_type == SAI_OBJECT_TYPE_VIRTUAL_ROUTER)
    {
        sai_object_id_t objectId;
        sai_deserialize_object_id(serializedObjectId, objectId);
        return removeVrf(objectId);
    }

    if (object_type == SAI_OBJECT_TYPE_ROUTE_ENTRY)
    {
        return removeIpRoute(serializedObjectId);
    }

    if (object_type == SAI_OBJECT_TYPE_NEXT_HOP)
    {
        return removeNexthop(serializedObjectId);
    }

    if (object_type == SAI_OBJECT_TYPE_NEXT_HOP_GROUP_MEMBER)
    {
        return removeNexthopGroupMember(serializedObjectId);
    }

    if (object_type == SAI_OBJECT_TYPE_NEIGHBOR_ENTRY)
    {
        return removeIpNbr(serializedObjectId);
    }

    if (object_type == SAI_OBJECT_TYPE_ACL_ENTRY)
    {
        return removeAclEntry(serializedObjectId);
    }

    if (object_type == SAI_OBJECT_TYPE_ACL_TABLE)
    {
        return aclTableRemove(serializedObjectId);
    }

    if (object_type == SAI_OBJECT_TYPE_ACL_TABLE_GROUP_MEMBER)
    {
        return removeAclGrpMbr(serializedObjectId);
    }

    if (object_type == SAI_OBJECT_TYPE_ACL_TABLE_GROUP)
    {
        return removeAclGrp(serializedObjectId);
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

    if (object_type == SAI_OBJECT_TYPE_VLAN_MEMBER)
    {
        sai_object_id_t objectId;
        sai_deserialize_object_id(serializedObjectId, objectId);
        return removeVlanMember(objectId);
    }
    else if (object_type == SAI_OBJECT_TYPE_FDB_ENTRY)
    {
        return FdbEntrydel(serializedObjectId);
    }
    else if (object_type == SAI_OBJECT_TYPE_BFD_SESSION)
    {
        return bfd_session_del(serializedObjectId);
    }

    return remove_internal(object_type, serializedObjectId);
}

sai_status_t SwitchVpp::remove_internal(
        _In_ sai_object_type_t object_type,
        _In_ const std::string &serializedObjectId)
{
    SWSS_LOG_ENTER();

    SWSS_LOG_INFO("removing object: %s", serializedObjectId.c_str());

    m_object_db.remove(object_type, serializedObjectId);

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

sai_status_t SwitchVpp::setPort(
        _In_ sai_object_id_t portId,
        _In_ const sai_attribute_t* attr)
{
    SWSS_LOG_ENTER();

    UpdatePort(portId, 1, attr);

    auto sid = sai_serialize_object_id(portId);

    return set_internal(SAI_OBJECT_TYPE_PORT, sid, attr);
}

sai_status_t SwitchVpp::setAclEntry(
        _In_ sai_object_id_t entry_id,
        _In_ const sai_attribute_t* attr)
{
    SWSS_LOG_ENTER();

    if (attr && attr->id == SAI_ACL_ENTRY_ATTR_ACTION_MACSEC_FLOW)
    {
        return setAclEntryMACsecFlowActive(entry_id, attr);
    }

    auto sid = sai_serialize_object_id(entry_id);

    set_internal(SAI_OBJECT_TYPE_ACL_ENTRY, sid, attr);

    sai_object_id_t tbl_oid;

    if (getAclTableId(entry_id, &tbl_oid) != SAI_STATUS_SUCCESS)
    {
        return SAI_STATUS_FAILURE;
    }

    auto status = AclAddRemoveCheck(tbl_oid);

    SWSS_LOG_NOTICE("ACL entry %s set in table %s set status %d",
            sid.c_str(),
            sai_serialize_object_id(tbl_oid).c_str(),
            status);

    return status;
}

sai_status_t SwitchVpp::set(
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

    if (objectType == SAI_OBJECT_TYPE_ROUTER_INTERFACE)
    {
        sai_object_id_t objectId;
        sai_deserialize_object_id(serializedObjectId, objectId);
        return vpp_update_router_interface(objectId, 1, attr);
    }

    if (objectType == SAI_OBJECT_TYPE_ACL_TABLE_GROUP_MEMBER)
    {
        sai_object_id_t objectId;
        sai_deserialize_object_id(serializedObjectId, objectId);
        return setAclGrpMbr(objectId, attr);
    }

    if (objectType == SAI_OBJECT_TYPE_ROUTE_ENTRY)
    {
        return updateIpRoute(serializedObjectId, attr);
    }

    if (objectType == SAI_OBJECT_TYPE_SWITCH)
    {
        switch(attr->id)
        {
            case SAI_SWITCH_ATTR_VXLAN_DEFAULT_ROUTER_MAC:
                {
                    m_tunnel_mgr.set_router_mac(attr);
                    break;
                }
            case SAI_SWITCH_ATTR_VXLAN_DEFAULT_PORT:
                {
                    m_tunnel_mgr.set_vxlan_port(attr);
                    break;
                }
        }
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

    return set_internal(objectType, serializedObjectId, attr);
}

sai_status_t SwitchVpp::set_internal(
        _In_ sai_object_type_t objectType,
        _In_ const std::string &serializedObjectId,
        _In_ const sai_attribute_t* attr)
{
    SWSS_LOG_ENTER();

    //Update child-parent relationship before updating the attribute
    m_object_db.create_or_update(objectType, serializedObjectId, 1, attr, false /*is_create*/);

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

sai_status_t SwitchVpp::get(
        _In_ sai_object_type_t objectType,
        _In_ const std::string &serializedObjectId,
        _In_ uint32_t attr_count,
        _Out_ sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    if (objectType == SAI_OBJECT_TYPE_ACL_COUNTER)
    {
        sai_object_id_t object_id;

        sai_deserialize_object_id(serializedObjectId, object_id);
        return getAclEntryStats(object_id, attr_count, attr_list);
    }

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
            return SAI_STATUS_ITEM_NOT_FOUND;

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

sai_status_t SwitchVpp::bulkCreate(
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
        object_statuses[it] = create_internal(object_type, serialized_object_ids[it], switch_id, attr_count[it], attr_list[it]);

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

sai_status_t SwitchVpp::bulkRemove(
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
        object_statuses[it] = remove_internal(object_type, serialized_object_ids[it]);

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

sai_status_t SwitchVpp::get_max(
        _In_ sai_object_type_t objectType,
        _In_ const std::string &serializedObjectId,
        _In_ const uint32_t  max_attr_count,
        _Out_ uint32_t *attr_count,
        _Out_ sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    *attr_count = 0;

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

    sai_status_t final_status = SAI_STATUS_SUCCESS, status;
    uint32_t idx = 0;
    sai_attribute_t *dst_attr;

    for (auto &kvp: attrHash)
    {
        auto attr = kvp.second->getAttr();

        dst_attr = &attr_list[idx];
        dst_attr->id = attr->id;

        status = transfer_attributes(objectType, 1, attr, dst_attr, false);

        if (status == SAI_STATUS_BUFFER_OVERFLOW)
        {
            /*
             * This is considered partial success, since we get correct list
             * length.  Note that other items ARE processes on the list.
             */

            SWSS_LOG_NOTICE("BUFFER_OVERFLOW %s: %d",
                    serializedObjectId.c_str(),
                    attr->id);

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

            SWSS_LOG_ERROR("get failed %s: %d: %s",
                    serializedObjectId.c_str(),
                    attr->id,
                    sai_serialize_status(status).c_str());

            return status;
        }

        ++idx;

        if (idx == max_attr_count)
            break;
    }

    *attr_count = idx;

    return final_status;
}

std::shared_ptr<SaiDBObject> SwitchVpp::get_sai_object(
        _In_ sai_object_type_t object_type,
        _In_ const std::string &serializedObjectId)
{
    SWSS_LOG_ENTER();

    return m_object_db.get(object_type, serializedObjectId);
}
