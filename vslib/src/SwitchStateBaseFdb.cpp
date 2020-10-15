#include "SwitchStateBase.h"
#include "EventPayloadNotification.h"

#include "swss/logger.h"
#include "swss/select.h"

#include "meta/sai_serialize.h"
#include "lib/inc/NotificationFdbEvent.h"

#include <linux/if_ether.h>
#include <arpa/inet.h>

using namespace saivs;

void SwitchStateBase::updateLocalDB(
        _In_ const sai_fdb_event_notification_data_t &data,
        _In_ sai_fdb_event_t fdb_event)
{
    SWSS_LOG_ENTER();

    sai_status_t status;

    switch (fdb_event)
    {
        case SAI_FDB_EVENT_LEARNED:

            {
                auto sid = sai_serialize_fdb_entry(data.fdb_entry);

                status = create(SAI_OBJECT_TYPE_FDB_ENTRY, sid, m_switch_id, data.attr_count, data.attr);

                if (status != SAI_STATUS_SUCCESS)
                {
                    SWSS_LOG_ERROR("failed to create fdb entry: %s",
                            sai_serialize_fdb_entry(data.fdb_entry).c_str());
                }
            }

            break;

        case SAI_FDB_EVENT_AGED:

            {
                auto sid = sai_serialize_fdb_entry(data.fdb_entry);

                status = remove(SAI_OBJECT_TYPE_FDB_ENTRY, sid);

                if (status != SAI_STATUS_SUCCESS)
                {
                    SWSS_LOG_ERROR("failed to remove fdb entry %s",
                            sai_serialize_fdb_entry(data.fdb_entry).c_str());
                }
            }

            break;

        default:
            SWSS_LOG_ERROR("unsupported fdb event: %d", fdb_event);
            break;
    }
}

void SwitchStateBase::processFdbInfo(
        _In_ const FdbInfo &fi,
        _In_ sai_fdb_event_t fdb_event)
{
    SWSS_LOG_ENTER();

    sai_attribute_t attrs[2];

    attrs[0].id = SAI_FDB_ENTRY_ATTR_TYPE;
    attrs[0].value.s32 = SAI_FDB_ENTRY_TYPE_DYNAMIC;

    attrs[1].id = SAI_FDB_ENTRY_ATTR_BRIDGE_PORT_ID;
    attrs[1].value.oid = fi.getBridgePortId();

    sai_fdb_event_notification_data_t data;

    data.event_type = fdb_event;

    data.fdb_entry = fi.getFdbEntry();

    data.attr_count = 2;
    data.attr = attrs;

    // update local DB
    updateLocalDB(data, fdb_event); // TODO we could move to send_fdb_event_notification and support flush

    send_fdb_event_notification(data);
}

void SwitchStateBase::findBridgeVlanForPortVlan(
        _In_ sai_object_id_t port_id,
        _In_ sai_vlan_id_t vlan_id,
        _Inout_ sai_object_id_t &bv_id,
        _Inout_ sai_object_id_t &bridge_port_id)
{
    SWSS_LOG_ENTER();

    bv_id = SAI_NULL_OBJECT_ID;
    bridge_port_id = SAI_NULL_OBJECT_ID;

    sai_object_id_t bridge_id;

    /*
     * The bridge port lookup process is two steps:
     *
     * - use (vlan_id, physical port_id) to match any .1D bridge port created.
     *   If there is match, then quit, found=true
     *
     * - use (physical port_id) to match any .1Q bridge created. if there is a
     *   match, the quite, found=true.
     *
     * If found==true, generate fdb learn event on the .1D or .1Q bridge port.
     * If not found, then do not generate fdb event. It means the packet is not
     * received on the bridge port.
     *
     * XXX this is not whats happening here, we are just looking for any
     * bridge id (as in our case this is shortcut, we will remove all bridge ports
     * when we will use router interface based port/lag and no bridge
     * will be found.
     */

    auto &objectHash = m_objectHash.at(SAI_OBJECT_TYPE_BRIDGE_PORT);

    // iterate via all bridge ports to find match on port id

    sai_object_id_t lag_id = SAI_NULL_OBJECT_ID;

    if (getLagFromPort(port_id,lag_id))
    {
        SWSS_LOG_INFO("got lag %s for port %s",
                sai_serialize_object_id(lag_id).c_str(),
                sai_serialize_object_id(port_id).c_str());
    }

    bool bv_id_set = false;

    for (auto it = objectHash.begin(); it != objectHash.end(); ++it)
    {
        sai_object_id_t bpid;

        sai_deserialize_object_id(it->first, bpid);

        sai_attribute_t attrs[2];

        attrs[0].id = SAI_BRIDGE_PORT_ATTR_PORT_ID;
        attrs[1].id = SAI_BRIDGE_PORT_ATTR_TYPE;

        sai_status_t status = get(SAI_OBJECT_TYPE_BRIDGE_PORT, bpid, (uint32_t)(sizeof(attrs)/sizeof(attrs[0])), attrs);

        if (status != SAI_STATUS_SUCCESS)
        {
            SWSS_LOG_WARN("failed to get attr PORT_ID and TYPE for bridge port %s",
                    sai_serialize_object_id(bpid).c_str());
            continue;
        }

        if (lag_id != SAI_NULL_OBJECT_ID)
        {
            // if port is member of lag, we should check if port_id is that LAG

            if (port_id == attrs[0].value.oid)
            {
                // there should be no case that the same port is lag member and has bridge port object on it

                SWSS_LOG_ERROR("port %s is member of lag %s, and also has bridge port created: %s",
                        sai_serialize_object_id(port_id).c_str(),
                        sai_serialize_object_id(lag_id).c_str(),
                        sai_serialize_object_id(attrs[0].value.oid).c_str());
                continue;
            }

            if (lag_id != attrs[0].value.oid)
            {
                // this is not expected port
                continue;
            }
        }
        else if (port_id != attrs[0].value.oid)
        {
            // this is not expected port
            continue;
        }

        bridge_port_id = bpid;

        // get the 1D bridge id if the bridge port type is subport
        auto bp_type = attrs[1].value.s32;

        SWSS_LOG_DEBUG("found bridge port %s of type %d",
                sai_serialize_object_id(bridge_port_id).c_str(),
                bp_type);

        if (bp_type == SAI_BRIDGE_PORT_TYPE_SUB_PORT)
        {
            sai_attribute_t attr;
            attr.id = SAI_BRIDGE_PORT_ATTR_BRIDGE_ID;

            status = get(SAI_OBJECT_TYPE_BRIDGE_PORT, bridge_port_id, 1, &attr);

            if (status != SAI_STATUS_SUCCESS)
            {
                break;
            }

            bridge_id = attr.value.oid;

            SWSS_LOG_DEBUG("found bridge %s for port %s",
                    sai_serialize_object_id(bridge_id).c_str(),
                    sai_serialize_object_id(port_id).c_str());

            attr.id = SAI_BRIDGE_ATTR_TYPE;

            status = get(SAI_OBJECT_TYPE_BRIDGE, bridge_id, 1, &attr);

            if (status != SAI_STATUS_SUCCESS)
            {
                break;
            }

            SWSS_LOG_DEBUG("bridge %s type is %d",
                    sai_serialize_object_id(bridge_id).c_str(),
                    attr.value.s32);
            bv_id = bridge_id;
            bv_id_set = true;
        }
        else
        {
            auto &objectHash2 = m_objectHash.at(SAI_OBJECT_TYPE_VLAN);

            // iterate via all vlans to find match on vlan id

            for (auto it2 = objectHash2.begin(); it2 != objectHash2.end(); ++it2)
            {
                sai_object_id_t vlan_oid;

                sai_deserialize_object_id(it2->first, vlan_oid);

                sai_attribute_t attr;
                attr.id = SAI_VLAN_ATTR_VLAN_ID;

                status = get(SAI_OBJECT_TYPE_VLAN, vlan_oid, 1, &attr);

                if (status != SAI_STATUS_SUCCESS)
                {
                    continue;
                }

                if (vlan_id == attr.value.u16)
                {
                    bv_id = vlan_oid;
                    bv_id_set = true;
                    break;
                }
            }
        }

        break;
    }

    if (!bv_id_set)
    {
        // if port is lag member, then we didn't found bridge_port for that lag (expected for rif lag)
        SWSS_LOG_WARN("failed to find bv_id for vlan %d and port_id %s",
                vlan_id,
                sai_serialize_object_id(port_id).c_str());
    }
}

bool SwitchStateBase::getLagFromPort(
        _In_ sai_object_id_t port_id,
        _Inout_ sai_object_id_t& lag_id)
{
    SWSS_LOG_ENTER();

    lag_id = SAI_NULL_OBJECT_ID;

    auto &objectHash = m_objectHash.at(SAI_OBJECT_TYPE_LAG_MEMBER);

    // iterate via all lag members to find match on port id

    for (auto it = objectHash.begin(); it != objectHash.end(); ++it)
    {
        sai_object_id_t lag_member_id;

        sai_deserialize_object_id(it->first, lag_member_id);

        sai_attribute_t attr;

        attr.id = SAI_LAG_MEMBER_ATTR_PORT_ID;

        sai_status_t status = get(SAI_OBJECT_TYPE_LAG_MEMBER, lag_member_id, 1, &attr);

        if (status != SAI_STATUS_SUCCESS)
        {
            SWSS_LOG_ERROR("failed to get port id from leg member %s",
                    sai_serialize_object_id(lag_member_id).c_str());
            continue;
        }

        if (port_id != attr.value.oid)
        {
            // this is not the port we are looking for
            continue;
        }

        attr.id = SAI_LAG_MEMBER_ATTR_LAG_ID;

        status = get(SAI_OBJECT_TYPE_LAG_MEMBER, lag_member_id, 1, &attr);

        if (status != SAI_STATUS_SUCCESS)
        {
            SWSS_LOG_ERROR("failed to get lag id from lag member %s",
                    sai_serialize_object_id(lag_member_id).c_str());
            continue;
        }

        lag_id = attr.value.oid;

        return true;
    }

    // this port does not belong to any lag

    return false;
}

bool SwitchStateBase::isLagOrPortRifBased(
        _In_ sai_object_id_t lag_or_port_id)
{
    SWSS_LOG_ENTER();

    auto &objectHash = m_objectHash.at(SAI_OBJECT_TYPE_ROUTER_INTERFACE);

    // iterate via all lag members to find match on port id

    for (auto it = objectHash.begin(); it != objectHash.end(); ++it)
    {
        sai_object_id_t rif_id;

        sai_deserialize_object_id(it->first, rif_id);

        sai_attribute_t attr;

        attr.id = SAI_ROUTER_INTERFACE_ATTR_TYPE;

        sai_status_t status = get(SAI_OBJECT_TYPE_ROUTER_INTERFACE, rif_id, 1, &attr);

        if (status != SAI_STATUS_SUCCESS)
        {
            SWSS_LOG_ERROR("failed to get rif type from rif %s",
                    sai_serialize_object_id(rif_id).c_str());
            continue;
        }

        switch (attr.value.s32)
        {
            case SAI_ROUTER_INTERFACE_TYPE_PORT:
            case SAI_ROUTER_INTERFACE_TYPE_SUB_PORT:
                break;

            default:
                continue;
        }

        attr.id = SAI_ROUTER_INTERFACE_ATTR_PORT_ID;

        status = get(SAI_OBJECT_TYPE_ROUTER_INTERFACE, rif_id, 1, &attr);

        if (status != SAI_STATUS_SUCCESS)
        {
            SWSS_LOG_ERROR("failed to get rif port id from rif %s",
                    sai_serialize_object_id(rif_id).c_str());
            continue;
        }

        if (attr.value.oid == lag_or_port_id)
        {
            return true;
        }
    }

    return false;
}

void SwitchStateBase::process_packet_for_fdb_event(
        _In_ sai_object_id_t portId,
        _In_ const std::string& name,
        _In_ const uint8_t *buffer,
        _In_ size_t size)
{
    SWSS_LOG_ENTER();

    // we would need hostif info here and maybe interface index, then we can
    // find host info from index

    uint32_t frametime = (uint32_t)time(NULL);

    /*
     * We add +2 in case if frame contains 1Q VLAN tag.
     */

    if (size < (sizeof(ethhdr) + 2))
    {
        SWSS_LOG_WARN("ethernet frame is too small: %zu", size);
        return;
    }

    const ethhdr *eh = (const ethhdr*)buffer;

    uint16_t proto = htons(eh->h_proto);

    uint16_t vlan_id = DEFAULT_VLAN_NUMBER;

    bool tagged = (proto == ETH_P_8021Q);

    if (tagged)
    {
        // this is tagged frame, get vlan id from frame

        uint16_t tci = htons(((const uint16_t*)&eh->h_proto)[1]); // tag is after h_proto field

        vlan_id = tci & 0xfff;

        if (vlan_id == 0xfff)
        {
            SWSS_LOG_WARN("invalid vlan id %u in ethernet frame on %s", vlan_id, name.c_str());
            return;
        }

        if (vlan_id == 0)
        {
            // priority packet, frame should be treated as non tagged
            tagged = false;
        }
    }

    if (tagged == false)
    {
        // untagged ethernet frame

        sai_attribute_t attr;

#ifdef SAI_LAG_ATTR_PORT_VLAN_ID

        sai_object_id_t lag_id;

        if (getLagFromPort(portid, lag_id))
        {
            // if port belongs to lag we need to get SAI_LAG_ATTR_PORT_VLAN_ID

            attr.id = SAI_LAG_ATTR_PORT_VLAN_ID

            sai_status_t status = get(SAI_OBJECT_TYPE_LAG, lag_id, 1, &attr);

            if (status != SAI_STATUS_SUCCESS)
            {
                SWSS_LOG_WARN("failed to get lag vlan id from lag %s",
                        sai_serialize_object_id(lag_id).c_str());
                return;
            }

            vlan_id = attr.value.u16;

            if (isLagOrPortRifBased(lag_id))
            {
                // this lag is router interface based, skip mac learning
                return;
            }
        }
        else
#endif
        {
            attr.id = SAI_PORT_ATTR_PORT_VLAN_ID;

            sai_status_t status = get(SAI_OBJECT_TYPE_PORT, portId, 1, &attr);

            if (status != SAI_STATUS_SUCCESS)
            {
                SWSS_LOG_WARN("failed to get port vlan id from port %s",
                        sai_serialize_object_id(portId).c_str());
                return;
            }

            // untagged port vlan (default is 1, but may change setting port attr)
            vlan_id = attr.value.u16;
        }
    }

    sai_object_id_t lag_id;
    if (getLagFromPort(portId, lag_id) && isLagOrPortRifBased(lag_id))
    {
        SWSS_LOG_DEBUG("lag %s is rif based, skip mac learning for port %s",
                sai_serialize_object_id(lag_id).c_str(),
                sai_serialize_object_id(portId).c_str());
        return;
    }

    if (isLagOrPortRifBased(portId))
    {
        SWSS_LOG_DEBUG("port %s is rif based, skip mac learning",
                sai_serialize_object_id(portId).c_str());
        return;
    }

    // we have vlan and mac address which is KEY, so just see if that is already defined

    FdbInfo fi;

    fi.setPortId((lag_id != SAI_NULL_OBJECT_ID) ? lag_id : portId);

    fi.setVlanId(vlan_id);

    memcpy(fi.m_fdbEntry.mac_address, eh->h_source, sizeof(sai_mac_t));

    std::set<FdbInfo>::iterator it = m_fdb_info_set.find(fi);

    if (it != m_fdb_info_set.end())
    {
        // this key was found, update timestamp
        // and since iterator is const we need to reinsert

        fi = *it;

        fi.setTimestamp(frametime);

        m_fdb_info_set.insert(fi);

        return;
    }

    // key was not found, get additional information

    fi.setTimestamp(frametime);

    fi.m_fdbEntry.switch_id = m_switch_id;

    findBridgeVlanForPortVlan(portId, vlan_id, fi.m_fdbEntry.bv_id, fi.m_bridgePortId);

    if (fi.getFdbEntry().bv_id == SAI_NULL_OBJECT_ID)
    {
        SWSS_LOG_WARN("skipping mac learn for %s, since BV_ID was not found for mac",
                sai_serialize_fdb_entry(fi.getFdbEntry()).c_str());

        // bridge was not found, skip mac learning
        return;
    }

    sai_attribute_t attr;

    attr.id = SAI_BRIDGE_PORT_ATTR_FDB_LEARNING_MODE;

    sai_status_t status = get(SAI_OBJECT_TYPE_BRIDGE_PORT, fi.getBridgePortId(), 1, &attr);

    if (status == SAI_STATUS_SUCCESS)
    {
        if (attr.value.s32 == SAI_BRIDGE_PORT_FDB_LEARNING_MODE_HW)
        {
            SWSS_LOG_INFO("inserting to fdb_info set: %s, vlan id: %d",
                    sai_serialize_fdb_entry(fi.getFdbEntry()).c_str(),
                    fi.getVlanId());

            m_fdb_info_set.insert(fi);

            processFdbInfo(fi, SAI_FDB_EVENT_LEARNED);
        }
        else if (attr.value.s32 == SAI_BRIDGE_PORT_FDB_LEARNING_MODE_DISABLE)
        {
            // do not learn, actually linux kernel will learn that MAC
        }
        else
        {
            SWSS_LOG_WARN("not supported SAI_BRIDGE_PORT_ATTR_FDB_LEARNING_MODE: %d, for %s",
                    attr.value.s32,
                    sai_serialize_fdb_entry(fi.getFdbEntry()).c_str());
        }
    }
    else
    {
        SWSS_LOG_ERROR("failed to get SAI_BRIDGE_PORT_ATTR_FDB_LEARNING_MODE for %s: %s",
                sai_serialize_object_id(fi.getBridgePortId()).c_str(),
                sai_serialize_status(status).c_str());
    }
}

void SwitchStateBase::send_fdb_event_notification(
        _In_ const sai_fdb_event_notification_data_t& data)
{
    SWSS_LOG_ENTER();

    auto meta = getMeta();

    if (meta)
    {
        meta->meta_sai_on_fdb_event(1, &data);
    }

    sai_attribute_t attr;

    attr.id = SAI_SWITCH_ATTR_FDB_EVENT_NOTIFY;

    sai_status_t status = get(SAI_OBJECT_TYPE_SWITCH, m_switch_id, 1, &attr);

    if (status != SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_ERROR("unable to get SAI_SWITCH_ATTR_FDB_EVENT_NOTIFY attribute for %s",
                sai_serialize_object_id(m_switch_id).c_str());

        return;
    }

    auto str = sai_serialize_fdb_event_ntf(1, &data);

    sai_switch_notifications_t sn = { };

    sn.on_fdb_event = (sai_fdb_event_notification_fn)attr.value.ptr;

    attr.id = SAI_PORT_ATTR_OPER_STATUS;

    SWSS_LOG_INFO("send event SAI_SWITCH_ATTR_FDB_EVENT_NOTIFY %s", str.c_str());

    auto ntf = std::make_shared<sairedis::NotificationFdbEvent>(str);

    auto payload = std::make_shared<EventPayloadNotification>(ntf, sn);

    m_switchConfig->m_eventQueue->enqueue(std::make_shared<Event>(EVENT_TYPE_NOTIFICATION, payload));
}
